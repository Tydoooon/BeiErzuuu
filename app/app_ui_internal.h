#ifndef __APP_UI_INTERNAL_H
#define __APP_UI_INTERNAL_H

#include "app_ui.h"

#include "FreeRTOS.h"
#include "task.h"
#include "app_lvgl.h"
#include "app_ui_font.h"
#include "app_ui_text.h"
#include "bsp_lcd.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define UI_PASSWORD                  "1234"
#define UI_FILE_MAX                  8
#define UI_FILE_NAME_LEN             24
#define UI_FILE_TYPE_LEN             8
#define UI_FILE_CONTENT_LEN          96
#define UI_LOG_MAX                   12
#define UI_LOG_TEXT_LEN              72
#define UI_DRAW_W                    96
#define UI_DRAW_H                    64
#define UI_DRAW_SAVE_BYTES           ((UI_DRAW_W * UI_DRAW_H + 7) / 8)
#define UI_STORAGE_SLOT_SIZE         0x100U

typedef enum {
    UI_FILE_STATE_EMPTY = 0,
    UI_FILE_STATE_NORMAL,
    UI_FILE_STATE_DELETED,
} ui_file_state_t;

typedef struct {
    char name[UI_FILE_NAME_LEN];
    char type[UI_FILE_TYPE_LEN];
    uint32_t size;
    uint32_t storage_addr;
    ui_file_state_t state;
    char content[UI_FILE_CONTENT_LEN];
} ui_file_entry_t;

typedef struct {
    uint8_t cursor_sensitivity;
    uint8_t cursor_size;
    uint8_t screen_brightness;
    uint8_t system_volume;
    uint16_t screen_timeout_s;
} ui_settings_t;

typedef struct {
    uint32_t tick_ms;
    char text[UI_LOG_TEXT_LEN];
} ui_log_entry_t;

typedef enum {
    UI_APP_FILE = 0,
    UI_APP_DRAW,
    UI_APP_MUSIC,
    UI_APP_LOG,
    UI_APP_MONITOR,
    UI_APP_SETTING,
} ui_app_id_t;

extern lv_obj_t *g_password_ta;
extern lv_obj_t *g_login_hint;
extern lv_obj_t *g_file_list;
extern lv_obj_t *g_file_detail;
extern lv_obj_t *g_canvas;
extern lv_obj_t *g_draw_status;
extern lv_obj_t *g_music_status;
extern lv_obj_t *g_monitor_label;
extern lv_timer_t *g_monitor_timer;

extern ui_file_entry_t g_files[UI_FILE_MAX];
extern ui_settings_t g_settings;
extern ui_log_entry_t g_logs[UI_LOG_MAX];
extern lv_color_t g_canvas_buf[UI_DRAW_W * UI_DRAW_H];
extern uint8_t g_draw_save[UI_DRAW_SAVE_BYTES];

extern uint8_t g_log_count;
extern uint8_t g_selected_file;
extern uint8_t g_file_serial;
extern uint32_t g_input_events;
extern uint32_t g_discarded_events;
extern uint32_t g_error_events;
extern volatile uint8_t g_pending_keys;
extern bool g_storage_dirty;
extern bool g_login_active;
extern bool g_music_playing;

#define UI_FONT ui_font_get()

void ui_model_init(void);
void ui_storage_load(void);
void ui_storage_commit(void);
void ui_log(const char *text);
void ui_apply_brightness(void);

void ui_load_screen(lv_obj_t *scr);
lv_obj_t *ui_create_screen(void);
lv_obj_t *ui_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y);
lv_obj_t *ui_button(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                    lv_coord_t w, lv_coord_t h, lv_event_cb_t cb, void *user_data);
lv_obj_t *ui_create_app_frame(const char *title);

void ui_login_submit(void);
void ui_create_login(void);
void ui_create_main(void);
void ui_create_file_app(void);
void ui_create_draw_app(void);
void ui_create_music_app(void);
void ui_create_log_app(void);
void ui_create_monitor_app(void);
void ui_create_setting_app(void);

#endif
