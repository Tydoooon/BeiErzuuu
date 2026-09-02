#include "app_ui_internal.h"

static void sleep_event_cb(lv_event_t *e)
{
    (void)e;
    g_input_events++;
    app_ui_sleep_now();
}

void ui_load_screen(lv_obj_t *scr)
{
    lv_obj_t *old = lv_scr_act();

    if (g_monitor_timer != NULL) {
        lv_timer_del(g_monitor_timer);
        g_monitor_timer = NULL;
    }

    lv_scr_load(scr);
    if (old != NULL && old != scr) {
        lv_obj_del_async(old);
    }
}

lv_obj_t *ui_create_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xEEF3F8), 0);
    lv_obj_set_style_text_font(scr, UI_FONT, 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0x1F2937), 0);
    return scr;
}

lv_obj_t *ui_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x1F2937), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
    return label;
}

lv_obj_t *ui_button(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                    lv_coord_t w, lv_coord_t h, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, h);
    lv_obj_align(btn, LV_ALIGN_TOP_LEFT, x, y);
    lv_obj_set_style_radius(btn, 6, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2F80ED), 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1F6FD0), LV_STATE_PRESSED);

    lv_obj_t *label = lv_label_create(btn);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_center(label);

    if (cb != NULL) {
        lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    }

    return btn;
}

lv_obj_t *ui_add_sleep_button(lv_obj_t *parent, lv_coord_t x, lv_coord_t y)
{
    return ui_button(parent, UI_TEXT_SLEEP, x, y, 44, 28, sleep_event_cb, NULL);
}

static void back_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_music_playing) {
        g_music_playing = false;
        ui_log(UI_TEXT_BACK_MUSIC_STOP);
    }
    g_input_events++;
    ui_create_main();
}

lv_obj_t *ui_create_app_frame(const char *title)
{
    lv_obj_t *scr = ui_create_screen();

    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 240, 40);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0xEEF3F8), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    ui_button(bar, "<", 8, 0, 40, 28, back_event_cb, NULL);
    ui_add_sleep_button(bar, 188, 0);

    lv_obj_t *label = lv_label_create(bar);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x1F2937), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 60, 7);

    return scr;
}
