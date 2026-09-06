#include "app_ui_internal.h"
#include "app_backlight.h"
#include "app_mouse.h"

static uint16_t app_ui_get_screen_timeout_s(void);

lv_obj_t *g_password_ta;
lv_obj_t *g_login_hint;
lv_obj_t *g_file_list;
lv_obj_t *g_file_detail;
lv_obj_t *g_canvas;
lv_obj_t *g_draw_status;
lv_obj_t *g_music_status;
lv_obj_t *g_monitor_label;
lv_timer_t *g_monitor_timer;
lv_obj_t *g_home_buttons[UI_APP_COUNT];
lv_obj_t *g_home_key_hint;

ui_file_entry_t g_files[UI_FILE_MAX];
ui_settings_t g_settings;
ui_log_entry_t g_logs[UI_LOG_MAX];
lv_color_t g_canvas_buf[UI_DRAW_W * UI_DRAW_H] __attribute__((section(".ccmram")));
uint8_t g_draw_save[UI_DRAW_SAVE_BYTES];
uint8_t g_draw_slots[UI_FILE_MAX][UI_DRAW_SAVE_BYTES] __attribute__((section(".ccmram")));

uint8_t g_log_count;
uint8_t g_selected_file;
uint8_t g_file_serial;
uint32_t g_input_events;
uint32_t g_discarded_events;
uint32_t g_error_events;
volatile uint8_t g_pending_keys;
bool g_storage_dirty;
bool g_login_active;
bool g_music_playing;
bool g_home_active;
uint8_t g_home_selected;
ui_app_id_t g_current_app = UI_APP_NONE;

static uint32_t g_last_activity_tick;

static void app_ui_process_key(uint8_t key_id);
static void app_ui_process_home_key(uint8_t key_id);
static void app_ui_process_current_app_key(uint8_t key_id);
static void app_ui_process_mouse_wheel(void);

void app_ui_create(void)
{
    ui_model_init();
    ui_font_init();
    app_ui_notify_activity();
    ui_create_login();
    app_event_set(APP_EVT_UI_READY);
}

bool app_ui_is_created(void)
{
    return app_event_is_set(APP_EVT_UI_READY);
}

void app_ui_load_w25q128_resources(void)
{
    (void)ui_storage_load_persisted();
    app_ui_notify_activity();
}

void app_ui_notify_activity(void)
{
    g_last_activity_tick = lv_tick_get();
    if (app_event_is_set(APP_EVT_SCREEN_SLEEP)) {
        app_event_clear(APP_EVT_SCREEN_SLEEP);
        ui_apply_brightness();
    }
}

void app_ui_handle_idle(void)
{
    uint32_t now = lv_tick_get();
    uint32_t timeout_ms = (uint32_t)app_ui_get_screen_timeout_s() * 1000U;

    app_ui_process_mouse_wheel();

    if (app_event_is_set(APP_EVT_SCREEN_SLEEP) || timeout_ms == 0U) {
        return;
    }
    if ((uint32_t)(now - g_last_activity_tick) < timeout_ms) {
        return;
    }

    app_backlight_set_percent(0U);
    app_event_set(APP_EVT_SCREEN_SLEEP);
}

void app_ui_sleep_now(void)
{
    g_last_activity_tick = lv_tick_get();
    app_backlight_set_percent(0U);
    app_event_set(APP_EVT_SCREEN_SLEEP);
}

void app_ui_lock_screen(void)
{
    g_home_active = false;
    g_current_app = UI_APP_NONE;
    ui_create_login();
}

uint8_t app_ui_get_system_volume(void)
{
    return g_settings.system_volume;
}

uint8_t app_ui_get_cursor_sensitivity(void)
{
    if (g_settings.cursor_sensitivity < 1U || g_settings.cursor_sensitivity > 100U) {
        return 50U;
    }

    return g_settings.cursor_sensitivity;
}

uint8_t app_ui_get_cursor_size(void)
{
    if (g_settings.cursor_size < 4U || g_settings.cursor_size > 32U) {
        return 12U;
    }

    return g_settings.cursor_size;
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

    if (keys == 0U) return;

    for (uint8_t key_id = 1U; key_id <= 5U; key_id++) {
        if ((keys & (uint8_t)(1U << (key_id - 1U))) == 0U) continue;

        app_ui_notify_activity();
        app_ui_process_key(key_id);
    }
}

void ui_open_app(ui_app_id_t app)
{
    g_home_active = false;
    g_current_app = app;

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

static void app_ui_process_key(uint8_t key_id)
{
    if (g_login_active && g_password_ta != NULL) {
        ui_login_handle_key(key_id);
        return;
    }

    if (g_home_active) {
        app_ui_process_home_key(key_id);
    } else {
        app_ui_process_current_app_key(key_id);
    }
}

static void app_ui_process_home_key(uint8_t key_id)
{
    g_input_events++;
    if (key_id == 1U && g_home_selected < (UI_APP_COUNT - 2U)) {
        g_home_selected = (uint8_t)(g_home_selected + 2U);
    } else if (key_id == 2U && (g_home_selected % 2U) != 0U) {
        g_home_selected--;
    } else if (key_id == 3U && (g_home_selected % 2U) == 0U && g_home_selected < (UI_APP_COUNT - 1U)) {
        g_home_selected++;
    } else if (key_id == 5U && g_home_selected >= 2U) {
        g_home_selected = (uint8_t)(g_home_selected - 2U);
    } else if (key_id == 4U) {
        ui_open_app((ui_app_id_t)g_home_selected);
        return;
    }

    ui_home_refresh_key_focus();
}

static void app_ui_process_current_app_key(uint8_t key_id)
{
    g_input_events++;

    switch (g_current_app) {
    case UI_APP_FILE:
        ui_file_handle_key(key_id);
        return;
    case UI_APP_DRAW:
        ui_draw_handle_key(key_id);
        return;
    case UI_APP_MUSIC:
        ui_music_handle_key(key_id);
        break;
    default: break;
    }

    if (key_id == 4U) {
        ui_create_main();
    }
}

static void app_ui_process_mouse_wheel(void)
{
    int16_t wheel_delta = app_mouse_take_wheel_delta();

    if (wheel_delta == 0) {
        return;
    }

    app_ui_notify_activity();
    g_input_events++;

    switch (g_current_app) {
    case UI_APP_FILE:
        ui_file_handle_wheel(wheel_delta);
        break;
    case UI_APP_LOG:
        ui_log_handle_wheel(wheel_delta);
        break;
    default:
        break;
    }
}

static uint16_t app_ui_get_screen_timeout_s(void)
{
    if (g_settings.screen_timeout_s < 10U || g_settings.screen_timeout_s > 300U) {
        return 60U;
    }

    return g_settings.screen_timeout_s;
}
