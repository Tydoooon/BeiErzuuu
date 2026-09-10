#ifndef __APP_LED_H
#define __APP_LED_H

#include "dev_led.h"
#include "app_ui.h"
#include "FreeRTOS.h"
#include "queue.h"

#define APP_KEY_QUEUE_LENGTH 5U

typedef struct
{
    uint8_t key_id;
    TickType_t generated_tick;
} app_key_event_t;

typedef struct
{
    uint32_t send_failures;
} app_key_queue_stats_t;

extern QueueHandle_t g_key_queue;

void key_scan(void);
void key_scan_reset(void);
QueueHandle_t KeyQueueInit(void);
void app_key_queue_get_stats(app_key_queue_stats_t *stats);

#endif
