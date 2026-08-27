#include "app_ui_internal.h"

lv_obj_t *g_password_ta;
lv_obj_t *g_login_hint;
lv_obj_t *g_file_list;
lv_obj_t *g_file_detail;
lv_obj_t *g_canvas;
lv_obj_t *g_draw_status;
lv_obj_t *g_music_status;
lv_obj_t *g_monitor_label;
lv_timer_t *g_monitor_timer;

ui_file_entry_t g_files[UI_FILE_MAX];
ui_settings_t g_settings;
ui_log_entry_t g_logs[UI_LOG_MAX];
lv_color_t g_canvas_buf[UI_DRAW_W * UI_DRAW_H];
uint8_t g_draw_save[UI_DRAW_SAVE_BYTES];

uint8_t g_log_count;
uint8_t g_selected_file;
uint8_t g_file_serial;
uint32_t g_input_events;
uint32_t g_discarded_events;
uint32_t g_error_events;
volatile uint8_t g_pending_keys;
bool g_storage_dirty;
bool g_login_active;
bool g_music_playing;

void app_ui_create(void)
{
    ui_model_init();
    ui_font_init();
    ui_create_login();
}

uint8_t app_ui_get_system_volume(void)
{
    return g_settings.system_volume;
}

void app_ui_key_push(uint8_t key_id)
{
    if (key_id == 0U || key_id > 5U) return;
    g_pending_keys |= (uint8_t)(1U << (key_id - 1U));
}

void app_ui_process_keys(void)
{
    uint8_t keys;

    taskENTER_CRITICAL();
    keys = g_pending_keys;
    g_pending_keys = 0U;
    taskEXIT_CRITICAL();

    if (!g_login_active || g_password_ta == NULL || keys == 0U) return;

    for (uint8_t key_id = 1U; key_id <= 5U; key_id++) {
        if ((keys & (uint8_t)(1U << (key_id - 1U))) == 0U) continue;

        g_input_events++;
        if (key_id == 5U) {
            ui_login_submit();
        } else if (strlen(lv_textarea_get_text(g_password_ta)) < 8U) {
            char digit[2] = { (char)('0' + key_id), '\0' };
            lv_textarea_add_text(g_password_ta, digit);
            lv_label_set_text(g_login_hint, UI_TEXT_KEY_HINT);
        } else {
            g_discarded_events++;
        }
    }
}
