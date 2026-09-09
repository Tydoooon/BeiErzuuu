#ifndef __APP_FAULT_H
#define __APP_FAULT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdbool.h>
#include <stdint.h>

/* 故障码：写入 .noinit 段，复位后仍可读取，用于定位“上次为什么复位” */
#define APP_FAULT_CODE_NONE           0U
#define APP_FAULT_CODE_STACK_OVERFLOW 1U
#define APP_FAULT_CODE_MALLOC_FAILED  2U
#define APP_FAULT_CODE_ASSERT         3U
#define APP_FAULT_CODE_TASK_HUNG      4U

/* 受监控的任务 id（心跳监测；部分任务可自动重启） */
typedef enum {
    APP_FAULT_TASK_KEY = 0,
    APP_FAULT_TASK_LCD,
    APP_FAULT_TASK_AUDIO,
    APP_FAULT_TASK_SAVE,
    APP_FAULT_TASK_COUNT
} app_fault_task_id_t;

/* 复位原因（RCC->CSR） */
typedef struct {
    bool independent_watchdog;   /* IWDG 复位 */
    bool window_watchdog;        /* WWDG 复位 */
    bool software_reset;         /* 软件复位 */
    bool power_on;               /* 上电复位 */
    bool low_power_reset;        /* 低功耗(退出 STOP)复位 */
} app_fault_reset_reason_t;

/* 启动 IWDG + 捕获复位原因 + 读取上次故障码（应在调度启动后尽早调用） */
void app_fault_init(void);
/* 创建监控任务（在各业务任务创建之后调用） */
void app_fault_start(void);
/* 任务心跳：各任务在自己的主循环里周期调用 */
void app_fault_heartbeat(app_fault_task_id_t id);
/* 可自动重启任务：进入/退出时置运行标志，供监控任务判断是否需要重建 */
void app_fault_task_entered(app_fault_task_id_t id);
void app_fault_task_exited(app_fault_task_id_t id);
/* 注册一个可自动重启的任务（handle 为该任务的 osThreadId_t*） */
void app_fault_register_restartable(app_fault_task_id_t id,
                                    void *handle_ptr,
                                    void (*fn)(void *),
                                    const void *attr);
/* 喂狗（空闲钩子/进入低功耗前/唤醒中断里调用） */
void app_fault_iwdg_refresh(void);

uint32_t app_fault_get_last_fault(void);
const app_fault_reset_reason_t *app_fault_get_reset_reason(void);
/* 记录故障码并复位系统（可任意上下文调用） */
void app_fault_trap(uint32_t code);

#ifdef __cplusplus
}
#endif

#endif
