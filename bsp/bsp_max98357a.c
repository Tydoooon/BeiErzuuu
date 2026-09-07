#include "bsp_max98357a.h"

#include "cmsis_os.h"
#include "i2s.h"

#define MAX98357A_SD_PIN         GPIO_PIN_5
#define MAX98357A_SD_PORT        GPIOE
#define MAX98357A_DEFAULT_RATE   I2S_AUDIOFREQ_44K

static void max98357a_enable_gpio_clock(GPIO_TypeDef *port);
static void max98357a_sd_init(void);
static void max98357a_sd_set(bool enable);
static bsp_max98357a_status_t max98357a_reinit_i2s(uint32_t sample_rate);
static bsp_max98357a_status_t max98357a_hal_status(HAL_StatusTypeDef status);
static void max98357a_dma_lowlevel_init(void);

/* I2S3_TX DMA 流（F407: DMA1 Stream5 Channel0 = SPI3_TX） */
static DMA_HandleTypeDef s_hdma_i2s3_tx;
static bool s_dma_inited = false;

bsp_max98357a_status_t BSP_MAX98357A_Init(void)
{
    max98357a_sd_init();
    max98357a_sd_set(false);
    return max98357a_reinit_i2s(MAX98357A_DEFAULT_RATE);
}

bsp_max98357a_status_t BSP_MAX98357A_Enable(bool enable)
{
    max98357a_sd_set(enable);
    return BSP_MAX98357A_OK;
}

bsp_max98357a_status_t BSP_MAX98357A_GetEnableState(bool *enabled)
{
    if (enabled == NULL) {
        return BSP_MAX98357A_INVALID_PARAM;
    }

    *enabled = (HAL_GPIO_ReadPin(MAX98357A_SD_PORT, MAX98357A_SD_PIN) == GPIO_PIN_SET);
    return BSP_MAX98357A_OK;
}

bsp_max98357a_status_t BSP_MAX98357A_SetSampleRate(uint32_t sample_rate)
{
    if (sample_rate < I2S_AUDIOFREQ_8K || sample_rate > I2S_AUDIOFREQ_192K) {
        return BSP_MAX98357A_INVALID_PARAM;
    }

    return max98357a_reinit_i2s(sample_rate);
}

bsp_max98357a_status_t BSP_MAX98357A_TransmitWords(const uint16_t *data, uint16_t word_count, uint32_t timeout_ms)
{
    if (data == NULL || word_count == 0U) {
        return BSP_MAX98357A_INVALID_PARAM;
    }

    return max98357a_hal_status(HAL_I2S_Transmit(&hi2s3, (uint16_t *)data, word_count, timeout_ms));
}

bsp_max98357a_status_t BSP_MAX98357A_TransmitSilence(uint16_t word_count, uint32_t timeout_ms)
{
    static const uint16_t silence[32] = {0};
    bsp_max98357a_status_t ret = BSP_MAX98357A_OK;

    while (word_count > 0U) {
        uint16_t chunk = word_count > (uint16_t)(sizeof(silence) / sizeof(silence[0]))
                           ? (uint16_t)(sizeof(silence) / sizeof(silence[0]))
                           : word_count;
        ret = BSP_MAX98357A_TransmitWords(silence, chunk, timeout_ms);
        if (ret != BSP_MAX98357A_OK) {
            return ret;
        }
        word_count = (uint16_t)(word_count - chunk);
    }

    return ret;
}

static void max98357a_enable_gpio_clock(GPIO_TypeDef *port)
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

static void max98357a_sd_init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    max98357a_enable_gpio_clock(MAX98357A_SD_PORT);
    HAL_GPIO_WritePin(MAX98357A_SD_PORT, MAX98357A_SD_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = MAX98357A_SD_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(MAX98357A_SD_PORT, &GPIO_InitStruct);
}

static void max98357a_sd_set(bool enable)
{
    HAL_GPIO_WritePin(MAX98357A_SD_PORT, MAX98357A_SD_PIN,
                      enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

static bsp_max98357a_status_t max98357a_reinit_i2s(uint32_t sample_rate)
{
    hi2s3.Init.AudioFreq = sample_rate;
    if (HAL_I2S_DeInit(&hi2s3) != HAL_OK) {
        return BSP_MAX98357A_ERROR;
    }

    if (HAL_I2S_Init(&hi2s3) != HAL_OK) {
        return BSP_MAX98357A_ERROR;
    }

    return BSP_MAX98357A_OK;
}

static bsp_max98357a_status_t max98357a_hal_status(HAL_StatusTypeDef status)
{
    if (status == HAL_OK) {
        return BSP_MAX98357A_OK;
    }
    if (status == HAL_TIMEOUT) {
        return BSP_MAX98357A_TIMEOUT;
    }
    return BSP_MAX98357A_ERROR;
}

static void max98357a_dma_lowlevel_init(void)
{
    if (s_dma_inited) {
        return;
    }
    s_dma_inited = true;

    __HAL_RCC_DMA1_CLK_ENABLE();

    s_hdma_i2s3_tx.Instance = DMA1_Stream5;
    s_hdma_i2s3_tx.Init.Channel = DMA_CHANNEL_0;              /* SPI3_TX/I2S3_TX */
    s_hdma_i2s3_tx.Init.Direction = DMA_MEMORY_TO_PERIPH;
    s_hdma_i2s3_tx.Init.PeriphInc = DMA_PINC_DISABLE;
    s_hdma_i2s3_tx.Init.MemInc = DMA_MINC_ENABLE;
    s_hdma_i2s3_tx.Init.PeriphDataAlignment = DMA_PDATAALIGN_HALFWORD;
    s_hdma_i2s3_tx.Init.MemDataAlignment = DMA_MDATAALIGN_HALFWORD;
    s_hdma_i2s3_tx.Init.Mode = DMA_NORMAL;                    /* 每片传完触发 TC，由回调续传下一片 */
    s_hdma_i2s3_tx.Init.Priority = DMA_PRIORITY_HIGH;
    s_hdma_i2s3_tx.Init.FIFOMode = DMA_FIFOMODE_DISABLE;
    if (HAL_DMA_Init(&s_hdma_i2s3_tx) != HAL_OK) {
        s_dma_inited = false;   /* 允许下次重试 */
        return;
    }

    __HAL_LINKDMA(&hi2s3, hdmatx, s_hdma_i2s3_tx);   /* 宏内部会自动 &s_hdma_i2s3_tx */

    /* 必须 <= configMAX_SYSCALL_INTERRUPT_PRIORITY(5)，ISR 里要调 FreeRTOS API */
    HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 5U, 0U);
    HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);
}

/**
  * @brief 启动一片 DMA 发送（可被发送完成中断重复调用实现连续续传）
  */
bsp_max98357a_status_t BSP_MAX98357A_TxStartDma(const uint16_t *data, uint16_t word_count)
{
    if (data == NULL || word_count == 0U) {
        return BSP_MAX98357A_INVALID_PARAM;
    }

    if (!s_dma_inited) {
        max98357a_dma_lowlevel_init();
    }
    if (!s_dma_inited) {
        return BSP_MAX98357A_ERROR;
    }

    return max98357a_hal_status(HAL_I2S_Transmit_DMA(&hi2s3, (uint16_t *)data, word_count));
}

/**
  * @brief 停止流式发送（仅在任务上下文调用）
  */
bsp_max98357a_status_t BSP_MAX98357A_TxStopDma(void)
{
    if (!s_dma_inited) {
        return BSP_MAX98357A_OK;
    }

    /* 先断 I2S 与 TX DMA 请求，避免停止后 I2S 主时钟空转 */
    CLEAR_BIT(hi2s3.Instance->CR2, SPI_CR2_TXDMAEN);
    __HAL_I2S_DISABLE(&hi2s3);

    if (s_hdma_i2s3_tx.State == HAL_DMA_STATE_BUSY) {
        (void)HAL_DMA_Abort(&s_hdma_i2s3_tx);
    }

    /* Abort 可能把 I2S 句柄留在 BUSY_TX，强制回 READY 供下次播放使用 */
    hi2s3.State = HAL_I2S_STATE_READY;
    hi2s3.ErrorCode = HAL_I2S_ERROR_NONE;

    return BSP_MAX98357A_OK;
}

/**
  * @brief I2S3_TX DMA 完成/半完成中断入口（覆盖 startup 的弱定义）
  */
void DMA1_Stream5_IRQHandler(void)
{
    HAL_DMA_IRQHandler(&s_hdma_i2s3_tx);
}
