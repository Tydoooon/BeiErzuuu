#ifndef __BSP_LED_H
#define __BSP_LED_H

#include "main.h"
#include "gpio.h"

void bsp_led_set(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);
void bsp_led_reset(GPIO_TypeDef *GPIOx, uint16_t GPIO_Pin);

#endif
