#include "app_ui_internal.h"

void ui_login_submit(void)
{
    if (strcmp(lv_textarea_get_text(g_password_ta), UI_PASSWORD) == 0) {
        ui_log(UI_TEXT_LOGIN_SUCCESS);
        ui_create_main();
    } else {
        g_error_events++;
        ui_log(UI_TEXT_LOGIN_FAILED);
        lv_label_set_text(g_login_hint, UI_TEXT_LOGIN_WRONG);
        lv_textarea_set_text(g_password_ta, "");
    }
}

static void login_button_event_cb(lv_event_t *e)
{
    (void)e;
    g_input_events++;
    ui_login_submit();
}

static void login_keypad_event_cb(lv_event_t *e)
{
    lv_obj_t *btnm = lv_event_get_target(e);
    uint16_t id = lv_btnmatrix_get_selected_btn(btnm);
    const char *txt = lv_btnmatrix_get_btn_text(btnm, id);

    if (txt == NULL) return;
    g_input_events++;

    if (strcmp(txt, "OK") == 0) {
        ui_login_submit();
    } else if (strcmp(txt, "Del") == 0) {
        lv_textarea_del_char(g_password_ta);
    } else {
        if (strlen(lv_textarea_get_text(g_password_ta)) >= 8U) {
            g_discarded_events++;
            return;
        }
        lv_textarea_add_text(g_password_ta, txt);
    }
}

void ui_create_login(void)
{
    static const char *key_map[] = {
        "1", "2", "3", "\n",
        "4", "5", "6", "\n",
        "7", "8", "9", "\n",
        "Del", "0", "OK", ""
    };

    lv_obj_t *scr = ui_create_screen();
    g_login_active = true;
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xE8EEF5), 0);

    lv_obj_t *panel = lv_obj_create(scr);
    lv_obj_set_size(panel, 216, 292);
    lv_obj_center(panel);
    lv_obj_set_style_radius(panel, 8, 0);
    lv_obj_set_style_bg_color(panel, lv_color_white(), 0);
    lv_obj_set_style_border_color(panel, lv_color_hex(0xC9D4E2), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(panel);
    lv_label_set_text(title, UI_TEXT_LOGIN_TITLE);
    lv_obj_set_style_text_font(title, UI_FONT, 0);
    lv_obj_set_style_text_color(title, lv_color_hex(0x1F6FD0), 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 12);

    g_password_ta = lv_textarea_create(panel);
    lv_obj_set_size(g_password_ta, 164, 36);
    lv_obj_align(g_password_ta, LV_ALIGN_TOP_MID, 0, 44);
    lv_textarea_set_one_line(g_password_ta, true);
    lv_textarea_set_password_mode(g_password_ta, true);
    lv_textarea_set_max_length(g_password_ta, 8);
    lv_textarea_set_accepted_chars(g_password_ta, "0123456789");
    lv_obj_set_style_text_font(g_password_ta, UI_FONT, 0);

    g_login_hint = lv_label_create(panel);
    lv_label_set_text(g_login_hint, UI_TEXT_LOGIN_HINT);
    lv_obj_set_style_text_font(g_login_hint, UI_FONT, 0);
    lv_obj_set_style_text_color(g_login_hint, lv_color_hex(0x6B7280), 0);
    lv_obj_align(g_login_hint, LV_ALIGN_TOP_MID, 0, 86);

    ui_button(panel, UI_TEXT_LOGIN_BUTTON, 68, 108, 80, 34, login_button_event_cb, NULL);

    lv_obj_t *keypad = lv_btnmatrix_create(panel);
    lv_btnmatrix_set_map(keypad, key_map);
    lv_obj_set_size(keypad, 184, 124);
    lv_obj_align(keypad, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_font(keypad, UI_FONT, LV_PART_ITEMS);
    lv_obj_set_style_radius(keypad, 7, LV_PART_ITEMS);
    lv_obj_add_event_cb(keypad, login_keypad_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_load_screen(scr);
}
