#include "app_led.h"
#include "app_ui.h"

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

    if(B1 == 0 && B1_last == 1)
    {
        app_ui_key_push(1);
        LED1_ON();
        LED3_ON();
    }
    if(B2 == 0 && B2_last == 1)
    {
        app_ui_key_push(2);
        LED1_OFF();
        LED3_OFF();
    }
    if(B3 == 0 && B3_last == 1)
    {
        app_ui_key_push(3);
        lcd_num ++;
    }
    if(B4 == 0 && B4_last == 1)
    {
        app_ui_key_push(4);
        lcd_num --;
    }
    if(B5 == 0 && B5_last == 1)
    {
        app_ui_key_push(5);
    }

    B1_last = B1;
    B2_last = B2;
    B3_last = B3;
    B4_last = B4;
    B5_last = B5;
}
