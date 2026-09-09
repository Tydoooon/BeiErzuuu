#include "app_ui_internal.h"

static void ui_draw_clear(void);
static void ui_draw_save(void);
static void ui_draw_save_named(const char *name);
static void ui_draw_open(void);
static void ui_draw_open_file(uint8_t slot);
static void ui_draw_next_tool(void);
static void ui_draw_next_color(void);
static void ui_draw_update_status(void);
static void ui_draw_show_save_dialog(void);
static void ui_draw_close_save_dialog(void);
static void ui_draw_show_open_dialog(void);
static void ui_draw_close_open_dialog(void);
static void ui_draw_refresh_open_selection(void);
static bool ui_draw_select_next_file(void);
static void ui_draw_set_saved_bit(uint32_t idx, bool value);
static bool ui_draw_get_saved_bit(uint32_t idx);
static lv_color_t ui_draw_color(void);
static const char *ui_draw_tool_name(void);
static const char *ui_draw_color_name(void);
static lv_coord_t ui_abs_coord(lv_coord_t value);
static int32_t ui_abs_i32(int32_t value);
static lv_coord_t ui_min_coord(lv_coord_t a, lv_coord_t b);
static void ui_draw_line(lv_obj_t *canvas, lv_point_t p1, lv_point_t p2);
static void ui_draw_plot_px(lv_coord_t x, lv_coord_t y, lv_color_t color, lv_area_t *dirty);
static void ui_draw_invalidate_dirty(lv_obj_t *canvas, lv_area_t *dirty);
static void ui_draw_rect(lv_obj_t *canvas, lv_point_t p1, lv_point_t p2);
static void ui_draw_circle(lv_obj_t *canvas, lv_point_t p1, lv_point_t p2);
static void draw_save_submit_event_cb(lv_event_t *e);
static void draw_save_keypad_event_cb(lv_event_t *e);
static void draw_open_select_event_cb(lv_event_t *e);
static void draw_open_cancel_event_cb(lv_event_t *e);

typedef enum {
    UI_DRAW_TOOL_PEN = 0,
    UI_DRAW_TOOL_LINE,
    UI_DRAW_TOOL_RECT,
    UI_DRAW_TOOL_CIRCLE,
    UI_DRAW_TOOL_COUNT,
} ui_draw_tool_t;

typedef enum {
    UI_DRAW_COLOR_BLACK = 0,
    UI_DRAW_COLOR_RED,
    UI_DRAW_COLOR_BLUE,
    UI_DRAW_COLOR_GREEN,
    UI_DRAW_COLOR_COUNT,
} ui_draw_color_t;

static ui_draw_tool_t g_draw_tool = UI_DRAW_TOOL_PEN;
static ui_draw_color_t g_draw_color = UI_DRAW_COLOR_BLACK;
static lv_point_t g_draw_shape_start;
static bool g_draw_has_shape_start;
static lv_obj_t *g_draw_save_dialog;
static lv_obj_t *g_draw_save_ta;
static lv_obj_t *g_draw_save_keyboard;
static lv_obj_t *g_draw_open_dialog;
static lv_obj_t *g_draw_open_list;
static lv_obj_t *g_draw_open_buttons[UI_FILE_MAX];
static uint8_t g_draw_open_selected = UI_FILE_MAX;

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
        g_draw_shape_start = p;
        g_draw_has_shape_start = true;
        g_input_events++;
    }

    if (g_draw_tool == UI_DRAW_TOOL_PEN &&
        (code == LV_EVENT_PRESSING || code == LV_EVENT_PRESSED)) {
        ui_draw_line(canvas, has_last ? last : p, p);

        last = p;
        has_last = true;
    } else if (code == LV_EVENT_RELEASED && g_draw_has_shape_start) {
        if (g_draw_tool == UI_DRAW_TOOL_LINE) {
            ui_draw_line(canvas, g_draw_shape_start, p);
        } else if (g_draw_tool == UI_DRAW_TOOL_RECT) {
            ui_draw_rect(canvas, g_draw_shape_start, p);
        } else if (g_draw_tool == UI_DRAW_TOOL_CIRCLE) {
            ui_draw_circle(canvas, g_draw_shape_start, p);
        }
        g_draw_has_shape_start = false;
        has_last = false;
    }
}

static void draw_action_event_cb(lv_event_t *e)
{
    uint8_t action = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    g_input_events++;
    if (action == 0) ui_draw_clear();
    else if (action == 1) ui_draw_save();
    else if (action == 2) ui_draw_open();
    else if (action == 3) ui_draw_next_tool();
    else ui_draw_next_color();
}

void ui_create_draw_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_DRAW);

    g_draw_save_dialog = NULL;
    g_draw_save_ta = NULL;
    g_draw_save_keyboard = NULL;
    g_draw_open_dialog = NULL;
    g_draw_open_list = NULL;
    memset(g_draw_open_buttons, 0, sizeof(g_draw_open_buttons));
    g_draw_open_selected = UI_FILE_MAX;
    g_draw_has_shape_start = false;

    g_canvas = lv_canvas_create(scr);
    lv_canvas_set_buffer(g_canvas, g_canvas_buf, UI_DRAW_W, UI_DRAW_H, LV_IMG_CF_TRUE_COLOR);
    lv_obj_set_size(g_canvas, UI_DRAW_W, UI_DRAW_H);
    lv_obj_align(g_canvas, LV_ALIGN_TOP_MID, 0, 52);
    lv_obj_add_flag(g_canvas, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_border_width(g_canvas, 1, 0);
    lv_obj_set_style_border_color(g_canvas, lv_color_hex(0x94A3B8), 0);
    lv_obj_add_event_cb(g_canvas, draw_canvas_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(g_canvas, draw_canvas_event_cb, LV_EVENT_PRESSING, NULL);
    lv_obj_add_event_cb(g_canvas, draw_canvas_event_cb, LV_EVENT_RELEASED, NULL);
    lv_canvas_fill_bg(g_canvas, lv_color_white(), LV_OPA_COVER);

    g_draw_status = ui_label(scr, "", 30, 164);
    ui_draw_update_status();

    ui_button(scr, UI_TEXT_DRAW_CLEAR, 24, 194, 58, 30, draw_action_event_cb, (void *)0);
    ui_button(scr, UI_TEXT_DRAW_SAVE, 91, 194, 58, 30, draw_action_event_cb, (void *)1);
    ui_button(scr, UI_TEXT_DRAW_OPEN, 158, 194, 58, 30, draw_action_event_cb, (void *)2);
    ui_button(scr, "Tool", 44, 236, 64, 30, draw_action_event_cb, (void *)3);
    ui_button(scr, "Color", 132, 236, 64, 30, draw_action_event_cb, (void *)4);
    ui_label(scr, "K1 Tool  K2 Color  K3 Clear  K4 Exit  K5 Open", 8, 286);

    ui_load_screen(scr);
}

void ui_draw_handle_key(uint8_t key_id)
{
    if (g_draw_save_dialog != NULL) {
        if (key_id == 4U) {
            ui_draw_close_save_dialog();
        }
        return;
    }

    if (g_draw_open_dialog != NULL) {
        if (key_id == 1U) {
            ui_draw_select_next_file();
        } else if (key_id == 3U && g_draw_open_selected < UI_FILE_MAX) {
            ui_draw_open_file(g_draw_open_selected);
            ui_draw_close_open_dialog();
        } else if (key_id == 4U) {
            ui_draw_close_open_dialog();
        }
        return;
    }

    if (key_id == 1U) {
        ui_draw_next_tool();
    } else if (key_id == 2U) {
        ui_draw_next_color();
    } else if (key_id == 3U) {
        ui_draw_clear();
    } else if (key_id == 4U) {
        ui_create_main();
    } else if (key_id == 5U) {
        ui_draw_open();
    }
}

static void ui_draw_open(void)
{
    ui_draw_show_open_dialog();
}

static void ui_draw_open_file(uint8_t slot)
{
    if (g_canvas == NULL || !ui_file_is_draw_file(slot)) {
        if (g_draw_status != NULL) {
            lv_label_set_text(g_draw_status, "Open failed");
        }
        return;
    }

    memcpy(g_draw_save, g_draw_slots[slot], UI_DRAW_SAVE_BYTES);
    lv_canvas_fill_bg(g_canvas, lv_color_white(), LV_OPA_COVER);
    for (uint32_t i = 0; i < (uint32_t)UI_DRAW_W * UI_DRAW_H; i++) {
        if (ui_draw_get_saved_bit(i)) {
            lv_canvas_set_px_color(g_canvas, i % UI_DRAW_W, i / UI_DRAW_W, lv_color_black());
        }
    }

    lv_label_set_text_fmt(g_draw_status, "Opened %s", g_files[slot].name);
    ui_log(UI_TEXT_DRAW_LOG_OPEN);
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
    ui_draw_show_save_dialog();
}

static void ui_draw_save_named(const char *name)
{
    if (g_canvas == NULL) return;

    memset(g_draw_save, 0, sizeof(g_draw_save));
    for (uint32_t i = 0; i < (uint32_t)UI_DRAW_W * UI_DRAW_H; i++) {
        lv_color_t px = lv_canvas_get_px(g_canvas, i % UI_DRAW_W, i / UI_DRAW_W);
        if (px.full != lv_color_white().full) {
            ui_draw_set_saved_bit(i, true);
        }
    }

    if (ui_file_save_draw_snapshot(name, UI_DRAW_SAVE_BYTES)) {
        lv_label_set_text(g_draw_status, "Saving...");
    } else {
        lv_label_set_text(g_draw_status, "Save failed");
    }
}

static void ui_draw_next_tool(void)
{
    g_draw_tool = (ui_draw_tool_t)(((uint8_t)g_draw_tool + 1U) % UI_DRAW_TOOL_COUNT);
    ui_draw_update_status();
}

static void ui_draw_next_color(void)
{
    g_draw_color = (ui_draw_color_t)(((uint8_t)g_draw_color + 1U) % UI_DRAW_COLOR_COUNT);
    ui_draw_update_status();
}

static void ui_draw_update_status(void)
{
    if (g_draw_status != NULL) {
        lv_label_set_text_fmt(g_draw_status, "Tool:%s  Color:%s",
                              ui_draw_tool_name(), ui_draw_color_name());
    }
}

static void ui_draw_show_save_dialog(void)
{
    static const char * const name_key_map[] = {
        "a", "b", "c", "d", "e", "f", "\n",
        "g", "h", "i", "j", "k", "l", "\n",
        "m", "n", "o", "p", "q", "r", "\n",
        "s", "t", "u", "v", "w", "x", "\n",
        "y", "z", "0", "1", "2", "3", "\n",
        "4", "5", "6", "7", "8", "9", "\n",
        "Bksp", "_", "-", ".", "OK", "Cancel", ""
    };
    lv_obj_t *scr = lv_scr_act();

    if (scr == NULL || g_draw_save_dialog != NULL) return;

    g_draw_save_dialog = lv_obj_create(scr);
    lv_obj_set_size(g_draw_save_dialog, 216, 268);
    lv_obj_align(g_draw_save_dialog, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(g_draw_save_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_color(g_draw_save_dialog, lv_color_hex(0x94A3B8), 0);
    lv_obj_clear_flag(g_draw_save_dialog, LV_OBJ_FLAG_SCROLLABLE);

    ui_label(g_draw_save_dialog, "Drawing name", 10, 0);
    g_draw_save_ta = lv_textarea_create(g_draw_save_dialog);
    lv_obj_set_size(g_draw_save_ta, 196, 34);
    lv_obj_align(g_draw_save_ta, LV_ALIGN_TOP_LEFT, 0, 19);
    lv_textarea_set_one_line(g_draw_save_ta, true);
    lv_textarea_set_max_length(g_draw_save_ta, UI_FILE_NAME_LEN - 5U);
    lv_textarea_set_accepted_chars(g_draw_save_ta, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.");

    g_draw_save_keyboard = lv_btnmatrix_create(g_draw_save_dialog);
    lv_btnmatrix_set_map(g_draw_save_keyboard, (const char **)name_key_map);
    lv_obj_set_size(g_draw_save_keyboard, 196, 184);
    lv_obj_align(g_draw_save_keyboard, LV_ALIGN_BOTTOM_MID, 0, 14);
    lv_obj_set_style_text_font(g_draw_save_keyboard, UI_FONT, LV_PART_ITEMS);
    lv_obj_set_style_radius(g_draw_save_keyboard, 5, LV_PART_ITEMS);
    lv_obj_add_event_cb(g_draw_save_keyboard, draw_save_keypad_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void ui_draw_close_save_dialog(void)
{
    if (g_draw_save_dialog != NULL) {
        lv_obj_del(g_draw_save_dialog);
        g_draw_save_dialog = NULL;
        g_draw_save_ta = NULL;
        g_draw_save_keyboard = NULL;
    }
}

static void ui_draw_show_open_dialog(void)
{
    lv_obj_t *scr = lv_scr_act();
    uint8_t count = 0;

    if (scr == NULL || g_draw_open_dialog != NULL) return;

    g_draw_open_dialog = lv_obj_create(scr);
    lv_obj_set_size(g_draw_open_dialog, 216, 232);
    lv_obj_align(g_draw_open_dialog, LV_ALIGN_TOP_MID, 0, 44);
    lv_obj_set_style_bg_color(g_draw_open_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_color(g_draw_open_dialog, lv_color_hex(0x94A3B8), 0);
    lv_obj_clear_flag(g_draw_open_dialog, LV_OBJ_FLAG_SCROLLABLE);

    ui_label(g_draw_open_dialog, "Open .drw", 10, 6);

    g_draw_open_list = lv_list_create(g_draw_open_dialog);
    lv_obj_set_size(g_draw_open_list, 196, 146);
    lv_obj_align(g_draw_open_list, LV_ALIGN_TOP_LEFT, 10, 30);
    lv_obj_set_style_text_font(g_draw_open_list, UI_FONT, 0);
    memset(g_draw_open_buttons, 0, sizeof(g_draw_open_buttons));
    g_draw_open_selected = UI_FILE_MAX;

    for (uint8_t i = 0; i < UI_FILE_MAX; i++) {
        if (!ui_file_is_draw_file(i)) continue;

        lv_obj_t *btn = lv_list_add_btn(g_draw_open_list, NULL, g_files[i].name);
        lv_obj_set_style_text_font(btn, UI_FONT, 0);
        lv_obj_add_event_cb(btn, draw_open_select_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        g_draw_open_buttons[i] = btn;
        if (g_draw_open_selected >= UI_FILE_MAX) {
            g_draw_open_selected = i;
        }
        count++;
    }

    if (count == 0U) {
        ui_label(g_draw_open_list, "No .drw file", 8, 8);
    }

    ui_button(g_draw_open_dialog, "Cancel", 68, 184, 80, 30, draw_open_cancel_event_cb, NULL);
    ui_label(g_draw_open_dialog, "K1 Next  K3 Open  K4 Cancel", 8, 214);
    ui_draw_refresh_open_selection();
}

static void ui_draw_close_open_dialog(void)
{
    if (g_draw_open_dialog != NULL) {
        lv_obj_del(g_draw_open_dialog);
        g_draw_open_dialog = NULL;
        g_draw_open_list = NULL;
        memset(g_draw_open_buttons, 0, sizeof(g_draw_open_buttons));
        g_draw_open_selected = UI_FILE_MAX;
    }
}

static void ui_draw_refresh_open_selection(void)
{
    for (uint8_t i = 0; i < UI_FILE_MAX; i++) {
        if (g_draw_open_buttons[i] == NULL) continue;

        lv_obj_set_style_bg_color(g_draw_open_buttons[i],
                                  i == g_draw_open_selected ?
                                  lv_color_hex(0xF59E0B) : lv_color_white(),
                                  0);
        lv_obj_set_style_text_color(g_draw_open_buttons[i],
                                    i == g_draw_open_selected ?
                                    lv_color_white() : lv_color_hex(0x1F2937),
                                    0);
    }
}

static bool ui_draw_select_next_file(void)
{
    uint8_t start = g_draw_open_selected < UI_FILE_MAX ? g_draw_open_selected : 0U;
    uint8_t idx = start;

    for (uint8_t step = 0; step < UI_FILE_MAX; step++) {
        idx = (uint8_t)((idx + 1U) % UI_FILE_MAX);
        if (ui_file_is_draw_file(idx)) {
            g_draw_open_selected = idx;
            ui_draw_refresh_open_selection();
            return true;
        }
    }

    return false;
}

static lv_color_t ui_draw_color(void)
{
    switch (g_draw_color) {
    case UI_DRAW_COLOR_RED: return lv_color_hex(0xE11D48);
    case UI_DRAW_COLOR_BLUE: return lv_color_hex(0x2563EB);
    case UI_DRAW_COLOR_GREEN: return lv_color_hex(0x16A34A);
    default: return lv_color_black();
    }
}

static const char *ui_draw_tool_name(void)
{
    switch (g_draw_tool) {
    case UI_DRAW_TOOL_LINE: return "Line";
    case UI_DRAW_TOOL_RECT: return "Rect";
    case UI_DRAW_TOOL_CIRCLE: return "Circle";
    default: return "Pen";
    }
}

static const char *ui_draw_color_name(void)
{
    switch (g_draw_color) {
    case UI_DRAW_COLOR_RED: return "Red";
    case UI_DRAW_COLOR_BLUE: return "Blue";
    case UI_DRAW_COLOR_GREEN: return "Green";
    default: return "Black";
    }
}

static lv_coord_t ui_abs_coord(lv_coord_t value)
{
    return value < 0 ? (lv_coord_t)-value : value;
}

static int32_t ui_abs_i32(int32_t value)
{
    return value < 0 ? -value : value;
}

static lv_coord_t ui_min_coord(lv_coord_t a, lv_coord_t b)
{
    return a < b ? a : b;
}

static void ui_draw_line(lv_obj_t *canvas, lv_point_t p1, lv_point_t p2)
{
    int32_t x0 = p1.x;
    int32_t y0 = p1.y;
    int32_t x1 = p2.x;
    int32_t y1 = p2.y;
    int32_t dx = ui_abs_i32(x1 - x0);
    int32_t dy = -ui_abs_i32(y1 - y0);
    int32_t sx = x0 < x1 ? 1 : -1;
    int32_t sy = y0 < y1 ? 1 : -1;
    int32_t err = dx + dy;
    lv_color_t color = ui_draw_color();
    lv_area_t dirty = { UI_DRAW_W, UI_DRAW_H, -1, -1 };

    for (;;) {
        for (lv_coord_t oy = -1; oy <= 1; oy++) {
            for (lv_coord_t ox = -1; ox <= 1; ox++) {
                ui_draw_plot_px((lv_coord_t)x0 + ox, (lv_coord_t)y0 + oy, color, &dirty);
            }
        }

        if (x0 == x1 && y0 == y1) break;

        int32_t e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }

    ui_draw_invalidate_dirty(canvas, &dirty);
}

static void ui_draw_plot_px(lv_coord_t x, lv_coord_t y, lv_color_t color, lv_area_t *dirty)
{
    if (x < 0 || y < 0 || x >= UI_DRAW_W || y >= UI_DRAW_H) return;

    g_canvas_buf[(uint32_t)y * UI_DRAW_W + (uint32_t)x] = color;
    if (x < dirty->x1) dirty->x1 = x;
    if (y < dirty->y1) dirty->y1 = y;
    if (x > dirty->x2) dirty->x2 = x;
    if (y > dirty->y2) dirty->y2 = y;
}

static void ui_draw_invalidate_dirty(lv_obj_t *canvas, lv_area_t *dirty)
{
    lv_area_t coords;

    if (canvas == NULL || dirty->x2 < dirty->x1 || dirty->y2 < dirty->y1) return;

    lv_obj_get_coords(canvas, &coords);
    dirty->x1 += coords.x1;
    dirty->y1 += coords.y1;
    dirty->x2 += coords.x1;
    dirty->y2 += coords.y1;
    lv_obj_invalidate_area(canvas, dirty);
}

static void ui_draw_rect(lv_obj_t *canvas, lv_point_t p1, lv_point_t p2)
{
    lv_draw_rect_dsc_t dsc;

    lv_draw_rect_dsc_init(&dsc);
    dsc.bg_opa = LV_OPA_TRANSP;
    dsc.border_color = ui_draw_color();
    dsc.border_width = 3;
    dsc.border_opa = LV_OPA_COVER;
    dsc.radius = 0;
    lv_canvas_draw_rect(canvas,
                        ui_min_coord(p1.x, p2.x),
                        ui_min_coord(p1.y, p2.y),
                        ui_abs_coord((lv_coord_t)(p2.x - p1.x)) + 1,
                        ui_abs_coord((lv_coord_t)(p2.y - p1.y)) + 1,
                        &dsc);
}

static void ui_draw_circle(lv_obj_t *canvas, lv_point_t p1, lv_point_t p2)
{
    lv_draw_arc_dsc_t dsc;
    lv_coord_t w = ui_abs_coord((lv_coord_t)(p2.x - p1.x));
    lv_coord_t h = ui_abs_coord((lv_coord_t)(p2.y - p1.y));
    lv_coord_t r = (w > h ? w : h) / 2;
    lv_coord_t cx = ui_min_coord(p1.x, p2.x) + w / 2;
    lv_coord_t cy = ui_min_coord(p1.y, p2.y) + h / 2;

    if (r < 2) return;

    lv_draw_arc_dsc_init(&dsc);
    dsc.color = ui_draw_color();
    dsc.width = 3;
    dsc.opa = LV_OPA_COVER;
    lv_canvas_draw_arc(canvas, cx, cy, r, 0, 360, &dsc);
}

static void draw_save_submit_event_cb(lv_event_t *e)
{
    const char *name;

    (void)e;
    if (g_draw_save_ta == NULL) return;

    name = lv_textarea_get_text(g_draw_save_ta);
    ui_draw_save_named((name != NULL && name[0] != '\0') ? name : "drawing");
    ui_draw_close_save_dialog();
}

static void draw_save_keypad_event_cb(lv_event_t *e)
{
    lv_obj_t *btnm = lv_event_get_target(e);
    uint16_t id = lv_btnmatrix_get_selected_btn(btnm);
    const char *txt = lv_btnmatrix_get_btn_text(btnm, id);

    if (txt == NULL || g_draw_save_ta == NULL) return;

    if (strcmp(txt, "OK") == 0) {
        draw_save_submit_event_cb(e);
    } else if (strcmp(txt, "Cancel") == 0) {
        ui_draw_close_save_dialog();
    } else if (strcmp(txt, "Bksp") == 0) {
        lv_textarea_del_char(g_draw_save_ta);
    } else {
        lv_textarea_add_text(g_draw_save_ta, txt);
    }
}

static void draw_open_select_event_cb(lv_event_t *e)
{
    uint8_t slot = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    g_draw_open_selected = slot;
    ui_draw_open_file(slot);
    ui_draw_close_open_dialog();
}

static void draw_open_cancel_event_cb(lv_event_t *e)
{
    (void)e;
    ui_draw_close_open_dialog();
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
