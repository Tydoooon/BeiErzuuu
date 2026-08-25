#include "app_ui.h"

#include "FreeRTOS.h"
#include "task.h"
#include "app_lvgl.h"
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

static lv_obj_t *g_password_ta;
static lv_obj_t *g_login_hint;
static lv_obj_t *g_file_list;
static lv_obj_t *g_file_detail;
static lv_obj_t *g_canvas;
static lv_obj_t *g_draw_status;
static lv_obj_t *g_music_status;
static lv_obj_t *g_monitor_label;
static lv_timer_t *g_monitor_timer;

static ui_file_entry_t g_files[UI_FILE_MAX];
static ui_settings_t g_settings;
static ui_log_entry_t g_logs[UI_LOG_MAX];
static lv_color_t g_canvas_buf[UI_DRAW_W * UI_DRAW_H];
static uint8_t g_draw_save[UI_DRAW_SAVE_BYTES];

static uint8_t g_log_count;
static uint8_t g_selected_file;
static uint8_t g_file_serial;
static uint32_t g_input_events;
static uint32_t g_discarded_events;
static uint32_t g_error_events;
static volatile uint8_t g_pending_keys;
static bool g_storage_dirty;
static bool g_login_active;
static bool g_music_playing;

#if LV_FONT_SIMSUN_16_CJK
#define UI_FONT (&lv_font_simsun_16_cjk)
#else
#define UI_FONT LV_FONT_DEFAULT
#endif

static void ui_model_init(void);
static void ui_storage_load(void);
static void ui_storage_commit(void);
static void ui_log(const char *text);
static void ui_apply_brightness(void);
static void ui_load_screen(lv_obj_t *scr);
static lv_obj_t *ui_create_screen(void);
static lv_obj_t *ui_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y);
static lv_obj_t *ui_button(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
                           lv_coord_t w, lv_coord_t h, lv_event_cb_t cb, void *user_data);
static lv_obj_t *ui_create_app_frame(const char *title);
static void ui_create_login(void);
static void ui_create_main(void);
static void ui_create_file_app(void);
static void ui_create_draw_app(void);
static void ui_create_music_app(void);
static void ui_create_log_app(void);
static void ui_create_monitor_app(void);
static void ui_create_setting_app(void);
static void ui_refresh_file_list(void);
static void ui_refresh_file_detail(void);
static void ui_refresh_logs(lv_obj_t *list);
static void ui_monitor_refresh(void);
static int ui_file_find(const char *name);
static int ui_file_find_free_slot(void);
static void ui_create_file(void);
static void ui_delete_file(void);
static void ui_save_file(void);
static void ui_open_file(void);
static void ui_draw_clear(void);
static void ui_draw_save(void);
static void ui_draw_open(void);
static void ui_draw_set_saved_bit(uint32_t idx, bool value);
static bool ui_draw_get_saved_bit(uint32_t idx);
static void ui_login_submit(void);

void app_ui_create(void)
{
    ui_model_init();
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
            lv_label_set_text(g_login_hint, "按 1-4 输入，5 确认");
        } else {
            g_discarded_events++;
        }
    }
}

static void ui_model_init(void)
{
    static bool inited;

    if (inited) return;
    inited = true;

    ui_storage_load();
    ui_apply_brightness();
    ui_log("系统启动");
}

static void ui_storage_load(void)
{
    memset(g_files, 0, sizeof(g_files));
    memset(g_logs, 0, sizeof(g_logs));
    memset(g_draw_save, 0, sizeof(g_draw_save));

    g_settings.cursor_sensitivity = 50;
    g_settings.cursor_size = 12;
    g_settings.screen_brightness = 80;
    g_settings.system_volume = 50;
    g_settings.screen_timeout_s = 60;

    snprintf(g_files[0].name, sizeof(g_files[0].name), "readme.txt");
    snprintf(g_files[0].type, sizeof(g_files[0].type), "txt");
    snprintf(g_files[0].content, sizeof(g_files[0].content), "LVGL file table demo.");
    g_files[0].size = strlen(g_files[0].content);
    g_files[0].storage_addr = 0U;
    g_files[0].state = UI_FILE_STATE_NORMAL;

    snprintf(g_files[1].name, sizeof(g_files[1].name), "paint.bin");
    snprintf(g_files[1].type, sizeof(g_files[1].type), "bin");
    snprintf(g_files[1].content, sizeof(g_files[1].content), "Drawing save slot.");
    g_files[1].size = UI_DRAW_SAVE_BYTES;
    g_files[1].storage_addr = UI_STORAGE_SLOT_SIZE;
    g_files[1].state = UI_FILE_STATE_NORMAL;

    g_selected_file = 0;
    g_file_serial = 1;
    g_storage_dirty = false;
}

static void ui_storage_commit(void)
{
    g_storage_dirty = false;
}

static void ui_log(const char *text)
{
    uint8_t pos;

    if (g_log_count < UI_LOG_MAX) {
        pos = g_log_count++;
    } else {
        memmove(&g_logs[0], &g_logs[1], sizeof(g_logs[0]) * (UI_LOG_MAX - 1));
        pos = UI_LOG_MAX - 1;
    }

    g_logs[pos].tick_ms = lv_tick_get();
    snprintf(g_logs[pos].text, sizeof(g_logs[pos].text), "%s", text);
}

static void ui_apply_brightness(void)
{
    HAL_GPIO_WritePin(TFT_BL_GPIO_Port, TFT_BL_Pin,
                      g_settings.screen_brightness == 0 ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

static void ui_load_screen(lv_obj_t *scr)
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

static lv_obj_t *ui_create_screen(void)
{
    lv_obj_t *scr = lv_obj_create(NULL);
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xEEF3F8), 0);
    lv_obj_set_style_text_font(scr, UI_FONT, 0);
    lv_obj_set_style_text_color(scr, lv_color_hex(0x1F2937), 0);
    return scr;
}

static lv_obj_t *ui_label(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_style_text_color(label, lv_color_hex(0x1F2937), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, x, y);
    return label;
}

static lv_obj_t *ui_button(lv_obj_t *parent, const char *text, lv_coord_t x, lv_coord_t y,
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

static void ui_login_submit(void)
{
    if (strcmp(lv_textarea_get_text(g_password_ta), UI_PASSWORD) == 0) {
        ui_log("登录成功");
        ui_create_main();
    } else {
        g_error_events++;
        ui_log("登录失败");
        lv_label_set_text(g_login_hint, "密码错误");
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

static void ui_create_login(void)
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
    lv_label_set_text(title, "登录");
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
    lv_label_set_text(g_login_hint, "请输入密码");
    lv_obj_set_style_text_font(g_login_hint, UI_FONT, 0);
    lv_obj_set_style_text_color(g_login_hint, lv_color_hex(0x6B7280), 0);
    lv_obj_align(g_login_hint, LV_ALIGN_TOP_MID, 0, 86);

    ui_button(panel, "登录", 68, 108, 80, 34, login_button_event_cb, NULL);

    lv_obj_t *keypad = lv_btnmatrix_create(panel);
    lv_btnmatrix_set_map(keypad, key_map);
    lv_obj_set_size(keypad, 184, 124);
    lv_obj_align(keypad, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_font(keypad, UI_FONT, LV_PART_ITEMS);
    lv_obj_set_style_radius(keypad, 7, LV_PART_ITEMS);
    lv_obj_add_event_cb(keypad, login_keypad_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_load_screen(scr);
}

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

static void back_event_cb(lv_event_t *e)
{
    (void)e;
    if (g_music_playing) {
        g_music_playing = false;
        ui_log("退出音乐应用并停止播放");
    }
    g_input_events++;
    ui_create_main();
}

static void ui_create_main(void)
{
    static const char *names[] = {
        "文件管理", "画图", "音乐播放", "日志查看", "系统监控", "系统设置"
    };
    static const char *hints[] = {
        "列表/新建/保存", "绘图/清空/打开", "旋律/音量/退出",
        "事件/错误记录", "任务/内存/状态", "亮度/音量/熄屏"
    };

    lv_obj_t *scr = ui_create_screen();
    g_login_active = false;
    ui_label(scr, "LVGL 主界面", 12, 10);

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

    ui_label(scr, g_storage_dirty ? "存储: 有未同步状态" : "存储: 已同步", 12, 292);
    ui_load_screen(scr);
}

static lv_obj_t *ui_create_app_frame(const char *title)
{
    lv_obj_t *scr = ui_create_screen();

    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, 240, 36);
    lv_obj_align(bar, LV_ALIGN_TOP_LEFT, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x1F2937), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);

    ui_button(bar, "<", 4, 4, 32, 28, back_event_cb, NULL);

    lv_obj_t *label = lv_label_create(bar);
    lv_label_set_text(label, title);
    lv_obj_set_style_text_font(label, UI_FONT, 0);
    lv_obj_set_style_text_color(label, lv_color_white(), 0);
    lv_obj_align(label, LV_ALIGN_TOP_LEFT, 46, 9);

    return scr;
}

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

static void ui_create_file_app(void)
{
    lv_obj_t *scr = ui_create_app_frame("文件管理");

    g_file_list = lv_list_create(scr);
    lv_obj_set_size(g_file_list, 216, 142);
    lv_obj_align(g_file_list, LV_ALIGN_TOP_LEFT, 12, 46);
    lv_obj_set_style_text_font(g_file_list, UI_FONT, 0);

    g_file_detail = ui_label(scr, "", 12, 196);
    lv_obj_set_width(g_file_detail, 216);

    ui_button(scr, "新建", 12, 266, 50, 32, file_action_event_cb, (void *)0);
    ui_button(scr, "删除", 67, 266, 50, 32, file_action_event_cb, (void *)1);
    ui_button(scr, "保存", 122, 266, 50, 32, file_action_event_cb, (void *)2);
    ui_button(scr, "打开", 177, 266, 50, 32, file_action_event_cb, (void *)3);

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
        ui_label(g_file_list, "暂无文件", 8, 8);
    }
}

static void ui_refresh_file_detail(void)
{
    char text[224];
    ui_file_entry_t *file = &g_files[g_selected_file];

    if (g_file_detail == NULL) return;

    if (file->state != UI_FILE_STATE_NORMAL) {
        lv_label_set_text(g_file_detail, "未选择文件");
        return;
    }

    snprintf(text, sizeof(text), "名称:%s\n类型:%s  大小:%luB\n位置:0x%08lX  状态:正常\n内容:%s",
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
        ui_log("文件新建失败: 表已满");
        return;
    }

    snprintf(name, sizeof(name), "note%u.txt", (unsigned)g_file_serial++);
    if (ui_file_find(name) >= 0) {
        g_error_events++;
        ui_log("文件新建失败: 重名");
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
    ui_log("新建文件");
}

static void ui_delete_file(void)
{
    if (g_files[g_selected_file].state != UI_FILE_STATE_NORMAL) return;

    g_files[g_selected_file].state = UI_FILE_STATE_DELETED;
    g_storage_dirty = true;
    ui_storage_commit();
    ui_log("删除文件");
}

static void ui_save_file(void)
{
    ui_file_entry_t *file = &g_files[g_selected_file];

    if (file->state != UI_FILE_STATE_NORMAL) return;

    snprintf(file->content, sizeof(file->content), "Saved at %lus.", (unsigned long)(lv_tick_get() / 1000U));
    file->size = strlen(file->content);
    g_storage_dirty = true;
    ui_storage_commit();
    ui_log("保存文件");
}

static void ui_open_file(void)
{
    if (g_files[g_selected_file].state == UI_FILE_STATE_NORMAL) {
        ui_log("打开文件");
    }
}

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

static void ui_create_draw_app(void)
{
    lv_obj_t *scr = ui_create_app_frame("画图");

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

    g_draw_status = ui_label(scr, "鼠标/触摸拖动绘图", 42, 130);

    ui_button(scr, "清空", 24, 170, 58, 34, draw_action_event_cb, (void *)0);
    ui_button(scr, "保存", 91, 170, 58, 34, draw_action_event_cb, (void *)1);
    ui_button(scr, "打开", 158, 170, 58, 34, draw_action_event_cb, (void *)2);
    ui_label(scr, "保存格式: 1bit 画布快照", 32, 226);

    ui_load_screen(scr);
}

static void ui_draw_clear(void)
{
    if (g_canvas == NULL) return;
    lv_canvas_fill_bg(g_canvas, lv_color_white(), LV_OPA_COVER);
    lv_label_set_text(g_draw_status, "画布已清空");
    ui_log("清空画布");
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
    lv_label_set_text(g_draw_status, "绘图已保存");
    ui_log("保存绘图");
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

    lv_label_set_text(g_draw_status, "已打开保存内容");
    ui_log("打开绘图");
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

static void music_play_event_cb(lv_event_t *e)
{
    (void)e;
    g_input_events++;
    g_music_playing = !g_music_playing;
    lv_label_set_text_fmt(g_music_status, "状态:%s  音量:%u%%",
                          g_music_playing ? "播放中" : "已停止",
                          g_settings.system_volume);
    ui_log(g_music_playing ? "音乐播放" : "音乐停止");
}

static void ui_create_music_app(void)
{
    lv_obj_t *scr = ui_create_app_frame("音乐播放");

    ui_label(scr, "曲目: 简单旋律 A", 28, 64);
    g_music_status = ui_label(scr, "", 28, 96);
    lv_label_set_text_fmt(g_music_status, "状态:%s  音量:%u%%",
                          g_music_playing ? "播放中" : "已停止",
                          g_settings.system_volume);
    ui_button(scr, g_music_playing ? "停止" : "播放", 70, 142, 100, 44, music_play_event_cb, NULL);
    ui_label(scr, "退出应用会停止播放状态", 34, 216);

    ui_load_screen(scr);
}

static void log_clear_event_cb(lv_event_t *e)
{
    (void)e;
    g_input_events++;
    g_log_count = 0;
    ui_log("清空日志");
    ui_create_log_app();
}

static void ui_create_log_app(void)
{
    lv_obj_t *scr = ui_create_app_frame("日志查看");
    lv_obj_t *list = lv_obj_create(scr);

    lv_obj_set_size(list, 216, 198);
    lv_obj_align(list, LV_ALIGN_TOP_LEFT, 12, 48);
    lv_obj_set_style_text_font(list, UI_FONT, 0);
    lv_obj_set_style_bg_color(list, lv_color_white(), 0);
    ui_refresh_logs(list);

    ui_button(scr, "清空日志", 70, 266, 100, 34, log_clear_event_cb, NULL);
    ui_load_screen(scr);
}

static void ui_refresh_logs(lv_obj_t *list)
{
    char line[96];

    lv_obj_clean(list);
    for (uint8_t i = 0; i < g_log_count; i++) {
        snprintf(line, sizeof(line), "[%lus] %s",
                 (unsigned long)(g_logs[i].tick_ms / 1000U), g_logs[i].text);
        lv_obj_t *label = lv_label_create(list);
        lv_label_set_text(label, line);
        lv_obj_set_style_text_font(label, UI_FONT, 0);
        lv_obj_set_width(label, 194);
        lv_obj_align(label, LV_ALIGN_TOP_LEFT, 4, 4 + i * 20);
    }
}

static void monitor_timer_cb(lv_timer_t *timer)
{
    (void)timer;
    ui_monitor_refresh();
}

static void ui_create_monitor_app(void)
{
    lv_obj_t *scr = ui_create_app_frame("系统监控");

    g_monitor_label = ui_label(scr, "", 14, 52);
    lv_obj_set_width(g_monitor_label, 212);
    ui_monitor_refresh();
    ui_load_screen(scr);

    g_monitor_timer = lv_timer_create(monitor_timer_cb, 1000, NULL);
}

static void ui_monitor_refresh(void)
{
    lv_mem_monitor_t mem;
    uint8_t free_pct;

    if (g_monitor_label == NULL) return;

    lv_mem_monitor(&mem);
    free_pct = mem.used_pct <= 100U ? (uint8_t)(100U - mem.used_pct) : 0U;
    lv_label_set_text_fmt(g_monitor_label,
                          "运行时间:%lus\n系统状态:%s\n输入事件:%lu\n丢弃事件:%lu\n错误事件:%lu\n任务数量:%lu\n当前栈水位:%lu\nRTOS剩余堆:%luB\nLVGL空闲:%u%%",
                          (unsigned long)(lv_tick_get() / 1000U),
                          g_music_playing ? "音乐播放" : "正常",
                          (unsigned long)g_input_events,
                          (unsigned long)g_discarded_events,
                          (unsigned long)g_error_events,
                          (unsigned long)uxTaskGetNumberOfTasks(),
                          (unsigned long)uxTaskGetStackHighWaterMark(NULL),
                          (unsigned long)xPortGetFreeHeapSize(),
                          free_pct);
}

static void setting_slider_event_cb(lv_event_t *e)
{
    uint8_t id = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    lv_obj_t *slider = lv_event_get_target(e);
    int32_t value = lv_slider_get_value(slider);

    g_input_events++;

    if (id == 0) g_settings.cursor_sensitivity = (uint8_t)value;
    else if (id == 1) g_settings.cursor_size = (uint8_t)value;
    else if (id == 2) {
        g_settings.screen_brightness = (uint8_t)value;
        ui_apply_brightness();
    } else if (id == 3) {
        g_settings.system_volume = (uint8_t)value;
    } else {
        g_settings.screen_timeout_s = (uint16_t)value;
    }

    g_storage_dirty = true;
    ui_storage_commit();
    ui_log("修改设置");
}

static void ui_setting_row(lv_obj_t *parent, const char *name, uint8_t id,
                           int32_t min, int32_t max, int32_t value, lv_coord_t y)
{
    lv_obj_t *slider;

    ui_label(parent, name, 18, y);
    slider = lv_slider_create(parent);
    lv_obj_set_size(slider, 128, 10);
    lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 94, y + 4);
    lv_slider_set_range(slider, min, max);
    lv_slider_set_value(slider, value, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, setting_slider_event_cb, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)id);
}

static void ui_create_setting_app(void)
{
    lv_obj_t *scr = ui_create_app_frame("系统设置");

    ui_setting_row(scr, "灵敏度", 0, 1, 100, g_settings.cursor_sensitivity, 58);
    ui_setting_row(scr, "光标大小", 1, 4, 32, g_settings.cursor_size, 96);
    ui_setting_row(scr, "亮度", 2, 0, 100, g_settings.screen_brightness, 134);
    ui_setting_row(scr, "音量", 3, 0, 100, g_settings.system_volume, 172);
    ui_setting_row(scr, "熄屏秒", 4, 10, 300, g_settings.screen_timeout_s, 210);
    ui_label(scr, "设置变化立即进入状态层", 30, 266);

    ui_load_screen(scr);
}
