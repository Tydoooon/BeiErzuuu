#include "app_ui_internal.h"

static void main_app_event_cb(lv_event_t *e)
{
    ui_app_id_t app = (ui_app_id_t)(uintptr_t)lv_event_get_user_data(e);

    g_input_events++;
    switch (app) {
    case UI_APP_FILE: ui_create_file_app(); break;
    case UI_APP_DRAW: ui_create_draw_app(); break;
    case UI_APP_MUSIC: ui_create_music_app(); break;
    case UI_APP_LOG: ui_create_log_app(); break;
    case UI_APP_MONITOR: ui_create_monitor_app(); break;
    case UI_APP_SETTING: ui_create_setting_app(); break;
    default: break;
    }
}

void ui_create_main(void)
{
    static const char *names[] = {
        UI_TEXT_APP_FILE, UI_TEXT_APP_DRAW, UI_TEXT_APP_MUSIC,
        UI_TEXT_APP_LOG, UI_TEXT_APP_MONITOR, UI_TEXT_APP_SETTING
    };
    static const char *hints[] = {
        UI_TEXT_HINT_FILE, UI_TEXT_HINT_DRAW, UI_TEXT_HINT_MUSIC,
        UI_TEXT_HINT_LOG, UI_TEXT_HINT_MONITOR, UI_TEXT_HINT_SETTING
    };

    lv_obj_t *scr = ui_create_screen();
    g_login_active = false;
    ui_label(scr, UI_TEXT_HOME_TITLE, 12, 10);

    for (uint8_t i = 0; i < 6; i++) {
        lv_coord_t x = (i % 2) == 0 ? 12 : 124;
        lv_coord_t y = 42 + (i / 2) * 82;
        lv_obj_t *btn = ui_button(scr, names[i], x, y, 104, 70, main_app_event_cb, (void *)(uintptr_t)i);

        lv_obj_t *hint = lv_label_create(btn);
        lv_label_set_text(hint, hints[i]);
        lv_obj_set_style_text_font(hint, UI_FONT, 0);
        lv_obj_set_style_text_color(hint, lv_color_hex(0xEAF2FF), 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -7);
    }

    ui_label(scr, g_storage_dirty ? UI_TEXT_STORAGE_DIRTY : UI_TEXT_STORAGE_SYNCED, 12, 292);
    ui_load_screen(scr);
}
