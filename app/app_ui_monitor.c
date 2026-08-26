#include "app_ui_internal.h"

static void ui_monitor_refresh(void);

static void monitor_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_monitor_refresh();
}

void ui_create_monitor_app(void)
{
    lv_obj_t *scr = ui_create_app_frame("系统监控");

    g_monitor_label = ui_label(scr, "", 14, 52);
    lv_obj_set_width(g_monitor_label, 212);
    ui_monitor_refresh();
    ui_load_screen(scr);

    g_monitor_timer = lv_timer_create(monitor_timer_cb, 1000, NULL);
}

static void ui_monitor_refresh(void)
{
    lv_mem_monitor_t mem;
    uint8_t free_pct;

    if (g_monitor_label == NULL) return;

    lv_mem_monitor(&mem);
    free_pct = mem.used_pct <= 100U ? (uint8_t)(100U - mem.used_pct) : 0U;
    lv_label_set_text_fmt(g_monitor_label,
                          "运行时间:%lus\n系统状态:%s\n输入事件:%lu\n丢弃事件:%lu\n错误事件:%lu\n任务数量:%lu\n当前栈水位:%lu\nRTOS剩余堆:%luB\nLVGL空闲:%u%%",
                          (unsigned long)(lv_tick_get() / 1000U),
                          g_music_playing ? "音乐播放" : "正常",
                          (unsigned long)g_input_events,
                          (unsigned long)g_discarded_events,
                          (unsigned long)g_error_events,
                          (unsigned long)uxTaskGetNumberOfTasks(),
                          (unsigned long)uxTaskGetStackHighWaterMark(NULL),
                          (unsigned long)xPortGetFreeHeapSize(),
                          free_pct);
}
