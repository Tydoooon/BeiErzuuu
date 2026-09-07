#ifndef __APP_LED_H
#define __APP_LED_H

#include "dev_led.h"
#include "app_ui.h"
#include "FreeRTOS.h"
#include "queue.h"

extern QueueHandle_t g_key_queue;

void key_scan(void);
QueueHandle_t KeyQueueInit(void);

#endif
