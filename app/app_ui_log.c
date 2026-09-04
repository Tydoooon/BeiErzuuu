#include "app_ui_internal.h"

#define UI_LOG_WHEEL_STEP 24

static void ui_refresh_logs(lv_obj_t *list);

static lv_obj_t *g_log_list;

static void log_clear_event_cb(lv_event_t *e)
{
    (void)e;
    g_input_events++;
    g_log_count = 0;
    ui_log(UI_TEXT_LOG_CLEAR);
    ui_create_log_app();
}

void ui_create_log_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_LOG);
    lv_obj_t *list = lv_obj_create(scr);

    g_log_list = list;
    lv_obj_set_size(list, 216, 198);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 12, 48);
    lv_obj_set_style_text_font(list, UI_FONT, 0);
    lv_obj_set_style_bg_color(list, lv_color_white(), 0);
    lv_obj_set_scroll_dir(list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(list, LV_SCROLLBAR_MODE_AUTO);
    ui_refresh_logs(list);

    ui_button(scr, UI_TEXT_LOG_CLEAR, 70, 266, 100, 34, log_clear_event_cb, NULL);
    ui_label(scr, "K4 Exit", 94, 304);
    ui_load_screen(scr);
}

void ui_log_handle_wheel(int16_t wheel_delta)
{
    if (g_log_list == NULL || wheel_delta == 0) {
        return;
    }

    lv_obj_scroll_by_bounded(g_log_list, 0, (lv_coord_t)wheel_delta * UI_LOG_WHEEL_STEP, LV_ANIM_OFF);
}

static void ui_refresh_logs(lv_obj_t *list)
{
    char line[96];

    lv_obj_clean(list);
    for (uint8_t i = 0; i < g_log_count; i++) {
        snprintf(line, sizeof(line), "[%lus] %s",
                 (unsigned long)(g_logs[i].tick_ms / 1000U), g_logs[i].text);
        lv_obj_t *label = lv_label_create(list);
        lv_label_set_text(label, line);
        lv_obj_set_style_text_font(label, UI_FONT, 0);
        lv_obj_set_width(label, 194);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, 4 + i * 20);
    }
}
