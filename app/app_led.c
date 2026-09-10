#include "app_led.h"
#include "app_queue_monitor.h"

/* 软件消抖：扫描周期 10ms，连续 N 次读到相同电平才确认跳变（约 30ms） */
#define KEY_DEBOUNCE_CNT 3U

QueueHandle_t g_key_queue = NULL;
uint8_t B1 = 0,B2 = 0,B3 = 0,B4 = 0,B5 = 0;
uint8_t B1_last = 1,B2_last = 1,B3_last = 1,B4_last = 1,B5_last = 1;

static uint8_t B1_cnt = 0,B2_cnt = 0,B3_cnt = 0,B4_cnt = 0,B5_cnt = 0;
static uint32_t g_key_queue_send_failures;

/* 稳定计数消抖：raw 与当前稳定值不同则累加计数，达到阈值才接受跳变 */
static uint8_t key_debounce(uint8_t raw, uint8_t stable, uint8_t *cnt)
{
    if (raw == stable) {
        *cnt = 0;
        return stable;
    }

    (*cnt)++;
    if (*cnt >= KEY_DEBOUNCE_CNT) {
        *cnt = 0;
        return raw;
    }

    return stable;
}

void key_scan(void)
{
    uint8_t key_id = 0;
    uint8_t raw1, raw2, raw3, raw4, raw5;

    raw1 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_7);
    raw2 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_8);
    raw3 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_9);
    raw4 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_10);
    raw5 = HAL_GPIO_ReadPin(GPIOE,GPIO_PIN_11);

    B1 = key_debounce(raw1, B1, &B1_cnt);
    B2 = key_debounce(raw2, B2, &B2_cnt);
    B3 = key_debounce(raw3, B3, &B3_cnt);
    B4 = key_debounce(raw4, B4, &B4_cnt);
    B5 = key_debounce(raw5, B5, &B5_cnt);

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
        app_key_event_t event = {
            .key_id = key_id,
            .generated_tick = xTaskGetTickCount(),
        };
        BaseType_t send_result;
        UBaseType_t depth = 0U;

        taskENTER_CRITICAL();
        send_result = xQueueSend(g_key_queue, &event, 0);
        if (send_result == pdPASS) {
            depth = uxQueueMessagesWaiting(g_key_queue);
        } else {
            g_key_queue_send_failures++;
        }
        taskEXIT_CRITICAL();

        if (send_result == pdPASS) {
            app_queue_monitor_record_depth(APP_QUEUE_KEY, depth);
        } else {
            app_ui_record_discarded_event();
        }
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

    B1_cnt = 0;
    B2_cnt = 0;
    B3_cnt = 0;
    B4_cnt = 0;
    B5_cnt = 0;
}

QueueHandle_t KeyQueueInit(void)
{
    QueueHandle_t KeyQueue;

    taskENTER_CRITICAL();
    g_key_queue_send_failures = 0U;
    taskEXIT_CRITICAL();

    KeyQueue = xQueueCreate(APP_KEY_QUEUE_LENGTH, sizeof(app_key_event_t));
    app_queue_monitor_register(APP_QUEUE_KEY, KeyQueue, APP_KEY_QUEUE_LENGTH);
    return KeyQueue;
}

void app_key_queue_get_stats(app_key_queue_stats_t *stats)
{
    if (stats == NULL) {
        return;
    }

    taskENTER_CRITICAL();
    stats->send_failures = g_key_queue_send_failures;
    taskEXIT_CRITICAL();
}
