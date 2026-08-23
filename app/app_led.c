#include "app_led.h"

extern uint8_t lcd_num;
uint8_t B1 = 0,B2 = 0,B3 = 0,B4 = 0,B5 = 0;
uint8_t B1_last = 1,B2_last = 1,B3_last = 1,B4_last = 1,B5_last = 1;
void key_scan(void)
{
    B1 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_7);
    B2 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_8);
    B3 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_9);
    B4 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_10);
    B5 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_11);

    if(B1 == 1 && B1_last == 0)
    {
        LED1_ON();
        LED3_ON();
    }
    if(B2 == 1 && B2_last == 0)
    {
        LED1_OFF();
        LED3_OFF();
    }
    if(B3 == 1 && B3_last == 0)
    {
        lcd_num ++;
    }
    if(B4 == 1 && B4_last == 0)
    {
        lcd_num --;
    }
    if(B5 == 1 && B5_last == 0)
    {
        HAL_GPIO_TogglePin(GPIOD,GPIO_PIN_9);
    }

    B1_last = B1;
    B2_last = B2;
    B3_last = B3;
    B4_last = B4;
    B5_last = B5;
}
