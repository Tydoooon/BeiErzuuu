#ifndef __BSP_MAX98357A_H
#define __BSP_MAX98357A_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BSP_MAX98357A_OK = 0,
    BSP_MAX98357A_ERROR,
    BSP_MAX98357A_TIMEOUT,
    BSP_MAX98357A_INVALID_PARAM
} bsp_max98357a_status_t;

bsp_max98357a_status_t BSP_MAX98357A_Init(void);
bsp_max98357a_status_t BSP_MAX98357A_Enable(bool enable);
bsp_max98357a_status_t BSP_MAX98357A_GetEnableState(bool *enabled);
bsp_max98357a_status_t BSP_MAX98357A_SetSampleRate(uint32_t sample_rate);
bsp_max98357a_status_t BSP_MAX98357A_TransmitWords(const uint16_t *data, uint16_t word_count, uint32_t timeout_ms);
bsp_max98357a_status_t BSP_MAX98357A_TransmitSilence(uint16_t word_count, uint32_t timeout_ms);

/* 流式播放：I2S3_TX 使用 DMA1_Stream5（SPI3_TX）传送
 * 注意：本函数可在 HAL_I2S_TxCpltCallback 内被调用（自动续传），
 * 以保持播放连续不产生块间空洞。*/
bsp_max98357a_status_t BSP_MAX98357A_TxStartDma(const uint16_t *data, uint16_t word_count);
bsp_max98357a_status_t BSP_MAX98357A_TxStopDma(void);

#ifdef __cplusplus
}
#endif

#endif
