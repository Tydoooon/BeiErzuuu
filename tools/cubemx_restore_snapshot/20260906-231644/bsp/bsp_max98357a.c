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
