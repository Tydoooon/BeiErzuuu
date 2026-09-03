#include "app_ui_internal.h"

static void ui_monitor_refresh(void);
static void ui_monitor_status_text(uint32_t bits, char *out, size_t out_size);

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
    char status[48];
    uint32_t bits;

    if (g_monitor_label == NULL) return;

    lv_mem_monitor(&mem);
    free_pct = mem.used_pct <= 100U ? (uint8_t)(100U - mem.used_pct) : 0U;
    bits = app_event_get_bits();
    ui_monitor_status_text(bits, status, sizeof(status));
    lv_label_set_text_fmt(g_monitor_label, UI_TEXT_MONITOR_FMT,
                          (unsigned long)(lv_tick_get() / 1000U),
                          status,
                          (unsigned long)g_input_events,
                          (unsigned long)g_discarded_events,
                          (unsigned long)g_error_events,
                          (unsigned long)uxTaskGetNumberOfTasks(),
                          (unsigned long)uxTaskGetStackHighWaterMark(NULL),
                          (unsigned long)xPortGetFreeHeapSize(),
                          free_pct);
}

static void ui_monitor_status_text(uint32_t bits, char *out, size_t out_size)
{
    size_t used = 0U;

    if (out == NULL || out_size == 0U) return;
    out[0] = '\0';

    if ((bits & APP_EVT_UI_READY) != 0U) {
        used += snprintf(out + used, out_size - used, "UI ");
    }
    if ((bits & APP_EVT_FLASH_READY) != 0U && used < out_size) {
        used += snprintf(out + used, out_size - used, "Flash ");
    }
    if ((bits & APP_EVT_USB_READY) != 0U && used < out_size) {
        used += snprintf(out + used, out_size - used, "USB ");
    }
    if ((bits & APP_EVT_SCREEN_SLEEP) != 0U && used < out_size) {
        used += snprintf(out + used, out_size - used, "Sleep ");
    }
    if ((bits & APP_EVT_LOGIN_LOCKED) != 0U && used < out_size) {
        used += snprintf(out + used, out_size - used, "Login ");
    }
    if ((bits & APP_EVT_MUSIC_PLAYING) != 0U && used < out_size) {
        used += snprintf(out + used, out_size - used, "Music ");
    }
    if ((bits & APP_EVT_STORAGE_DIRTY) != 0U && used < out_size) {
        used += snprintf(out + used, out_size - used, "Dirty ");
    }
    if ((bits & APP_EVT_STORAGE_BUSY) != 0U && used < out_size) {
        used += snprintf(out + used, out_size - used, "Busy ");
    }

    if (out[0] == '\0') {
        (void)snprintf(out, out_size, "%s", UI_TEXT_STATUS_NORMAL);
    }
}
