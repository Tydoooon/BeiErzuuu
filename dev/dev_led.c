#include "dev_led.h"
#include "bsp_led.h"

void LED_ON(void)
{
    bsp_led_set(GPIOB, GPIO_PIN_13);
}

void LED_OFF(void)
{
    bsp_led_reset(GPIOB, GPIO_PIN_13);
}
