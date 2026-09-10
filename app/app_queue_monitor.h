#ifndef __APP_QUEUE_MONITOR_H
#define __APP_QUEUE_MONITOR_H

#include "FreeRTOS.h"
#include "queue.h"

#include <stdbool.h>

typedef enum
{
    APP_QUEUE_KEY = 0,
    APP_QUEUE_MUSIC_COMMAND,
    APP_QUEUE_STORAGE_REQUEST,
    APP_QUEUE_STORAGE_RESULT,
    APP_QUEUE_COUNT
} app_queue_id_t;

typedef struct
{
    UBaseType_t current;
    UBaseType_t high_water_mark;
    UBaseType_t capacity;
} app_queue_watermark_t;

void app_queue_monitor_register(app_queue_id_t id, QueueHandle_t queue,
                                UBaseType_t capacity);
void app_queue_monitor_record_depth(app_queue_id_t id, UBaseType_t depth);
bool app_queue_monitor_get(app_queue_id_t id, app_queue_watermark_t *watermark);

#endif
