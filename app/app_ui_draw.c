#include "app_ui_internal.h"

static void ui_draw_clear(void);
static void ui_draw_save(void);
static void ui_draw_open(void);
static void ui_draw_set_saved_bit(uint32_t idx, bool value);
static bool ui_draw_get_saved_bit(uint32_t idx);

static void draw_canvas_event_cb(lv_event_t *e)
{
    lv_indev_t *indev = lv_indev_get_act();
    lv_point_t p;
    lv_area_t area;
    static lv_point_t last;
    static bool has_last;
    lv_obj_t *canvas = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);

    if (indev == NULL) return;

    lv_indev_get_point(indev, &p);
    lv_obj_get_coords(canvas, &area);
    p.x -= area.x1;
    p.y -= area.y1;

    if (p.x < 0 || p.y < 0 || p.x >= UI_DRAW_W || p.y >= UI_DRAW_H) {
        has_last = false;
        return;
    }

    if (code == LV_EVENT_PRESSED) {
        has_last = false;
        g_input_events++;
    }

    if (code == LV_EVENT_PRESSING || code == LV_EVENT_PRESSED) {
        lv_draw_line_dsc_t dsc;
        lv_point_t pts[2];

        lv_draw_line_dsc_init(&dsc);
        dsc.color = lv_color_black();
        dsc.width = 3;
        dsc.round_start = 1;
        dsc.round_end = 1;

        pts[0] = has_last ? last : p;
        pts[1] = p;
        lv_canvas_draw_line(canvas, pts, 2, &dsc);

        last = p;
        has_last = true;
    }
}

static void draw_action_event_cb(lv_event_t *e)
{
    uint8_t action = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    g_input_events++;
    if (action == 0) ui_draw_clear();
    else if (action == 1) ui_draw_save();
    else ui_draw_open();
}

void ui_create_draw_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_DRAW);

    g_canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(g_canvas, g_canvas_buf, UI_DRAW_W, UI_DRAW_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_size(g_canvas, UI_DRAW_W, UI_DRAW_H);
    lv_obj_align(g_canvas, LV_ALIGN_TOP_MID, 0, 54);
    lv_obj_add_flag(g_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(g_canvas, 1, 0);
    lv_obj_set_style_border_color(g_canvas, lv_color_hex(0x94A3B8), 0);
    lv_obj_add_event_cb(g_canvas, draw_canvas_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_canvas, draw_canvas_event_cb, LV_EVENT_PRESSING, NULL);
    lv_canvas_fill_bg(g_canvas, lv_color_white(), LV_OPA_COVER);

    g_draw_status = ui_label(scr, UI_TEXT_DRAW_TIP, 42, 130);

    ui_button(scr, UI_TEXT_DRAW_CLEAR, 24, 170, 58, 34, draw_action_event_cb, (void *)0);
    ui_button(scr, UI_TEXT_DRAW_SAVE, 91, 170, 58, 34, draw_action_event_cb, (void *)1);
    ui_button(scr, UI_TEXT_DRAW_OPEN, 158, 170, 58, 34, draw_action_event_cb, (void *)2);
    ui_label(scr, UI_TEXT_DRAW_FORMAT, 32, 226);

    ui_load_screen(scr);
}

static void ui_draw_clear(void)
{
    if (g_canvas == NULL) return;
    lv_canvas_fill_bg(g_canvas, lv_color_white(), LV_OPA_COVER);
    lv_label_set_text(g_draw_status, UI_TEXT_DRAW_CLEARED);
    ui_log(UI_TEXT_DRAW_LOG_CLEAR);
}

static void ui_draw_save(void)
{
    if (g_canvas == NULL) return;

    memset(g_draw_save, 0, sizeof(g_draw_save));
    for (uint32_t i = 0; i < (uint32_t)UI_DRAW_W * UI_DRAW_H; i++) {
        lv_color_t px = lv_canvas_get_px(g_canvas, i % UI_DRAW_W, i / UI_DRAW_W);
        if (px.full != lv_color_white().full) {
            ui_draw_set_saved_bit(i, true);
        }
    }

    g_storage_dirty = true;
    ui_storage_commit();
    lv_label_set_text(g_draw_status, UI_TEXT_DRAW_SAVED);
    ui_log(UI_TEXT_DRAW_LOG_SAVE);
}

static void ui_draw_open(void)
{
    if (g_canvas == NULL) return;

    lv_canvas_fill_bg(g_canvas, lv_color_white(), LV_OPA_COVER);
    for (uint32_t i = 0; i < (uint32_t)UI_DRAW_W * UI_DRAW_H; i++) {
        if (ui_draw_get_saved_bit(i)) {
            lv_canvas_set_px_color(g_canvas, i % UI_DRAW_W, i / UI_DRAW_W, lv_color_black());
        }
    }

    lv_label_set_text(g_draw_status, UI_TEXT_DRAW_OPENED);
    ui_log(UI_TEXT_DRAW_LOG_OPEN);
}

static void ui_draw_set_saved_bit(uint32_t idx, bool value)
{
    uint32_t byte = idx / 8U;
    uint8_t bit = (uint8_t)(idx % 8U);

    if (value) g_draw_save[byte] |= (uint8_t)(1U << bit);
    else g_draw_save[byte] &= (uint8_t)~(1U << bit);
}

static bool ui_draw_get_saved_bit(uint32_t idx)
{
    return (g_draw_save[idx / 8U] & (uint8_t)(1U << (idx % 8U))) != 0U;
}
