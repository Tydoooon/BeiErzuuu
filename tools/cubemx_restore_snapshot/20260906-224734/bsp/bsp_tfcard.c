#include "bsp_tfcard.h"

#include "cmsis_os.h"

#define TFCARD_CMD_GO_IDLE_STATE        0U
#define TFCARD_CMD_SEND_IF_COND         8U
#define TFCARD_CMD_SEND_CSD             9U
#define TFCARD_CMD_SEND_CID             10U
#define TFCARD_CMD_SET_BLOCKLEN         16U
#define TFCARD_CMD_READ_SINGLE_BLOCK    17U
#define TFCARD_CMD_APP_CMD              55U
#define TFCARD_CMD_READ_OCR             58U
#define TFCARD_ACMD_SEND_OP_COND        41U

#define TFCARD_DUMMY_BYTE               0xFFU
#define TFCARD_START_BLOCK_TOKEN        0xFEU
#define TFCARD_R1_IDLE_STATE            0x01U
#define TFCARD_R1_ILLEGAL_COMMAND       0x04U
#define TFCARD_CMD_RETRY_COUNT          8U
#define TFCARD_INIT_TIMEOUT_MS          1000U
#define TFCARD_READY_TIMEOUT_MS         500U
#define TFCARD_READ_TOKEN_TIMEOUT_MS    200U
#define TFCARD_SPI_INIT_DELAY_CYCLES    120U
#define TFCARD_SPI_DATA_DELAY_CYCLES    2U

static bsp_tfcard_type_t tfcard_type = BSP_TFCARD_TYPE_UNKNOWN;
static bool tfcard_ready = false;
static uint32_t tfcard_spi_delay_cycles = TFCARD_SPI_INIT_DELAY_CYCLES;

static void tfcard_enable_gpio_clock(GPIO_TypeDef *port);
static void tfcard_gpio_init(void);
static void tfcard_delay_ms(uint32_t ms);
static void tfcard_delay_cycle(void);
static void tfcard_cs_low(void);
static void tfcard_cs_high(void);
static void tfcard_sck_low(void);
static void tfcard_sck_high(void);
static void tfcard_mosi_low(void);
static void tfcard_mosi_high(void);
static GPIO_PinState tfcard_miso_read(void);
static uint8_t tfcard_spi_rw(uint8_t tx);
static void tfcard_send_clocks(uint32_t bytes);
static uint8_t tfcard_wait_ready(uint32_t timeout_ms);
static uint8_t tfcard_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc);
static uint8_t tfcard_send_acmd(uint8_t acmd, uint32_t arg);
static bsp_tfcard_status_t tfcard_read_data(uint8_t *data, size_t len);
static uint32_t tfcard_block_to_addr(uint32_t block);

bsp_tfcard_status_t BSP_TFCARD_Init(void)
{
    uint8_t r1;
    uint8_t ocr[4];
    uint32_t start;

    tfcard_ready = false;
    tfcard_type = BSP_TFCARD_TYPE_UNKNOWN;
    tfcard_spi_delay_cycles = TFCARD_SPI_INIT_DELAY_CYCLES;

    tfcard_gpio_init();
    tfcard_cs_high();
    tfcard_send_clocks(10U);

    r1 = tfcard_send_cmd(TFCARD_CMD_GO_IDLE_STATE, 0U, 0x95U);
    if (r1 != TFCARD_R1_IDLE_STATE) {
        tfcard_cs_high();
        tfcard_spi_rw(TFCARD_DUMMY_BYTE);
        return BSP_TFCARD_NOT_FOUND;
    }

    r1 = tfcard_send_cmd(TFCARD_CMD_SEND_IF_COND, 0x000001AAU, 0x87U);
    if (r1 == TFCARD_R1_IDLE_STATE) {
        for (uint32_t i = 0; i < sizeof(ocr); i++) {
            ocr[i] = tfcard_spi_rw(TFCARD_DUMMY_BYTE);
        }

        if (ocr[2] != 0x01U || ocr[3] != 0xAAU) {
            tfcard_cs_high();
            tfcard_spi_rw(TFCARD_DUMMY_BYTE);
            return BSP_TFCARD_UNSUPPORTED;
        }

        start = HAL_GetTick();
        do {
            r1 = tfcard_send_acmd(TFCARD_ACMD_SEND_OP_COND, 0x40000000U);
            if (r1 == 0U) {
                break;
            }
            if ((HAL_GetTick() - start) >= TFCARD_INIT_TIMEOUT_MS) {
                tfcard_cs_high();
                tfcard_spi_rw(TFCARD_DUMMY_BYTE);
                return BSP_TFCARD_TIMEOUT;
            }
            tfcard_delay_ms(1U);
        } while (true);

        r1 = tfcard_send_cmd(TFCARD_CMD_READ_OCR, 0U, 0xFFU);
        if (r1 != 0U) {
            tfcard_cs_high();
            tfcard_spi_rw(TFCARD_DUMMY_BYTE);
            return BSP_TFCARD_ERROR;
        }

        for (uint32_t i = 0; i < sizeof(ocr); i++) {
            ocr[i] = tfcard_spi_rw(TFCARD_DUMMY_BYTE);
        }
        tfcard_type = (ocr[0] & 0x40U) != 0U ? BSP_TFCARD_TYPE_SDHC_SDXC : BSP_TFCARD_TYPE_SDSC;
    } else if ((r1 & TFCARD_R1_ILLEGAL_COMMAND) != 0U) {
        start = HAL_GetTick();
        do {
            r1 = tfcard_send_acmd(TFCARD_ACMD_SEND_OP_COND, 0U);
            if (r1 == 0U) {
                break;
            }
            if ((HAL_GetTick() - start) >= TFCARD_INIT_TIMEOUT_MS) {
                tfcard_cs_high();
                tfcard_spi_rw(TFCARD_DUMMY_BYTE);
                return BSP_TFCARD_TIMEOUT;
            }
            tfcard_delay_ms(1U);
        } while (true);

        tfcard_type = BSP_TFCARD_TYPE_SDSC;
    } else {
        tfcard_cs_high();
        tfcard_spi_rw(TFCARD_DUMMY_BYTE);
        return BSP_TFCARD_ERROR;
    }

    if (tfcard_type == BSP_TFCARD_TYPE_SDSC) {
        r1 = tfcard_send_cmd(TFCARD_CMD_SET_BLOCKLEN, TFCARD_BLOCK_SIZE, 0xFFU);
        if (r1 != 0U) {
            tfcard_cs_high();
            tfcard_spi_rw(TFCARD_DUMMY_BYTE);
            return BSP_TFCARD_ERROR;
        }
    }

    tfcard_cs_high();
    tfcard_spi_rw(TFCARD_DUMMY_BYTE);
    tfcard_spi_delay_cycles = TFCARD_SPI_DATA_DELAY_CYCLES;
    tfcard_ready = true;
    return BSP_TFCARD_OK;
}

bsp_tfcard_status_t BSP_TFCARD_ReadBlock(uint32_t block, uint8_t data[TFCARD_BLOCK_SIZE])
{
    uint8_t r1;
    bsp_tfcard_status_t ret;

    if (data == NULL) {
        return BSP_TFCARD_INVALID_PARAM;
    }

    if (!tfcard_ready) {
        return BSP_TFCARD_NOT_FOUND;
    }

    r1 = tfcard_send_cmd(TFCARD_CMD_READ_SINGLE_BLOCK, tfcard_block_to_addr(block), 0xFFU);
    if (r1 != 0U) {
        tfcard_cs_high();
        tfcard_spi_rw(TFCARD_DUMMY_BYTE);
        return BSP_TFCARD_ERROR;
    }

    ret = tfcard_read_data(data, TFCARD_BLOCK_SIZE);
    tfcard_cs_high();
    tfcard_spi_rw(TFCARD_DUMMY_BYTE);

    return ret;
}

bsp_tfcard_status_t BSP_TFCARD_ReadBlocks(uint32_t start_block, uint8_t *data, uint32_t block_count)
{
    bsp_tfcard_status_t ret;

    if (block_count == 0U) {
        return BSP_TFCARD_OK;
    }

    if (data == NULL) {
        return BSP_TFCARD_INVALID_PARAM;
    }

    for (uint32_t i = 0; i < block_count; i++) {
        ret = BSP_TFCARD_ReadBlock(start_block + i, &data[i * TFCARD_BLOCK_SIZE]);
        if (ret != BSP_TFCARD_OK) {
            return ret;
        }
    }

    return BSP_TFCARD_OK;
}

bsp_tfcard_status_t BSP_TFCARD_ReadCSD(uint8_t csd[16])
{
    uint8_t r1;
    bsp_tfcard_status_t ret;

    if (csd == NULL) {
        return BSP_TFCARD_INVALID_PARAM;
    }

    if (!tfcard_ready) {
        return BSP_TFCARD_NOT_FOUND;
    }

    r1 = tfcard_send_cmd(TFCARD_CMD_SEND_CSD, 0U, 0xFFU);
    if (r1 != 0U) {
        tfcard_cs_high();
        tfcard_spi_rw(TFCARD_DUMMY_BYTE);
        return BSP_TFCARD_ERROR;
    }

    ret = tfcard_read_data(csd, 16U);
    tfcard_cs_high();
    tfcard_spi_rw(TFCARD_DUMMY_BYTE);

    return ret;
}

bsp_tfcard_status_t BSP_TFCARD_ReadCID(uint8_t cid[16])
{
    uint8_t r1;
    bsp_tfcard_status_t ret;

    if (cid == NULL) {
        return BSP_TFCARD_INVALID_PARAM;
    }

    if (!tfcard_ready) {
        return BSP_TFCARD_NOT_FOUND;
    }

    r1 = tfcard_send_cmd(TFCARD_CMD_SEND_CID, 0U, 0xFFU);
    if (r1 != 0U) {
        tfcard_cs_high();
        tfcard_spi_rw(TFCARD_DUMMY_BYTE);
        return BSP_TFCARD_ERROR;
    }

    ret = tfcard_read_data(cid, 16U);
    tfcard_cs_high();
    tfcard_spi_rw(TFCARD_DUMMY_BYTE);

    return ret;
}

bsp_tfcard_type_t BSP_TFCARD_GetType(void)
{
    return tfcard_type;
}

bool BSP_TFCARD_IsReady(void)
{
    return tfcard_ready;
}

static void tfcard_enable_gpio_clock(GPIO_TypeDef *port)
{
    if (port == GPIOA) {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    } else if (port == GPIOB) {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    } else if (port == GPIOC) {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    } else if (port == GPIOD) {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    } else if (port == GPIOE) {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    } else if (port == GPIOH) {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    }
}

static void tfcard_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    tfcard_enable_gpio_clock(TF_CS_GPIO_Port);
    tfcard_enable_gpio_clock(TF_SCK_GPIO_Port);
    tfcard_enable_gpio_clock(TF_MOSI_GPIO_Port);
    tfcard_enable_gpio_clock(TF_MISO_GPIO_Port);

    HAL_GPIO_WritePin(TF_CS_GPIO_Port, TF_CS_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(TF_SCK_GPIO_Port, TF_SCK_Pin, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(TF_MOSI_GPIO_Port, TF_MOSI_Pin, GPIO_PIN_SET);

    GPIO_InitStruct.Pin = TF_CS_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(TF_CS_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = TF_SCK_Pin;
    HAL_GPIO_Init(TF_SCK_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = TF_MOSI_Pin;
    HAL_GPIO_Init(TF_MOSI_GPIO_Port, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = TF_MISO_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(TF_MISO_GPIO_Port, &GPIO_InitStruct);
}

static void tfcard_delay_ms(uint32_t ms)
{
    if (osKernelGetState() == osKernelRunning) {
        (void)osDelay(ms);
    } else {
        HAL_Delay(ms);
    }
}

static void tfcard_delay_cycle(void)
{
    for (uint32_t i = 0; i < tfcard_spi_delay_cycles; i++) {
        __NOP();
    }
}

static void tfcard_cs_low(void)
{
    TF_CS_GPIO_Port->BSRR = (uint32_t)TF_CS_Pin << 16;
}

static void tfcard_cs_high(void)
{
    TF_CS_GPIO_Port->BSRR = TF_CS_Pin;
}

static void tfcard_sck_low(void)
{
    TF_SCK_GPIO_Port->BSRR = (uint32_t)TF_SCK_Pin << 16;
}

static void tfcard_sck_high(void)
{
    TF_SCK_GPIO_Port->BSRR = TF_SCK_Pin;
}

static void tfcard_mosi_low(void)
{
    TF_MOSI_GPIO_Port->BSRR = (uint32_t)TF_MOSI_Pin << 16;
}

static void tfcard_mosi_high(void)
{
    TF_MOSI_GPIO_Port->BSRR = TF_MOSI_Pin;
}

static GPIO_PinState tfcard_miso_read(void)
{
    return (TF_MISO_GPIO_Port->IDR & TF_MISO_Pin) != 0U ? GPIO_PIN_SET : GPIO_PIN_RESET;
}

static uint8_t tfcard_spi_rw(uint8_t tx)
{
    uint8_t rx = 0;

    for (uint8_t mask = 0x80U; mask != 0U; mask >>= 1) {
        if ((tx & mask) != 0U) {
            tfcard_mosi_high();
        } else {
            tfcard_mosi_low();
        }

        tfcard_delay_cycle();
        tfcard_sck_high();
        tfcard_delay_cycle();

        if (tfcard_miso_read() == GPIO_PIN_SET) {
            rx |= mask;
        }

        tfcard_sck_low();
        tfcard_delay_cycle();
    }

    return rx;
}

static void tfcard_send_clocks(uint32_t bytes)
{
    while (bytes-- > 0U) {
        (void)tfcard_spi_rw(TFCARD_DUMMY_BYTE);
    }
}

static uint8_t tfcard_wait_ready(uint32_t timeout_ms)
{
    uint32_t start = HAL_GetTick();
    uint8_t value;

    do {
        value = tfcard_spi_rw(TFCARD_DUMMY_BYTE);
        if (value == TFCARD_DUMMY_BYTE) {
            return value;
        }
    } while ((HAL_GetTick() - start) < timeout_ms);

    return value;
}

static uint8_t tfcard_send_cmd(uint8_t cmd, uint32_t arg, uint8_t crc)
{
    uint8_t r1 = 0xFFU;

    tfcard_cs_high();
    tfcard_spi_rw(TFCARD_DUMMY_BYTE);
    tfcard_cs_low();

    if (tfcard_wait_ready(TFCARD_READY_TIMEOUT_MS) != TFCARD_DUMMY_BYTE) {
        return 0xFFU;
    }

    tfcard_spi_rw(0x40U | cmd);
    tfcard_spi_rw((uint8_t)(arg >> 24));
    tfcard_spi_rw((uint8_t)(arg >> 16));
    tfcard_spi_rw((uint8_t)(arg >> 8));
    tfcard_spi_rw((uint8_t)arg);
    tfcard_spi_rw(crc);

    for (uint32_t i = 0; i < TFCARD_CMD_RETRY_COUNT; i++) {
        r1 = tfcard_spi_rw(TFCARD_DUMMY_BYTE);
        if ((r1 & 0x80U) == 0U) {
            break;
        }
    }

    return r1;
}

static uint8_t tfcard_send_acmd(uint8_t acmd, uint32_t arg)
{
    uint8_t r1;

    r1 = tfcard_send_cmd(TFCARD_CMD_APP_CMD, 0U, 0xFFU);
    if (r1 > TFCARD_R1_IDLE_STATE) {
        return r1;
    }

    return tfcard_send_cmd(acmd, arg, 0xFFU);
}

static bsp_tfcard_status_t tfcard_read_data(uint8_t *data, size_t len)
{
    uint32_t start = HAL_GetTick();
    uint8_t token;

    do {
        token = tfcard_spi_rw(TFCARD_DUMMY_BYTE);
        if (token == TFCARD_START_BLOCK_TOKEN) {
            break;
        }
        if ((HAL_GetTick() - start) >= TFCARD_READ_TOKEN_TIMEOUT_MS) {
            return BSP_TFCARD_TIMEOUT;
        }
    } while (true);

    for (size_t i = 0; i < len; i++) {
        data[i] = tfcard_spi_rw(TFCARD_DUMMY_BYTE);
    }

    (void)tfcard_spi_rw(TFCARD_DUMMY_BYTE);
    (void)tfcard_spi_rw(TFCARD_DUMMY_BYTE);

    return BSP_TFCARD_OK;
}

static uint32_t tfcard_block_to_addr(uint32_t block)
{
    if (tfcard_type == BSP_TFCARD_TYPE_SDHC_SDXC) {
        return block;
    }

    return block * TFCARD_BLOCK_SIZE;
}
