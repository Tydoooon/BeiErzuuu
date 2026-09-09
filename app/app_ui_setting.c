#include "app_ui_internal.h"
#include "app_mouse.h"

#define UI_SETTING_COUNT 5U

static lv_obj_t *g_setting_value_labels[UI_SETTING_COUNT];

static void ui_setting_update_value_label(uint8_t id, int32_t value);

static void setting_slider_event_cb(lv_event_t *e)
{
    uint8_t id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);

    g_input_events++;

    if (id == 0) {
        g_settings.cursor_sensitivity = (uint8_t)value;
    } else if (id == 1) {
        g_settings.cursor_size = (uint8_t)value;
        app_mouse_apply_settings();
    }
    else if (id == 2) {
        g_settings.screen_brightness = (uint8_t)value;
        ui_apply_brightness();
    } else if (id == 3) {
        g_settings.system_volume = (uint8_t)value;
    } else {
        g_settings.screen_timeout_s = (uint16_t)value;
    }

    ui_setting_update_value_label(id, value);

    if (!ui_storage_request_commit(UI_STORAGE_FEEDBACK_SETTINGS)) {
        g_error_events++;
        ui_log("Setting save request failed");
    }
}

static void ui_setting_row(lv_obj_t *parent, const char *name, uint8_t id,
                           int32_t min, int32_t max, int32_t value, lv_coord_t y)
{
    lv_obj_t *slider;

    ui_label(parent, name, 18, y);
    slider = lv_slider_create(parent);
    lv_obj_set_size(slider, 128, 10);
    lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 94, y + 4);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, setting_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)id);

    if (id < UI_SETTING_COUNT) {
        g_setting_value_labels[id] = ui_label(parent, "", 94, y + 17);
        ui_setting_update_value_label(id, value);
    }
}

void ui_create_setting_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_SETTING);

    memset(g_setting_value_labels, 0, sizeof(g_setting_value_labels));

    ui_setting_row(scr, UI_TEXT_SETTING_SENSITIVITY, 0, 1, 100, g_settings.cursor_sensitivity, 58);
    ui_setting_row(scr, UI_TEXT_SETTING_CURSOR_SIZE, 1, 4, 32, g_settings.cursor_size, 96);
    ui_setting_row(scr, UI_TEXT_SETTING_BRIGHTNESS, 2, 1, 100, g_settings.screen_brightness, 134);
    ui_setting_row(scr, UI_TEXT_SETTING_VOLUME, 3, 0, 100, g_settings.system_volume, 172);
    ui_setting_row(scr, UI_TEXT_SETTING_TIMEOUT, 4, 10, 300, g_settings.screen_timeout_s, 210);
    ui_label(scr, UI_TEXT_SETTING_TIP, 30, 260);
    ui_label(scr, "K4 Exit", 94, 292);

    ui_load_screen(scr);
}

static void ui_setting_update_value_label(uint8_t id, int32_t value)
{
    if (id >= UI_SETTING_COUNT || g_setting_value_labels[id] == NULL) {
        return;
    }

    if (id == 2U || id == 3U) {
        lv_label_set_text_fmt(g_setting_value_labels[id], "%ld%%", (long)value);
    } else if (id == 4U) {
        lv_label_set_text_fmt(g_setting_value_labels[id], "%lds", (long)value);
    } else {
        lv_label_set_text_fmt(g_setting_value_labels[id], "%ld", (long)value);
    }
}
