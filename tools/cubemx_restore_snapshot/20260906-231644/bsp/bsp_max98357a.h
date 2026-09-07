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

#ifdef __cplusplus
}
#endif

#endif
