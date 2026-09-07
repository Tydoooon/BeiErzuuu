#include "app_ui_internal.h"

static void home_lock_event_cb(lv_event_t *e)
{
    (void)e;
    g_input_events++;
    app_ui_lock_screen();
}

static void main_app_event_cb(lv_event_t *e)
{
    ui_app_id_t app = (ui_app_id_t)(uintptr_t)lv_event_get_user_data(e);

    g_input_events++;
    ui_open_app(app);
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
    g_home_active = true;
    g_current_app = UI_APP_NONE;
    g_login_active = false;
    if (g_home_selected >= UI_APP_COUNT) {
        g_home_selected = 0U;
    }
    ui_label(scr, UI_TEXT_HOME_TITLE, 12, 10);
    ui_button(scr, UI_TEXT_HOME_LOCK, 124, 8, 52, 28, home_lock_event_cb, NULL);
    ui_add_sleep_button(scr, 188, 8);

    for (uint8_t i = 0; i < UI_APP_COUNT; i++) {
        lv_coord_t x = (i % 2) == 0 ? 12 : 124;
        lv_coord_t y = 42 + (i / 2) * 82;
        lv_obj_t *btn = ui_button(scr, names[i], x, y, 104, 70, main_app_event_cb, (void *)(uintptr_t)i);
        g_home_buttons[i] = btn;

        lv_obj_t *hint = lv_label_create(btn);
        lv_label_set_text(hint, hints[i]);
        lv_obj_set_style_text_font(hint, UI_FONT, 0);
        lv_obj_set_style_text_color(hint, lv_color_hex(0xEAF2FF), 0);
        lv_obj_align(hint, LV_ALIGN_BOTTOM_MID, 0, -7);
    }

    g_home_key_hint = ui_label(scr, "K1 D  K2 L  K3 R  K4 Enter  K5 U", 12, 292);
    ui_home_refresh_key_focus();
    ui_load_screen(scr);
}

void ui_home_refresh_key_focus(void)
{
    for (uint8_t i = 0; i < UI_APP_COUNT; i++) {
        if (g_home_buttons[i] == NULL) continue;
        lv_obj_set_style_bg_color(g_home_buttons[i],
                                  i == g_home_selected ?
                                  lv_color_hex(0xF59E0B) : lv_color_hex(0x2F80ED),
                                  0);
    }

    if (g_home_key_hint != NULL) {
        lv_label_set_text(g_home_key_hint, "K1 D  K2 L  K3 R  K4 Enter  K5 U");
    }
}
