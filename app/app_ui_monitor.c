#include "app_ui_internal.h"
#include "app_fault.h"
#include "app_led.h"
#include "app_queue_monitor.h"

#define UI_MONITOR_TASK_MAX 12
#define UI_MONITOR_WHEEL_STEP 24
#define UI_MONITOR_LINE_H 19
#define UI_MONITOR_SUMMARY_H (14 * UI_MONITOR_LINE_H)
#define UI_MONITOR_TASK_ROW_H 36
#define UI_MONITOR_TASK_Y0 (UI_MONITOR_SUMMARY_H + 4 + UI_MONITOR_LINE_H)

static void ui_monitor_refresh(void);
static void ui_monitor_refresh_tasks(void);
static void ui_monitor_status_text(uint32_t bits, char *out, size_t out_size);
static void ui_monitor_reset_text(char *out, size_t out_size);
static const char *monitor_task_state_name(eTaskState state);
static bool ui_monitor_previous_run(TaskHandle_t handle, uint32_t *run_time);
static void ui_monitor_save_snapshot(UBaseType_t count, uint32_t total_run_time);
static void ui_monitor_sort_tasks(UBaseType_t count);
static void ui_monitor_set_text(lv_obj_t *label, const char *text);

typedef struct
{
    TaskHandle_t handle;
    uint32_t run_time;
} ui_monitor_task_sample_t;

static lv_obj_t *g_monitor_scroll;
static lv_obj_t *g_monitor_task_labels[UI_MONITOR_TASK_MAX];
static TaskStatus_t g_monitor_task_status[UI_MONITOR_TASK_MAX];
static ui_monitor_task_sample_t g_monitor_previous[UI_MONITOR_TASK_MAX];
static UBaseType_t g_monitor_previous_count;
static uint32_t g_monitor_previous_total;
static uint8_t g_monitor_cpu;
static bool g_monitor_sample_valid;

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
    g_monitor_sample_valid = false;
    g_monitor_previous_count = 0U;
    g_monitor_cpu = 0U;

    lv_obj_set_size(scroll, 216, 236);
    lv_obj_align(scroll, LV_ALIGN_TOP_LEFT, 12, 44);
    lv_obj_set_style_bg_color(scroll, lv_color_white(), 0);
    lv_obj_set_style_border_width(scroll, 0, 0);
    lv_obj_set_style_pad_all(scroll, 0, 0);
    lv_obj_set_scroll_dir(scroll, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(scroll, LV_SCROLLBAR_MODE_AUTO);

    g_monitor_label = ui_label(scroll, "", 4, 0);
    lv_obj_set_size(g_monitor_label, 208, UI_MONITOR_SUMMARY_H);
    lv_label_set_long_mode(g_monitor_label, LV_LABEL_LONG_CLIP);

    ui_label(scroll, UI_TEXT_MONITOR_TASKS, 4, UI_MONITOR_SUMMARY_H + 4);

    for (uint8_t i = 0; i < UI_MONITOR_TASK_MAX; i++) {
        g_monitor_task_labels[i] = lv_label_create(scroll);
        /* 任务信息全是 ASCII，使用内置字体可避免每次从外部 Flash 取字形。 */
        lv_obj_set_style_text_font(g_monitor_task_labels[i], LV_FONT_DEFAULT, 0);
        lv_obj_set_size(g_monitor_task_labels[i], 204, UI_MONITOR_TASK_ROW_H);
        lv_label_set_long_mode(g_monitor_task_labels[i], LV_LABEL_LONG_CLIP);
        lv_obj_align(g_monitor_task_labels[i], LV_ALIGN_TOP_LEFT, 4,
                     (lv_coord_t)(UI_MONITOR_TASK_Y0 + (lv_coord_t)i * UI_MONITOR_TASK_ROW_H));
    }

    ui_label(scr, "K4 Exit", 94, 286);

    ui_monitor_refresh();
    ui_load_screen(scr);

    g_monitor_timer = lv_timer_create(monitor_timer_cb, 1000, NULL);
}

void ui_monitor_handle_wheel(int16_t wheel_delta)
{
    if (g_monitor_scroll == NULL || wheel_delta == 0) {
        return ;
    }

    lv_obj_scroll_by_bounded(g_monitor_scroll, 0,
                             (lv_coord_t)wheel_delta * UI_MONITOR_WHEEL_STEP, LV_ANIM_OFF);
}

static void ui_monitor_refresh(void)
{
    lv_mem_monitor_t mem;
    uint8_t free_pct;
    char status[48];
    char reset[64];
    uint32_t bits;
    app_key_queue_stats_t queue_stats;
    app_ui_key_latency_stats_t latency_stats;
    app_queue_watermark_t key_queue;
    app_queue_watermark_t music_queue;
    app_queue_watermark_t storage_request_queue;
    app_queue_watermark_t storage_result_queue;

    ui_monitor_refresh_tasks();

    if (g_monitor_label != NULL) {
        lv_mem_monitor(&mem);
        free_pct = mem.used_pct <= 100U ? (uint8_t)(100U - mem.used_pct) : 0U;
        bits = app_event_get_bits();
        ui_monitor_status_text(bits, status, sizeof(status));
        ui_monitor_reset_text(reset, sizeof(reset));
        app_key_queue_get_stats(&queue_stats);
        app_ui_get_key_latency_stats(&latency_stats);
        (void)app_queue_monitor_get(APP_QUEUE_KEY, &key_queue);
        (void)app_queue_monitor_get(APP_QUEUE_MUSIC_COMMAND, &music_queue);
        (void)app_queue_monitor_get(APP_QUEUE_STORAGE_REQUEST, &storage_request_queue);
        (void)app_queue_monitor_get(APP_QUEUE_STORAGE_RESULT, &storage_result_queue);
        lv_label_set_text_fmt(g_monitor_label, UI_TEXT_MONITOR_FMT,
                               (unsigned long)(lv_tick_get() / 1000U),
                               status,
                               reset,
                               (unsigned)g_monitor_cpu,
                               (unsigned long)g_input_events,
                               (unsigned long)g_discarded_events,
                               (unsigned long)g_error_events,
                               (unsigned long)key_queue.current,
                               (unsigned long)key_queue.capacity,
                               (unsigned long)key_queue.high_water_mark,
                               (unsigned long)queue_stats.send_failures,
                               (unsigned long)music_queue.current,
                               (unsigned long)music_queue.capacity,
                               (unsigned long)music_queue.high_water_mark,
                               (unsigned long)storage_request_queue.current,
                               (unsigned long)storage_request_queue.capacity,
                               (unsigned long)storage_request_queue.high_water_mark,
                               (unsigned long)storage_result_queue.current,
                               (unsigned long)storage_result_queue.capacity,
                               (unsigned long)storage_result_queue.high_water_mark,
                               (unsigned long)latency_stats.last_ms,
                               (unsigned long)latency_stats.average_ms,
                               (unsigned long)latency_stats.max_ms,
                               (unsigned long)xPortGetFreeHeapSize(),
                               (unsigned long)xPortGetMinimumEverFreeHeapSize(),
                               (unsigned long)mem.free_size,
                               free_pct);
    }
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
    UBaseType_t sampled;
    uint32_t total_run_time = 0U;
    uint32_t total_delta;
    uint8_t idle_cpu = 0U;
    bool idle_found = false;

    if (count > UI_MONITOR_TASK_MAX) {
        for (uint8_t i = 0; i < UI_MONITOR_TASK_MAX; i++) {
            ui_monitor_set_text(g_monitor_task_labels[i], "");
        }
        snprintf(line, sizeof(line), "Too many tasks: %lu", (unsigned long)count);
        ui_monitor_set_text(g_monitor_task_labels[0], line);
        g_monitor_sample_valid = false;
        g_monitor_cpu = 0U;
        return;
    }

    sampled = (count > 0U)
                  ? uxTaskGetSystemState(g_monitor_task_status, count, &total_run_time)
                  : 0U;
    if (sampled == 0U) {
        for (uint8_t i = 0; i < UI_MONITOR_TASK_MAX; i++) {
            ui_monitor_set_text(g_monitor_task_labels[i], "");
        }
        ui_monitor_set_text(g_monitor_task_labels[0], UI_TEXT_MONITOR_NO_TASK);
        g_monitor_sample_valid = false;
        g_monitor_cpu = 0U;
        return;
    }

    total_delta = total_run_time - g_monitor_previous_total;
    ui_monitor_sort_tasks(sampled);

    for (uint8_t i = 0; i < UI_MONITOR_TASK_MAX; i++) {
        if ((UBaseType_t)i < sampled) {
            uint32_t previous_run = 0U;
            uint32_t run = g_monitor_task_status[i].ulRunTimeCounter;
            uint32_t cpu_x10 = 0U;

            if (g_monitor_sample_valid && total_delta != 0U &&
                ui_monitor_previous_run(g_monitor_task_status[i].xHandle, &previous_run)) {
                uint32_t run_delta = run - previous_run;
                cpu_x10 = (uint32_t)(((uint64_t)run_delta * 1000U) / total_delta);
                if (cpu_x10 > 1000U) {
                    cpu_x10 = 1000U;
                }
            }

            if (strcmp(g_monitor_task_status[i].pcTaskName, "IDLE") == 0) {
                idle_cpu = (uint8_t)(cpu_x10 / 10U);
                idle_found = true;
            }

            snprintf(line, sizeof(line), "%.10s %s P%lu CPU:%lu.%lu%%\nStack free:%luB",
                     g_monitor_task_status[i].pcTaskName,
                     monitor_task_state_name(g_monitor_task_status[i].eCurrentState),
                     (unsigned long)g_monitor_task_status[i].uxCurrentPriority,
                     (unsigned long)(cpu_x10 / 10U),
                     (unsigned long)(cpu_x10 % 10U),
                     (unsigned long)((uint32_t)g_monitor_task_status[i].usStackHighWaterMark * sizeof(StackType_t)));
            ui_monitor_set_text(g_monitor_task_labels[i], line);
        } else {
            ui_monitor_set_text(g_monitor_task_labels[i], "");
        }
    }

    g_monitor_cpu = (g_monitor_sample_valid && idle_found)
                        ? (uint8_t)(100U - idle_cpu)
                        : 0U;
    ui_monitor_save_snapshot(sampled, total_run_time);
    g_monitor_sample_valid = true;
}

static bool ui_monitor_previous_run(TaskHandle_t handle, uint32_t *run_time)
{
    for (UBaseType_t i = 0U; i < g_monitor_previous_count; i++) {
        if (g_monitor_previous[i].handle == handle) {
            *run_time = g_monitor_previous[i].run_time;
            return true;
        }
    }

    return false;
}

static void ui_monitor_save_snapshot(UBaseType_t count, uint32_t total_run_time)
{
    g_monitor_previous_count = count;
    g_monitor_previous_total = total_run_time;

    for (UBaseType_t i = 0U; i < count; i++) {
        g_monitor_previous[i].handle = g_monitor_task_status[i].xHandle;
        g_monitor_previous[i].run_time = g_monitor_task_status[i].ulRunTimeCounter;
    }
}

static void ui_monitor_sort_tasks(UBaseType_t count)
{
    for (UBaseType_t i = 1U; i < count; i++) {
        TaskStatus_t item = g_monitor_task_status[i];
        UBaseType_t pos = i;

        while (pos > 0U &&
               strcmp(g_monitor_task_status[pos - 1U].pcTaskName, item.pcTaskName) > 0) {
            g_monitor_task_status[pos] = g_monitor_task_status[pos - 1U];
            pos--;
        }
        g_monitor_task_status[pos] = item;
    }
}

static void ui_monitor_set_text(lv_obj_t *label, const char *text)
{
    const char *current;

    if (label == NULL || text == NULL) {
        return;
    }

    current = lv_label_get_text(label);
    if (current == NULL || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static void ui_monitor_reset_text(char *out, size_t out_size)
{
    const app_fault_reset_reason_t *r = app_fault_get_reset_reason();
    uint32_t fault = app_fault_get_last_fault();
    const char *cause = "POR";
    const char *fault_name = "-";

    if (r->independent_watchdog) {
        cause = "IWDG";
    } else if (r->window_watchdog) {
        cause = "WWDG";
    } else if (r->software_reset) {
        cause = "Soft";
    } else if (r->low_power_reset) {
        cause = "LP";
    } else if (r->power_on) {
        cause = "POR";
    }

    switch (fault) {
    case APP_FAULT_CODE_STACK_OVERFLOW: fault_name = "StackOVF"; break;
    case APP_FAULT_CODE_MALLOC_FAILED:  fault_name = "Malloc";   break;
    case APP_FAULT_CODE_ASSERT:         fault_name = "Assert";   break;
    case APP_FAULT_CODE_TASK_HUNG:      fault_name = "TaskHung"; break;
    default:                            fault_name = "-";        break;
    }

    snprintf(out, out_size, "复位:%s 上次故障:%s", cause, fault_name);
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
