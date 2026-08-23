#include "dev_led.h"
#include "bsp_led.h"

void LED1_ON(void)
{
    bsp_led_set(GPIOB, GPIO_PIN_11);
}

void LED1_OFF(void)
{
    bsp_led_reset(GPIOB, GPIO_PIN_11);
}

void LED2_ON(void)
{
    bsp_led_set(GPIOB, GPIO_PIN_12);
}

void LED2_OFF(void)
{
    bsp_led_reset(GPIOB, GPIO_PIN_12);
}

void LED3_ON(void)
{
    bsp_led_set(GPIOB, GPIO_PIN_13);
}

void LED3_OFF(void)
{
    bsp_led_reset(GPIOB, GPIO_PIN_13);
}
