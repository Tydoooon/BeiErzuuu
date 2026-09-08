#include "app_ui_internal.h"

#define UI_MONITOR_TASK_MAX 16
#define UI_MONITOR_WHEEL_STEP 24
#define UI_MONITOR_LINE_H 19                 /* UI_FONT 行高 */
#define UI_MONITOR_SUMMARY_H (7 * UI_MONITOR_LINE_H)  /* 汇总标签共 7 行 */
#define UI_MONITOR_TASK_Y0 (UI_MONITOR_SUMMARY_H + 4 + UI_MONITOR_LINE_H)  /* 任务首行 Y */

static void ui_monitor_refresh(void);
static void ui_monitor_refresh_tasks(void);
static void ui_monitor_status_text(uint32_t bits, char *out, size_t out_size);
static const char *monitor_task_state_name(eTaskState state);

static lv_obj_t *g_monitor_scroll;
static lv_obj_t *g_monitor_task_labels[UI_MONITOR_TASK_MAX];
static TaskStatus_t g_monitor_task_status[UI_MONITOR_TASK_MAX];

static void monitor_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_monitor_refresh();
}

void ui_create_monitor_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_MONITOR);
    lv_obj_t *scroll = lv_obj_create(scr);

    g_monitor_scroll = scroll;
    /* 汇总 + 任务列表放进同一个可滚动图层，滚轮滚动整个界面 */
    lv_obj_set_size(scroll, 216, 236);
    lv_obj_align(scroll, LV_ALIGN_TOP_LEFT, 12, 44);
    lv_obj_set_style_bg_color(scroll, lv_color_white(), 0);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_style_pad_all(scroll, 0, 0);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_AUTO);

    g_monitor_label = ui_label(scroll, "", 4, 0);
    lv_obj_set_width(g_monitor_label, 208);

    ui_label(scroll, UI_TEXT_MONITOR_TASKS, 4, UI_MONITOR_SUMMARY_H + 4);

    /* 任务标签一次性创建，刷新时原地改文本。
     * 不再用 lv_obj_clean 重建，避免每次刷新把滚动位置重置回顶部。 */
    for (uint8_t i = 0; i < UI_MONITOR_TASK_MAX; i++) {
        g_monitor_task_labels[i] = lv_label_create(scroll);
        lv_obj_set_style_text_font(g_monitor_task_labels[i], UI_FONT, 0);
        lv_obj_set_width(g_monitor_task_labels[i], 194);
        lv_obj_align(g_monitor_task_labels[i], LV_ALIGN_TOP_LEFT, 4,
                     (lv_coord_t)(UI_MONITOR_TASK_Y0 + (lv_coord_t)i * UI_MONITOR_LINE_H));
    }

    ui_label(scr, "K4 Exit", 94, 286);

    ui_monitor_refresh();
    ui_load_screen(scr);

    g_monitor_timer = lv_timer_create(monitor_timer_cb, 1000, NULL);
}

void ui_monitor_handle_wheel(int16_t wheel_delta)
{
    if (g_monitor_scroll == NULL || wheel_delta == 0) {
        return;
    }

    lv_obj_scroll_by_bounded(g_monitor_scroll, 0,
                             (lv_coord_t)wheel_delta * UI_MONITOR_WHEEL_STEP, LV_ANIM_OFF);
}

static void ui_monitor_refresh(void)
{
    lv_mem_monitor_t mem;
    uint8_t free_pct;
    char status[48];
    uint32_t bits;

    if (g_monitor_label != NULL) {
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
                              (unsigned long)xPortGetFreeHeapSize(),
                              free_pct);
    }

    ui_monitor_refresh_tasks();
}

static const char *monitor_task_state_name(eTaskState state)
{
    switch (state) {
    case eRunning:   return "Run";
    case eReady:     return "Rdy";
    case eBlocked:   return "Blk";
    case eSuspended: return "Sus";
    case eDeleted:   return "Del";
    default:         return "Inv";
    }
}

static void ui_monitor_refresh_tasks(void)
{
    char line[96];
    UBaseType_t count = uxTaskGetNumberOfTasks();
    uint32_t total_run_time = 0U;

    if (count > UI_MONITOR_TASK_MAX) {
        count = UI_MONITOR_TASK_MAX;
    }

    if (count == 0U || uxTaskGetSystemState(g_monitor_task_status, count, &total_run_time) == 0U) {
        for (uint8_t i = 0; i < UI_MONITOR_TASK_MAX; i++) {
            lv_label_set_text(g_monitor_task_labels[i], "");
        }
        lv_label_set_text(g_monitor_task_labels[0], UI_TEXT_MONITOR_NO_TASK);
        return;
    }

    for (uint8_t i = 0; i < UI_MONITOR_TASK_MAX; i++) {
        if ((UBaseType_t)i < count) {
            snprintf(line, sizeof(line), "%s  P%lu %s  %luB",
                     g_monitor_task_status[i].pcTaskName,
                     (unsigned long)g_monitor_task_status[i].uxCurrentPriority,
                     monitor_task_state_name(g_monitor_task_status[i].eCurrentState),
                     (unsigned long)((uint32_t)g_monitor_task_status[i].usStackHighWaterMark * sizeof(StackType_t)));
            lv_label_set_text(g_monitor_task_labels[i], line);
        } else {
            lv_label_set_text(g_monitor_task_labels[i], "");
        }
    }
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
