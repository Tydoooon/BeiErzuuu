#include "app_ui_internal.h"

#define UI_FILE_WHEEL_STEP 28

static void ui_refresh_file_list(void);
static void ui_refresh_file_detail(void);
static int ui_file_find(const char *name);
static int ui_file_find_free_slot(void);
static bool ui_file_name_has_ext(const char *name, const char *ext);
static void ui_file_make_name(const char *input, const char *ext, char *out, size_t out_size);
static bool ui_file_make_unique_name(const char *input, const char *ext, char *out, size_t out_size);
static bool ui_create_file_named(const char *input);
static bool ui_rename_file(const char *input);
static void ui_create_file(void);
static void ui_delete_file(void);
static void ui_save_file(void);
static void ui_open_file(void);
static void ui_show_file_name_dialog(const char *title);
static void ui_close_file_name_dialog(void);
static void ui_show_rename_dialog(void);
static void ui_show_file_context_menu(int16_t x, int16_t y);
static void ui_close_file_context_menu(void);
static void ui_show_file_open_dialog(void);
static void ui_close_file_open_dialog(void);
static void file_name_submit_event_cb(lv_event_t *e);
static void file_name_keypad_event_cb(lv_event_t *e);
static void file_menu_event_cb(lv_event_t *e);
static void file_open_close_event_cb(lv_event_t *e);

typedef enum {
    UI_FILE_DIALOG_CREATE = 0,
    UI_FILE_DIALOG_RENAME,
} ui_file_dialog_mode_t;

static ui_file_dialog_mode_t g_file_dialog_mode;
static lv_obj_t *g_file_name_dialog;
static lv_obj_t *g_file_name_ta;
static lv_obj_t *g_file_name_keyboard;
static lv_obj_t *g_file_context_menu;
static lv_obj_t *g_file_open_dialog;

static void file_select_event_cb(lv_event_t *e)
{
    g_selected_file = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    g_input_events++;
    ui_refresh_file_detail();
}

static void file_menu_event_cb(lv_event_t *e)
{
    uint8_t action = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    g_input_events++;
    ui_close_file_context_menu();

    switch (action) {
    case 0: ui_create_file(); break;
    case 1: ui_open_file(); break;
    case 2: ui_show_rename_dialog(); break;
    case 3: ui_delete_file(); break;
    case 4: ui_save_file(); break;
    default: break;
    }

    ui_refresh_file_list();
    ui_refresh_file_detail();
}

void ui_create_file_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_FILE);

    g_file_name_dialog = NULL;
    g_file_name_ta = NULL;
    g_file_name_keyboard = NULL;
    g_file_context_menu = NULL;
    g_file_open_dialog = NULL;
    g_file_dialog_mode = UI_FILE_DIALOG_CREATE;

    g_file_list = lv_list_create(scr);
    lv_obj_set_size(g_file_list, 216, 190);
    lv_obj_align(g_file_list, LV_ALIGN_TOP_LEFT, 12, 44);
    lv_obj_set_style_text_font(g_file_list, UI_FONT, 0);
    lv_obj_set_scroll_dir(g_file_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_file_list, LV_SCROLLBAR_MODE_AUTO);

    g_file_detail = ui_label(scr, "", 12, 238);
    lv_obj_set_width(g_file_detail, 216);

    ui_refresh_file_list();
    ui_refresh_file_detail();
    ui_load_screen(scr);
}

void ui_file_handle_key(uint8_t key_id)
{
    if (g_file_name_dialog != NULL) {
        if (key_id == 4U) {
            ui_close_file_name_dialog();
        }
        return;
    }
    if (g_file_open_dialog != NULL) {
        if (key_id == 4U) {
            ui_close_file_open_dialog();
        }
        return;
    }
    if (g_file_context_menu != NULL) {
        if (key_id == 4U) {
            ui_close_file_context_menu();
        }
        return;
    }

    if (key_id == 1U) {
        uint8_t start = g_selected_file;
        do {
            g_selected_file = (uint8_t)((g_selected_file + 1U) % UI_FILE_MAX);
            if (g_files[g_selected_file].state == UI_FILE_STATE_NORMAL) {
                ui_refresh_file_detail();
                return;
            }
        } while (g_selected_file != start);
    } else if (key_id == 2U) {
        ui_create_file();
    } else if (key_id == 3U) {
        ui_open_file();
    } else if (key_id == 4U) {
        ui_create_main();
    } else if (key_id == 5U) {
        ui_save_file();
        ui_refresh_file_list();
        ui_refresh_file_detail();
    }
}

void ui_file_handle_wheel(int16_t wheel_delta)
{
    if (g_file_list == NULL || wheel_delta == 0) {
        return;
    }

    lv_obj_scroll_by_bounded(g_file_list, 0, (lv_coord_t)wheel_delta * UI_FILE_WHEEL_STEP, LV_ANIM_OFF);
}

void ui_file_handle_right_click(int16_t x, int16_t y)
{
    if (g_file_name_dialog != NULL || g_file_open_dialog != NULL) {
        return;
    }

    ui_show_file_context_menu(x, y);
}

bool ui_file_save_draw_snapshot(const char *name, uint32_t size)
{
    int slot;
    char file_name[UI_FILE_NAME_LEN];

    ui_file_make_name(name, ".drw", file_name, sizeof(file_name));
    slot = ui_file_find(file_name);
    if (slot < 0) {
        slot = ui_file_find_free_slot();
    }
    if (slot < 0) {
        g_error_events++;
        ui_log(UI_TEXT_FILE_TABLE_FULL);
        return false;
    }

    snprintf(g_files[slot].name, sizeof(g_files[slot].name), "%s", file_name);
    snprintf(g_files[slot].type, sizeof(g_files[slot].type), "draw");
    snprintf(g_files[slot].content, sizeof(g_files[slot].content), "Drawing snapshot");
    g_files[slot].size = size;
    g_files[slot].storage_addr = APP_W25Q128_DRAW_START + (uint32_t)slot * APP_W25Q128_DRAW_SLOT_SIZE;
    g_files[slot].state = UI_FILE_STATE_NORMAL;
    memcpy(g_draw_slots[slot], g_draw_save, UI_DRAW_SAVE_BYTES);
    g_selected_file = (uint8_t)slot;

    if (!ui_storage_save_draw_slot((uint8_t)slot)) {
        g_error_events++;
        ui_log("Draw flash save failed");
        return false;
    }

    ui_storage_mark_dirty();
    ui_storage_commit();
    if (g_storage_dirty) {
        g_error_events++;
        ui_log("File table save failed");
        return false;
    }

    ui_log(UI_TEXT_DRAW_LOG_SAVE);
    return true;
}

bool ui_file_is_draw_file(uint8_t slot)
{
    if (slot >= UI_FILE_MAX || g_files[slot].state != UI_FILE_STATE_NORMAL) {
        return false;
    }

    return strcmp(g_files[slot].type, "draw") == 0 ||
           ui_file_name_has_ext(g_files[slot].name, ".drw");
}

static void ui_refresh_file_list(void)
{
    uint8_t active_count = 0;
    char text[80];

    if (g_file_list == NULL) return;
    lv_obj_clean(g_file_list);

    for (uint8_t i = 0; i < UI_FILE_MAX; i++) {
        if (g_files[i].state != UI_FILE_STATE_NORMAL) continue;

        snprintf(text, sizeof(text), "%.*s  %.*s  %luB",
                 UI_FILE_NAME_LEN - 1, g_files[i].name,
                 UI_FILE_TYPE_LEN - 1, g_files[i].type,
                 (unsigned long)g_files[i].size);
        lv_obj_t *btn = lv_list_add_btn(g_file_list, NULL, text);
        lv_obj_set_style_text_font(btn, UI_FONT, 0);
        lv_obj_add_event_cb(btn, file_select_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
        active_count++;
    }

    if (active_count == 0) {
        ui_label(g_file_list, UI_TEXT_FILE_EMPTY, 8, 8);
    }
}

static void ui_refresh_file_detail(void)
{
    char text[224];
    ui_file_entry_t *file = &g_files[g_selected_file];

    if (g_file_detail == NULL) return;

    if (file->state != UI_FILE_STATE_NORMAL) {
        lv_label_set_text(g_file_detail, UI_TEXT_FILE_NONE);
        return;
    }

    snprintf(text, sizeof(text), UI_TEXT_FILE_DETAIL_FMT,
             file->name, file->type, (unsigned long)file->size,
             (unsigned long)file->storage_addr, file->content);
    lv_label_set_text(g_file_detail, text);
}

static int ui_file_find(const char *name)
{
    for (uint8_t i = 0; i < UI_FILE_MAX; i++) {
        if (g_files[i].state == UI_FILE_STATE_NORMAL && strcmp(g_files[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static int ui_file_find_free_slot(void)
{
    for (uint8_t i = 0; i < UI_FILE_MAX; i++) {
        if (g_files[i].state != UI_FILE_STATE_NORMAL) return i;
    }
    return -1;
}

static bool ui_file_name_has_ext(const char *name, const char *ext)
{
    size_t name_len;
    size_t ext_len;

    if (name == NULL || ext == NULL) return false;

    name_len = strnlen(name, UI_FILE_NAME_LEN);
    ext_len = strlen(ext);
    if (name_len < ext_len) return false;

    return strncmp(name + name_len - ext_len, ext, ext_len) == 0;
}

static void ui_file_make_name(const char *input, const char *ext, char *out, size_t out_size)
{
    char base[UI_FILE_NAME_LEN];
    size_t base_len;

    if (out == NULL || out_size == 0U) return;

    if (input == NULL || input[0] == '\0') {
        snprintf(base, sizeof(base), "note%u", (unsigned)g_file_serial++);
    } else {
        snprintf(base, sizeof(base), "%s", input);
    }

    base_len = strlen(base);
    while (base_len > 0U && base[base_len - 1U] == ' ') {
        base[--base_len] = '\0';
    }

    if (strchr(base, '.') != NULL || ext == NULL) {
        snprintf(out, out_size, "%s", base);
    } else {
        snprintf(out, out_size, "%s%s", base, ext);
    }
}

static bool ui_file_make_unique_name(const char *input, const char *ext, char *out, size_t out_size)
{
    char candidate[UI_FILE_NAME_LEN];
    char stem[UI_FILE_NAME_LEN];
    char ext_part[UI_FILE_NAME_LEN];
    char suffix[8];
    const char *dot;
    size_t stem_len;
    size_t ext_len;

    if (out == NULL || out_size == 0U) return false;

    ui_file_make_name(input, ext, candidate, sizeof(candidate));
    if (ui_file_find(candidate) < 0) {
        snprintf(out, out_size, "%s", candidate);
        return true;
    }

    dot = strrchr(candidate, '.');
    if (dot != NULL && dot != candidate) {
        stem_len = (size_t)(dot - candidate);
        snprintf(ext_part, sizeof(ext_part), "%s", dot);
    } else {
        stem_len = strlen(candidate);
        ext_part[0] = '\0';
    }

    if (stem_len >= sizeof(stem)) {
        stem_len = sizeof(stem) - 1U;
    }
    memcpy(stem, candidate, stem_len);
    stem[stem_len] = '\0';

    for (uint16_t index = 1U; index < 1000U; index++) {
        size_t max_stem_len;
        size_t copy_len;
        size_t suffix_len;

        snprintf(suffix, sizeof(suffix), "%u", (unsigned)index);
        suffix_len = strlen(suffix);
        ext_len = strlen(ext_part);
        if ((suffix_len + ext_len + 1U) >= out_size) {
            return false;
        }

        max_stem_len = out_size - suffix_len - ext_len - 1U;
        copy_len = strlen(stem);
        if (copy_len > max_stem_len) {
            copy_len = max_stem_len;
        }
        memcpy(candidate, stem, copy_len);
        memcpy(candidate + copy_len, suffix, suffix_len);
        memcpy(candidate + copy_len + suffix_len, ext_part, ext_len);
        candidate[copy_len + suffix_len + ext_len] = '\0';

        if (ui_file_find(candidate) < 0) {
            snprintf(out, out_size, "%s", candidate);
            return true;
        }
    }

    return false;
}

static bool ui_create_file_named(const char *input)
{
    int slot = ui_file_find_free_slot();
    char name[UI_FILE_NAME_LEN];

    if (slot < 0) {
        g_error_events++;
        ui_log(UI_TEXT_FILE_TABLE_FULL);
        return false;
    }

    if (!ui_file_make_unique_name(input, ".txt", name, sizeof(name))) {
        g_error_events++;
        ui_log(UI_TEXT_FILE_NAME_DUP);
        return false;
    }

    snprintf(g_files[slot].name, sizeof(g_files[slot].name), "%s", name);
    snprintf(g_files[slot].type, sizeof(g_files[slot].type), "txt");
    snprintf(g_files[slot].content, sizeof(g_files[slot].content), "New file.");
    g_files[slot].size = strlen(g_files[slot].content);
    g_files[slot].storage_addr = APP_W25Q128_FILE_DATA_START + (uint32_t)slot * UI_STORAGE_SLOT_SIZE;
    g_files[slot].state = UI_FILE_STATE_NORMAL;
    g_selected_file = (uint8_t)slot;
    ui_storage_mark_dirty();
    ui_storage_commit();
    ui_log(UI_TEXT_FILE_CREATED);
    return true;
}

static void ui_create_file(void)
{
    g_file_dialog_mode = UI_FILE_DIALOG_CREATE;
    ui_show_file_name_dialog("File name");
}

static bool ui_rename_file(const char *input)
{
    ui_file_entry_t *file = &g_files[g_selected_file];
    char name[UI_FILE_NAME_LEN];

    if (file->state != UI_FILE_STATE_NORMAL) return false;
    if (input == NULL || input[0] == '\0') return false;

    ui_file_make_name(input, NULL, name, sizeof(name));
    if (name[0] == '\0') return false;

    if (strcmp(file->name, name) == 0) return true;

    for (uint8_t i = 0; i < UI_FILE_MAX; i++) {
        if (i == g_selected_file) continue;
        if (g_files[i].state == UI_FILE_STATE_NORMAL && strcmp(g_files[i].name, name) == 0) {
            g_error_events++;
            ui_log(UI_TEXT_FILE_NAME_DUP);
            return false;
        }
    }

    snprintf(file->name, sizeof(file->name), "%s", name);
    ui_storage_mark_dirty();
    ui_storage_commit();
    ui_log(UI_TEXT_FILE_RENAMED);
    return true;
}

static void ui_show_rename_dialog(void)
{
    if (g_files[g_selected_file].state != UI_FILE_STATE_NORMAL) return;

    g_file_dialog_mode = UI_FILE_DIALOG_RENAME;
    ui_show_file_name_dialog(UI_TEXT_FILE_RENAME);
    if (g_file_name_ta != NULL) {
        lv_textarea_set_text(g_file_name_ta, g_files[g_selected_file].name);
    }
}

static void ui_delete_file(void)
{
    if (g_files[g_selected_file].state != UI_FILE_STATE_NORMAL) return;

    g_files[g_selected_file].state = UI_FILE_STATE_DELETED;
    ui_storage_mark_dirty();
    ui_storage_commit();
    ui_log(UI_TEXT_FILE_DELETED);
}

static void ui_save_file(void)
{
    ui_file_entry_t *file = &g_files[g_selected_file];

    if (file->state != UI_FILE_STATE_NORMAL) return;

    snprintf(file->content, sizeof(file->content), "Saved at %lus.", (unsigned long)(lv_tick_get() / 1000U));
    file->size = strlen(file->content);
    ui_storage_mark_dirty();
    ui_storage_commit();
    ui_log(UI_TEXT_FILE_SAVED);
}

static void ui_open_file(void)
{
    if (g_files[g_selected_file].state != UI_FILE_STATE_NORMAL) return;

    ui_log(UI_TEXT_FILE_OPENED);
    ui_show_file_open_dialog();
}

static void ui_show_file_context_menu(int16_t x, int16_t y)
{
    static const char * const menu_items[] = {
        UI_TEXT_FILE_NEW, UI_TEXT_FILE_OPEN, UI_TEXT_FILE_RENAME,
        UI_TEXT_FILE_DELETE, UI_TEXT_FILE_SAVE
    };
    lv_obj_t *scr = lv_scr_act();
    lv_coord_t mx;
    lv_coord_t my;

    if (scr == NULL) return;

    if (g_file_context_menu != NULL) {
        ui_close_file_context_menu();
        return;
    }

    mx = (lv_coord_t)x;
    my = (lv_coord_t)y;
    if (mx + 96 > 240) mx = 240 - 96;
    if (my + 178 > 320) my = 320 - 178;
    if (mx < 0) mx = 0;
    if (my < 0) my = 0;

    g_file_context_menu = lv_obj_create(scr);
    lv_obj_set_size(g_file_context_menu, 96, 178);
    lv_obj_align(g_file_context_menu, LV_ALIGN_TOP_LEFT, mx, my);
    lv_obj_set_style_bg_color(g_file_context_menu, lv_color_white(), 0);
    lv_obj_set_style_border_color(g_file_context_menu, lv_color_hex(0x94A3B8), 0);
    lv_obj_set_style_border_width(g_file_context_menu, 2, 0);
    lv_obj_clear_flag(g_file_context_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_pad_all(g_file_context_menu, 4, 0);
    lv_obj_move_foreground(g_file_context_menu);

    for (uint8_t i = 0; i < 5U; i++) {
        ui_button(g_file_context_menu, menu_items[i], 0, (lv_coord_t)(4 + i * 34), 88, 30,
                  file_menu_event_cb, (void *)(uintptr_t)i);
    }
}

static void ui_close_file_context_menu(void)
{
    if (g_file_context_menu != NULL) {
        lv_obj_del(g_file_context_menu);
        g_file_context_menu = NULL;
    }
}

static void ui_show_file_open_dialog(void)
{
    ui_file_entry_t *file = &g_files[g_selected_file];
    lv_obj_t *scr = lv_scr_act();
    lv_obj_t *label;
    char text[256];

    if (scr == NULL || g_file_open_dialog != NULL) return;
    if (file->state != UI_FILE_STATE_NORMAL) return;

    g_file_open_dialog = lv_obj_create(scr);
    lv_obj_set_size(g_file_open_dialog, 216, 200);
    lv_obj_align(g_file_open_dialog, LV_ALIGN_TOP_MID, 0, 56);
    lv_obj_set_style_bg_color(g_file_open_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_color(g_file_open_dialog, lv_color_hex(0x94A3B8), 0);
    lv_obj_clear_flag(g_file_open_dialog, LV_OBJ_FLAG_SCROLLABLE);

    snprintf(text, sizeof(text), UI_TEXT_FILE_DETAIL_FMT,
             file->name, file->type, (unsigned long)file->size,
             (unsigned long)file->storage_addr, file->content);
    label = lv_label_create(g_file_open_dialog);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_width(label, 196);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 10, 8);

    ui_button(g_file_open_dialog, UI_TEXT_FILE_CLOSE, 68, 162, 80, 30,
              file_open_close_event_cb, NULL);
}

static void file_open_close_event_cb(lv_event_t *e)
{
    (void)e;
    g_input_events++;
    ui_close_file_open_dialog();
}

static void ui_close_file_open_dialog(void)
{
    if (g_file_open_dialog != NULL) {
        lv_obj_del(g_file_open_dialog);
        g_file_open_dialog = NULL;
    }
}

static void ui_show_file_name_dialog(const char *title)
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

    if (scr == NULL || g_file_name_dialog != NULL) return;

    //"File name"
    g_file_name_dialog = lv_obj_create(scr);
    lv_obj_set_size(g_file_name_dialog, 216, 268);
    lv_obj_align(g_file_name_dialog, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_set_style_bg_color(g_file_name_dialog, lv_color_white(), 0);
    lv_obj_set_style_border_color(g_file_name_dialog, lv_color_hex(0x94A3B8), 0);
    lv_obj_clear_flag(g_file_name_dialog, LV_OBJ_FLAG_SCROLLABLE);

    //文本框
    ui_label(g_file_name_dialog, title, 10, 0);
    g_file_name_ta = lv_textarea_create(g_file_name_dialog);
    lv_obj_set_size(g_file_name_ta, 196, 34);
    lv_obj_align(g_file_name_ta, LV_ALIGN_TOP_LEFT, 0, 19);
    lv_textarea_set_one_line(g_file_name_ta, true);
    lv_textarea_set_max_length(g_file_name_ta,
                               g_file_dialog_mode == UI_FILE_DIALOG_RENAME
                                   ? UI_FILE_NAME_LEN - 1U
                                   : UI_FILE_NAME_LEN - 5U);
    lv_textarea_set_accepted_chars(g_file_name_ta, "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_-.");

    //键盘
    g_file_name_keyboard = lv_btnmatrix_create(g_file_name_dialog);
    lv_btnmatrix_set_map(g_file_name_keyboard, (const char **)name_key_map);
    lv_obj_set_size(g_file_name_keyboard, 196, 184);
    lv_obj_align(g_file_name_keyboard, LV_ALIGN_BOTTOM_MID, 0, 14);
    lv_obj_set_style_text_font(g_file_name_keyboard, UI_FONT, LV_PART_ITEMS);
    lv_obj_set_style_radius(g_file_name_keyboard, 5, LV_PART_ITEMS);
    lv_obj_add_event_cb(g_file_name_keyboard, file_name_keypad_event_cb, LV_EVENT_VALUE_CHANGED, NULL);
}

static void ui_close_file_name_dialog(void)
{
    if (g_file_name_dialog != NULL) {
        lv_obj_del(g_file_name_dialog);
        g_file_name_dialog = NULL;
        g_file_name_ta = NULL;
        g_file_name_keyboard = NULL;
    }
}

static void file_name_submit_event_cb(lv_event_t *e)
{
    bool ok;
    (void)e;

    if (g_file_name_ta == NULL) return;

    if (g_file_dialog_mode == UI_FILE_DIALOG_RENAME) {
        ok = ui_rename_file(lv_textarea_get_text(g_file_name_ta));
    } else {
        ok = ui_create_file_named(lv_textarea_get_text(g_file_name_ta));
    }

    if (ok) {
        ui_close_file_name_dialog();
        ui_refresh_file_list();
        ui_refresh_file_detail();
    }
}

static void file_name_keypad_event_cb(lv_event_t *e)
{
    lv_obj_t *btnm = lv_event_get_target(e);
    uint16_t id = lv_btnmatrix_get_selected_btn(btnm);
    const char *txt = lv_btnmatrix_get_btn_text(btnm, id);

    if (txt == NULL || g_file_name_ta == NULL) return;

    if (strcmp(txt, "OK") == 0) {
        file_name_submit_event_cb(e);
    } else if (strcmp(txt, "Cancel") == 0) {
        ui_close_file_name_dialog();
    } else if (strcmp(txt, "Bksp") == 0) {
        lv_textarea_del_char(g_file_name_ta);
    } else {
        lv_textarea_add_text(g_file_name_ta, txt);
    }
}
