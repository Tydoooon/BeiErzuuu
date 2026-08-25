#ifndef __APP_UI_H
#define __APP_UI_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 创建 LVGL 应用界面，从登录界面开始运行。
 */
void app_ui_create(void);

/**
 * @brief 获取当前系统音量，音乐播放输出可按该值缩放。
 * @return 音量百分比，范围 0~100。
 */
uint8_t app_ui_get_system_volume(void);

/**
 * @brief 从按键扫描任务投递一个 UI 按键事件。
 * @param key_id 按键编号，范围 1~5。
 */
void app_ui_key_push(uint8_t key_id);

/**
 * @brief 在 LVGL 任务中处理已投递的按键事件。
 */
void app_ui_process_keys(void);

#ifdef __cplusplus
}
#endif

#endif
