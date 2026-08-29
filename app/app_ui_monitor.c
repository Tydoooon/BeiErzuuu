#include "app_ui_internal.h"

static void ui_monitor_refresh(void);

static void monitor_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_monitor_refresh();
}

void ui_create_monitor_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_MONITOR);

    g_monitor_label = ui_label(scr, "", 14, 52);
    lv_obj_set_width(g_monitor_label, 212);
    ui_label(scr, "K4 Exit", 94, 286);
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
    lv_label_set_text_fmt(g_monitor_label, UI_TEXT_MONITOR_FMT,
                          (unsigned long)(lv_tick_get() / 1000U),
                          g_music_playing ? UI_TEXT_STATUS_MUSIC : UI_TEXT_STATUS_NORMAL,
                          (unsigned long)g_input_events,
                          (unsigned long)g_discarded_events,
                          (unsigned long)g_error_events,
                          (unsigned long)uxTaskGetNumberOfTasks(),
                          (unsigned long)uxTaskGetStackHighWaterMark(NULL),
                          (unsigned long)xPortGetFreeHeapSize(),
                          free_pct);
}
