#ifndef __APP_UI_H
#define __APP_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

typedef struct
{
    uint32_t last_ms;
    uint32_t average_ms;
    uint32_t max_ms;
    uint32_t sample_count;
} app_ui_key_latency_stats_t;

/**
 * @brief 创建 LVGL 应用界面，从登录界面开始运行。
 */
void app_ui_create(void);
bool app_ui_is_created(void);
void app_ui_load_w25q128_resources(void);
void app_ui_storage_service_init(void);
void app_ui_storage_task(void *argument);
void app_ui_notify_activity(void);
void app_ui_handle_idle(void);
void app_ui_sleep_now(void);
void app_ui_lock_screen(void);

/**
 * @brief 获取当前系统音量，音乐播放输出可按该值缩放。
 * @return 音量百分比，范围 0~100。
 */
uint8_t app_ui_get_system_volume(void);
uint8_t app_ui_get_cursor_sensitivity(void);
uint8_t app_ui_get_cursor_size(void);

/**
 * @brief 从按键扫描任务投递一个 UI 按键事件。
 * @param key_id 按键编号，范围 1~5。
 * @param generated_tick 按键消抖确认时的 FreeRTOS tick。
 */
void app_ui_key_push(uint8_t key_id, uint32_t generated_tick);

/**
 * @brief 在 LVGL 任务中处理已投递的按键事件。
 */
void app_ui_process_keys(void);
void app_ui_record_discarded_event(void);
void app_ui_get_key_latency_stats(app_ui_key_latency_stats_t *stats);

#ifdef __cplusplus
}
#endif

#endif
