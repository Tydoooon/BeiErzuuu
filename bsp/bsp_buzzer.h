#ifndef __BSP_BUZZER_H
#define __BSP_BUZZER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef enum
{
    BSP_BUZZER_OK = 0,
    BSP_BUZZER_ERROR,
    BSP_BUZZER_INVALID_PARAM
} bsp_buzzer_status_t;

bsp_buzzer_status_t BSP_Buzzer_Init(void);
bsp_buzzer_status_t BSP_Buzzer_PlayTone(uint32_t frequency_hz, uint8_t duty_percent);
void BSP_Buzzer_Mute(void);
void BSP_Buzzer_Stop(void);
bool BSP_Buzzer_IsRunning(void);

#ifdef __cplusplus
}
#endif

#endif
