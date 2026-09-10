#ifndef __APP_POWER_H
#define __APP_POWER_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>

/**
 * @brief 进入 STOP 低功耗模式（真息屏）。
 *
 * RTC 会周期性唤醒 CPU 喂 IWDG 并立即重新进入 STOP；函数仍会阻塞，
 * 直到按键（PE7~PE11）触发 EXTI 才真正退出，并恢复 168MHz 主时钟、
 * 按键轮询输入、节拍中断与背光亮度。
 *
 * 调用前应先通过 app_power_can_sleep() 判断是否允许进入低功耗，
 * 否则播放中的音乐 / 活动的 USB 鼠标会因外设停摆而中断。
 */
void app_power_enter_stop(void);

/**
 * @brief 当前是否允许进入 STOP 低功耗。
 *
 * 无音乐播放、且无活动 USB 设备（APPLICATION_READY）时才允许。
 * 否则应退回“仅关背光”的旧逻辑，避免打断音乐或让鼠标断连。
 */
bool app_power_can_sleep(void);

#ifdef __cplusplus
}
#endif

#endif
