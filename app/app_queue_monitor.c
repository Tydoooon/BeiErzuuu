#include "app_queue_monitor.h"

typedef struct
{
    QueueHandle_t queue;
    UBaseType_t high_water_mark;
    UBaseType_t capacity;
} app_queue_monitor_entry_t;

static app_queue_monitor_entry_t g_queue_entries[APP_QUEUE_COUNT];

void app_queue_monitor_register(app_queue_id_t id, QueueHandle_t queue,
                                UBaseType_t capacity)
{
    UBaseType_t current = 0U;

    if ((uint32_t)id >= (uint32_t)APP_QUEUE_COUNT || queue == NULL) {
        return;
    }

    current = uxQueueMessagesWaiting(queue);

    g_queue_entries[id].queue = queue;
    g_queue_entries[id].capacity = capacity;
    g_queue_entries[id].high_water_mark = current;
}

void app_queue_monitor_record_depth(app_queue_id_t id, UBaseType_t depth)
{
    if ((uint32_t)id >= (uint32_t)APP_QUEUE_COUNT) {
        return;
    }

    taskENTER_CRITICAL();
    if (depth > g_queue_entries[id].high_water_mark) {
        g_queue_entries[id].high_water_mark = depth;
    }
    taskEXIT_CRITICAL();
}

bool app_queue_monitor_get(app_queue_id_t id, app_queue_watermark_t *watermark)
{
    QueueHandle_t queue;

    if ((uint32_t)id >= (uint32_t)APP_QUEUE_COUNT || watermark == NULL) {
        return false;
    }

    taskENTER_CRITICAL();
    queue = g_queue_entries[id].queue;
    watermark->capacity = g_queue_entries[id].capacity;
    watermark->high_water_mark = g_queue_entries[id].high_water_mark;
    taskEXIT_CRITICAL();

    watermark->current = (queue != NULL) ? uxQueueMessagesWaiting(queue) : 0U;
    return queue != NULL;
}
