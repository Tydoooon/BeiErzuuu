#include "bsp_w25q128.h"

#include "cmsis_os.h"
#include "spi.h"

#define W25Q_CMD_WRITE_ENABLE        0x06U
#define W25Q_CMD_WRITE_DISABLE       0x04U
#define W25Q_CMD_READ_STATUS1        0x05U
#define W25Q_CMD_READ_STATUS2        0x35U
#define W25Q_CMD_PAGE_PROGRAM        0x02U
#define W25Q_CMD_READ_DATA           0x03U
#define W25Q_CMD_SECTOR_ERASE_4K     0x20U
#define W25Q_CMD_BLOCK_ERASE_64K     0xD8U
#define W25Q_CMD_CHIP_ERASE          0xC7U
#define W25Q_CMD_POWER_DOWN          0xB9U
#define W25Q_CMD_RELEASE_POWER_DOWN  0xABU
#define W25Q_CMD_JEDEC_ID            0x9FU

#define W25Q_STATUS_BUSY             0x01U
#define W25Q_STATUS_WEL              0x02U

#define W25Q_SPI_TIMEOUT_MS          100U
#define W25Q_SPI_MAX_CHUNK           0xFFFFU
#define W25Q_READY_TIMEOUT_MS        1000U
#define W25Q_PAGE_PROGRAM_TIMEOUT_MS 100U
#define W25Q_SECTOR_ERASE_TIMEOUT_MS 1000U
#define W25Q_BLOCK_ERASE_TIMEOUT_MS  3000U
#define W25Q_CHIP_ERASE_TIMEOUT_MS   120000U

static void w25q128_cs_init(void);
static void w25q128_cs_low(void);
static void w25q128_cs_high(void);
static void w25q128_delay_ms(uint32_t ms);
static bool w25q128_addr_valid(uint32_t addr, size_t len);
static bsp_w25q128_status_t w25q128_hal_status(HAL_StatusTypeDef status);
static bsp_w25q128_status_t w25q128_cmd(uint8_t cmd);
static bsp_w25q128_status_t w25q128_write_enable(void);
static bsp_w25q128_status_t w25q128_check_write_enable(void);
static void w25q128_make_addr_cmd(uint8_t cmd, uint32_t addr, uint8_t out[4]);

bsp_w25q128_status_t BSP_W25Q128_Init(void)
{
    uint32_t id = 0;

    w25q128_cs_init();
    w25q128_cs_high();
    w25q128_delay_ms(2);

    if (BSP_W25Q128_ReleasePowerDown() != BSP_W25Q128_OK) {
        return BSP_W25Q128_ERROR;
    }

    if (BSP_W25Q128_ReadJedecID(&id) != BSP_W25Q128_OK) {
        return BSP_W25Q128_ERROR;
    }

    if (id == 0U || id == 0xFFFFFFU || ((id & 0xFFU) != W25Q128_JEDEC_CAPACITY_ID)) {
        return BSP_W25Q128_NOT_FOUND;
    }

    return BSP_W25Q128_OK;
}

bsp_w25q128_status_t BSP_W25Q128_ReadJedecID(uint32_t *jedec_id)
{
    uint8_t cmd = W25Q_CMD_JEDEC_ID;
    uint8_t id[3] = {0};
    bsp_w25q128_status_t ret;

    if (jedec_id == NULL) {
        return BSP_W25Q128_INVALID_PARAM;
    }

    w25q128_cs_low();
    ret = w25q128_hal_status(HAL_SPI_Transmit(&hspi2, &cmd, 1, W25Q_SPI_TIMEOUT_MS));
    if (ret == BSP_W25Q128_OK) {
        ret = w25q128_hal_status(HAL_SPI_Receive(&hspi2, id, sizeof(id), W25Q_SPI_TIMEOUT_MS));
    }
    w25q128_cs_high();

    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    *jedec_id = ((uint32_t)id[0] << 16) | ((uint32_t)id[1] << 8) | id[2];
    return BSP_W25Q128_OK;
}

bsp_w25q128_status_t BSP_W25Q128_ReadStatus1(uint8_t *status)
{
    uint8_t cmd = W25Q_CMD_READ_STATUS1;
    bsp_w25q128_status_t ret;

    if (status == NULL) {
        return BSP_W25Q128_INVALID_PARAM;
    }

    w25q128_cs_low();
    ret = w25q128_hal_status(HAL_SPI_Transmit(&hspi2, &cmd, 1, W25Q_SPI_TIMEOUT_MS));
    if (ret == BSP_W25Q128_OK) {
        ret = w25q128_hal_status(HAL_SPI_Receive(&hspi2, status, 1, W25Q_SPI_TIMEOUT_MS));
    }
    w25q128_cs_high();

    return ret;
}

bsp_w25q128_status_t BSP_W25Q128_ReadStatus2(uint8_t *status)
{
    uint8_t cmd = W25Q_CMD_READ_STATUS2;
    bsp_w25q128_status_t ret;

    if (status == NULL) {
        return BSP_W25Q128_INVALID_PARAM;
    }

    w25q128_cs_low();
    ret = w25q128_hal_status(HAL_SPI_Transmit(&hspi2, &cmd, 1, W25Q_SPI_TIMEOUT_MS));
    if (ret == BSP_W25Q128_OK) {
        ret = w25q128_hal_status(HAL_SPI_Receive(&hspi2, status, 1, W25Q_SPI_TIMEOUT_MS));
    }
    w25q128_cs_high();

    return ret;
}

bool BSP_W25Q128_IsBusy(void)
{
    uint8_t status = 0xFFU;

    if (BSP_W25Q128_ReadStatus1(&status) != BSP_W25Q128_OK) {
        return true;
    }

    return (status & W25Q_STATUS_BUSY) != 0U;
}

bsp_w25q128_status_t BSP_W25Q128_WaitReady(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();

    while (BSP_W25Q128_IsBusy()) {
        if ((HAL_GetTick() - start) >= timeout_ms) {
            return BSP_W25Q128_TIMEOUT;
        }
        w25q128_delay_ms(1);
    }

    return BSP_W25Q128_OK;
}

bsp_w25q128_status_t BSP_W25Q128_Read(uint32_t addr, uint8_t *data, size_t len)
{
    uint8_t cmd[4];
    bsp_w25q128_status_t ret;

    if (len == 0U) {
        return BSP_W25Q128_OK;
    }

    if (data == NULL || !w25q128_addr_valid(addr, len)) {
        return BSP_W25Q128_INVALID_PARAM;
    }

    ret = BSP_W25Q128_WaitReady(W25Q_READY_TIMEOUT_MS);
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    w25q128_make_addr_cmd(W25Q_CMD_READ_DATA, addr, cmd);

    w25q128_cs_low();
    ret = w25q128_hal_status(HAL_SPI_Transmit(&hspi2, cmd, sizeof(cmd), W25Q_SPI_TIMEOUT_MS));
    while (ret == BSP_W25Q128_OK && len > 0U) {
        size_t chunk = len > W25Q_SPI_MAX_CHUNK ? W25Q_SPI_MAX_CHUNK : len;

        ret = w25q128_hal_status(HAL_SPI_Receive(&hspi2, data, (uint16_t)chunk, W25Q_SPI_TIMEOUT_MS));
        data += chunk;
        len -= chunk;
    }
    w25q128_cs_high();

    return ret;
}

bsp_w25q128_status_t BSP_W25Q128_PageProgram(uint32_t addr, const uint8_t *data, size_t len)
{
    uint8_t cmd[4];
    bsp_w25q128_status_t ret;

    if (len == 0U) {
        return BSP_W25Q128_OK;
    }

    if (data == NULL || len > W25Q128_PAGE_SIZE || !w25q128_addr_valid(addr, len)) {
        return BSP_W25Q128_INVALID_PARAM;
    }

    if ((addr / W25Q128_PAGE_SIZE) != ((addr + len - 1U) / W25Q128_PAGE_SIZE)) {
        return BSP_W25Q128_INVALID_PARAM;
    }

    ret = BSP_W25Q128_WaitReady(W25Q_READY_TIMEOUT_MS);
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    ret = w25q128_write_enable();
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    ret = w25q128_check_write_enable();
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    w25q128_make_addr_cmd(W25Q_CMD_PAGE_PROGRAM, addr, cmd);

    w25q128_cs_low();
    ret = w25q128_hal_status(HAL_SPI_Transmit(&hspi2, cmd, sizeof(cmd), W25Q_SPI_TIMEOUT_MS));
    if (ret == BSP_W25Q128_OK) {
        ret = w25q128_hal_status(HAL_SPI_Transmit(&hspi2, (uint8_t *)data, (uint16_t)len, W25Q_SPI_TIMEOUT_MS));
    }
    w25q128_cs_high();

    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    return BSP_W25Q128_WaitReady(W25Q_PAGE_PROGRAM_TIMEOUT_MS);
}

bsp_w25q128_status_t BSP_W25Q128_Write(uint32_t addr, const uint8_t *data, size_t len)
{
    bsp_w25q128_status_t ret;

    if (len == 0U) {
        return BSP_W25Q128_OK;
    }

    if (data == NULL || !w25q128_addr_valid(addr, len)) {
        return BSP_W25Q128_INVALID_PARAM;
    }

    while (len > 0U) {
        size_t page_remain = W25Q128_PAGE_SIZE - (addr % W25Q128_PAGE_SIZE);
        size_t chunk = len < page_remain ? len : page_remain;

        ret = BSP_W25Q128_PageProgram(addr, data, chunk);
        if (ret != BSP_W25Q128_OK) {
            return ret;
        }

        addr += (uint32_t)chunk;
        data += chunk;
        len -= chunk;
    }

    return BSP_W25Q128_OK;
}

bsp_w25q128_status_t BSP_W25Q128_EraseSector(uint32_t addr)
{
    uint8_t cmd[4];
    bsp_w25q128_status_t ret;

    if (addr >= W25Q128_SIZE_BYTES) {
        return BSP_W25Q128_INVALID_PARAM;
    }

    ret = BSP_W25Q128_WaitReady(W25Q_READY_TIMEOUT_MS);
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    ret = w25q128_write_enable();
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    ret = w25q128_check_write_enable();
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    w25q128_make_addr_cmd(W25Q_CMD_SECTOR_ERASE_4K, addr, cmd);

    w25q128_cs_low();
    ret = w25q128_hal_status(HAL_SPI_Transmit(&hspi2, cmd, sizeof(cmd), W25Q_SPI_TIMEOUT_MS));
    w25q128_cs_high();

    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    return BSP_W25Q128_WaitReady(W25Q_SECTOR_ERASE_TIMEOUT_MS);
}

bsp_w25q128_status_t BSP_W25Q128_EraseBlock64K(uint32_t addr)
{
    uint8_t cmd[4];
    bsp_w25q128_status_t ret;

    if (addr >= W25Q128_SIZE_BYTES) {
        return BSP_W25Q128_INVALID_PARAM;
    }

    ret = BSP_W25Q128_WaitReady(W25Q_READY_TIMEOUT_MS);
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    ret = w25q128_write_enable();
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    ret = w25q128_check_write_enable();
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    w25q128_make_addr_cmd(W25Q_CMD_BLOCK_ERASE_64K, addr, cmd);

    w25q128_cs_low();
    ret = w25q128_hal_status(HAL_SPI_Transmit(&hspi2, cmd, sizeof(cmd), W25Q_SPI_TIMEOUT_MS));
    w25q128_cs_high();

    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    return BSP_W25Q128_WaitReady(W25Q_BLOCK_ERASE_TIMEOUT_MS);
}

bsp_w25q128_status_t BSP_W25Q128_EraseChip(void)
{
    bsp_w25q128_status_t ret;

    ret = BSP_W25Q128_WaitReady(W25Q_READY_TIMEOUT_MS);
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    ret = w25q128_write_enable();
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    ret = w25q128_check_write_enable();
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    ret = w25q128_cmd(W25Q_CMD_CHIP_ERASE);
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    return BSP_W25Q128_WaitReady(W25Q_CHIP_ERASE_TIMEOUT_MS);
}

bsp_w25q128_status_t BSP_W25Q128_PowerDown(void)
{
    return w25q128_cmd(W25Q_CMD_POWER_DOWN);
}

bsp_w25q128_status_t BSP_W25Q128_ReleasePowerDown(void)
{
    bsp_w25q128_status_t ret = w25q128_cmd(W25Q_CMD_RELEASE_POWER_DOWN);
    w25q128_delay_ms(1);
    return ret;
}

static void w25q128_cs_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    if (W25Q128_CS_GPIO_Port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    } else if (W25Q128_CS_GPIO_Port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    } else if (W25Q128_CS_GPIO_Port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    } else if (W25Q128_CS_GPIO_Port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    } else if (W25Q128_CS_GPIO_Port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    } else if (W25Q128_CS_GPIO_Port == GPIOH) {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    }

    HAL_GPIO_WritePin(W25Q128_CS_GPIO_Port, W25Q128_CS_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = W25Q128_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(W25Q128_CS_GPIO_Port, &GPIO_InitStruct);
}

static void w25q128_cs_low(void)
{
    HAL_GPIO_WritePin(W25Q128_CS_GPIO_Port, W25Q128_CS_Pin, GPIO_PIN_RESET);
}

static void w25q128_cs_high(void)
{
    HAL_GPIO_WritePin(W25Q128_CS_GPIO_Port, W25Q128_CS_Pin, GPIO_PIN_SET);
}

static void w25q128_delay_ms(uint32_t ms)
{
    if (osKernelGetState() == osKernelRunning) {
        (void)osDelay(ms);
    } else {
        HAL_Delay(ms);
    }
}

static bool w25q128_addr_valid(uint32_t addr, size_t len)
{
    if (addr >= W25Q128_SIZE_BYTES) {
        return false;
    }

    return len <= (size_t)(W25Q128_SIZE_BYTES - addr);
}

static bsp_w25q128_status_t w25q128_hal_status(HAL_StatusTypeDef status)
{
    if (status == HAL_OK) {
        return BSP_W25Q128_OK;
    }

    if (status == HAL_TIMEOUT) {
        return BSP_W25Q128_TIMEOUT;
    }

    return BSP_W25Q128_ERROR;
}

static bsp_w25q128_status_t w25q128_cmd(uint8_t cmd)
{
    bsp_w25q128_status_t ret;

    w25q128_cs_low();
    ret = w25q128_hal_status(HAL_SPI_Transmit(&hspi2, &cmd, 1, W25Q_SPI_TIMEOUT_MS));
    w25q128_cs_high();

    return ret;
}

static bsp_w25q128_status_t w25q128_write_enable(void)
{
    return w25q128_cmd(W25Q_CMD_WRITE_ENABLE);
}

static bsp_w25q128_status_t w25q128_check_write_enable(void)
{
    uint8_t status = 0;
    bsp_w25q128_status_t ret = BSP_W25Q128_ReadStatus1(&status);

    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    return (status & W25Q_STATUS_WEL) != 0U ? BSP_W25Q128_OK : BSP_W25Q128_ERROR;
}

static void w25q128_make_addr_cmd(uint8_t cmd, uint32_t addr, uint8_t out[4])
{
    out[0] = cmd;
    out[1] = (uint8_t)(addr >> 16);
    out[2] = (uint8_t)(addr >> 8);
    out[3] = (uint8_t)addr;
}
