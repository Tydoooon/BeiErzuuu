#include "app_led.h"

QueueHandle_t g_key_queue = NULL;
uint8_t B1 = 0,B2 = 0,B3 = 0,B4 = 0,B5 = 0;
uint8_t B1_last = 1,B2_last = 1,B3_last = 1,B4_last = 1,B5_last = 1;
void key_scan(void)
{
    uint8_t key_id = 0;

    B1 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_7);
    B2 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_8);
    B3 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_9);
    B4 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_10);
    B5 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_11);

    if(B1 == 0 && B1_last == 1)
    {
        key_id = 1;
        LED_ON();
    }
    else if(B2 == 0 && B2_last == 1)
    {
        key_id = 2;
        LED_OFF();
    }
    else if(B3 == 0 && B3_last == 1)
    {
        key_id = 3;
    }
    else if(B4 == 0 && B4_last == 1)
    {
        key_id = 4;
    }
    else if(B5 == 0 && B5_last == 1)
    {
        key_id = 5;
    }

    if (key_id != 0 && g_key_queue != NULL) {
        xQueueSend(g_key_queue, &key_id, 0);
    }

    B1_last = B1;
    B2_last = B2;
    B3_last = B3;
    B4_last = B4;
    B5_last = B5;
}

void key_scan_reset(void)
{
    B1 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_7);
    B2 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_8);
    B3 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_9);
    B4 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_10);
    B5 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_11);

    B1_last = B1;
    B2_last = B2;
    B3_last = B3;
    B4_last = B4;
    B5_last = B5;
}

QueueHandle_t KeyQueueInit(void)
{
    QueueHandle_t KeyQueue;
    KeyQueue = xQueueCreate(5,sizeof(uint8_t));
    return KeyQueue;
}
