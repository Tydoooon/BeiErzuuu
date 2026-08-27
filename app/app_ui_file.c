#include "app_ui_internal.h"

static void ui_refresh_file_list(void);
static void ui_refresh_file_detail(void);
static int ui_file_find(const char *name);
static int ui_file_find_free_slot(void);
static void ui_create_file(void);
static void ui_delete_file(void);
static void ui_save_file(void);
static void ui_open_file(void);

static void file_select_event_cb(lv_event_t *e)
{
    g_selected_file = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    g_input_events++;
    ui_refresh_file_detail();
}

static void file_action_event_cb(lv_event_t *e)
{
    uint8_t action = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    g_input_events++;
    if (action == 0) ui_create_file();
    else if (action == 1) ui_delete_file();
    else if (action == 2) ui_save_file();
    else ui_open_file();

    ui_refresh_file_list();
    ui_refresh_file_detail();
}

void ui_create_file_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_FILE);

    g_file_list = lv_list_create(scr);
    lv_obj_set_size(g_file_list, 216, 142);
    lv_obj_align(g_file_list, LV_ALIGN_TOP_LEFT, 12, 46);
    lv_obj_set_style_text_font(g_file_list, UI_FONT, 0);

    g_file_detail = ui_label(scr, "", 12, 196);
    lv_obj_set_width(g_file_detail, 216);

    ui_button(scr, UI_TEXT_FILE_NEW, 12, 266, 50, 32, file_action_event_cb, (void *)0);
    ui_button(scr, UI_TEXT_FILE_DELETE, 67, 266, 50, 32, file_action_event_cb, (void *)1);
    ui_button(scr, UI_TEXT_FILE_SAVE, 122, 266, 50, 32, file_action_event_cb, (void *)2);
    ui_button(scr, UI_TEXT_FILE_OPEN, 177, 266, 50, 32, file_action_event_cb, (void *)3);

    ui_refresh_file_list();
    ui_refresh_file_detail();
    ui_load_screen(scr);
}

static void ui_refresh_file_list(void)
{
    uint8_t active_count = 0;
    char text[80];

    if (g_file_list == NULL) return;
    lv_obj_clean(g_file_list);

    for (uint8_t i = 0; i < UI_FILE_MAX; i++) {
        if (g_files[i].state != UI_FILE_STATE_NORMAL) continue;

        snprintf(text, sizeof(text), "%s  %s  %luB",
                 g_files[i].name, g_files[i].type, (unsigned long)g_files[i].size);
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

static void ui_create_file(void)
{
    int slot = ui_file_find_free_slot();
    char name[UI_FILE_NAME_LEN];

    if (slot < 0) {
        g_error_events++;
        ui_log(UI_TEXT_FILE_TABLE_FULL);
        return;
    }

    snprintf(name, sizeof(name), "note%u.txt", (unsigned)g_file_serial++);
    if (ui_file_find(name) >= 0) {
        g_error_events++;
        ui_log(UI_TEXT_FILE_NAME_DUP);
        return;
    }

    snprintf(g_files[slot].name, sizeof(g_files[slot].name), "%s", name);
    snprintf(g_files[slot].type, sizeof(g_files[slot].type), "txt");
    snprintf(g_files[slot].content, sizeof(g_files[slot].content), "New file.");
    g_files[slot].size = strlen(g_files[slot].content);
    g_files[slot].storage_addr = (uint32_t)slot * UI_STORAGE_SLOT_SIZE;
    g_files[slot].state = UI_FILE_STATE_NORMAL;
    g_selected_file = (uint8_t)slot;
    g_storage_dirty = true;
    ui_storage_commit();
    ui_log(UI_TEXT_FILE_CREATED);
}

static void ui_delete_file(void)
{
    if (g_files[g_selected_file].state != UI_FILE_STATE_NORMAL) return;

    g_files[g_selected_file].state = UI_FILE_STATE_DELETED;
    g_storage_dirty = true;
    ui_storage_commit();
    ui_log(UI_TEXT_FILE_DELETED);
}

static void ui_save_file(void)
{
    ui_file_entry_t *file = &g_files[g_selected_file];

    if (file->state != UI_FILE_STATE_NORMAL) return;

    snprintf(file->content, sizeof(file->content), "Saved at %lus.", (unsigned long)(lv_tick_get() / 1000U));
    file->size = strlen(file->content);
    g_storage_dirty = true;
    ui_storage_commit();
    ui_log(UI_TEXT_FILE_SAVED);
}

static void ui_open_file(void)
{
    if (g_files[g_selected_file].state == UI_FILE_STATE_NORMAL) {
        ui_log(UI_TEXT_FILE_OPENED);
    }
}
