#include "app_ui_internal.h"

#define UI_LOGIN_MAX_WRONG_COUNT 5U
#define UI_LOGIN_LOCK_MS         10000U
#define UI_LOGIN_PASSWORD_MAX_LENGTH 4U

static uint8_t g_login_wrong_count;
static uint32_t g_login_lock_until;
static lv_timer_t *g_login_lock_timer;

static void ui_login_add_text(const char *text);
static bool ui_login_is_locked(void);
static uint32_t ui_login_lock_remaining_ms(void);
static void ui_login_start_lock(void);
static void ui_login_stop_lock_timer(void);
static void ui_login_refresh_hint(void);
static void login_lock_timer_cb(lv_timer_t *timer);

void ui_login_submit(void)
{
    if (ui_login_is_locked()) {
        ui_login_refresh_hint();
        return;
    }

    if (strcmp(lv_textarea_get_text(g_password_ta), UI_PASSWORD) == 0) {
        g_login_wrong_count = 0U;
        g_login_lock_until = 0U;
        app_event_clear(APP_EVT_LOGIN_LOCKED);
        ui_login_stop_lock_timer();
        ui_log(UI_TEXT_LOGIN_SUCCESS);
        ui_create_main();
    } else {
        g_error_events++;
        g_login_wrong_count++;
        ui_log(UI_TEXT_LOGIN_FAILED);
        lv_textarea_set_text(g_password_ta, "");

        if (g_login_wrong_count >= UI_LOGIN_MAX_WRONG_COUNT) {
            ui_login_start_lock();
        } else {
            ui_login_refresh_hint();
        }
    }
}

void ui_login_handle_key(uint8_t key_id)
{
    g_input_events++;

    if (ui_login_is_locked()) {
        ui_login_refresh_hint();
        return;
    }

    if (key_id == 5U) {
        ui_login_submit();
    } else {
        char digit[2] = { (char)('0' + key_id), '\0' };
        ui_login_add_text(digit);
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

    if (ui_login_is_locked()) {
        ui_login_refresh_hint();
        return;
    }

    if (strcmp(txt, "OK") == 0) {
        ui_login_submit();
    } else if (strcmp(txt, "Del") == 0) {
        lv_textarea_del_char(g_password_ta);
    } else {
        ui_login_add_text(txt);
    }
}

void ui_create_login(void)
{
    static const char * const key_map[] = {
        "1", "2", "3", "\n",
        "4", "5", "6", "\n",
        "7", "8", "9", "\n",
        "Del", "0", "OK", ""
    };

    lv_obj_t *scr = ui_create_screen();
    g_login_active = true;
    g_login_wrong_count = 0U;
    g_login_lock_until = 0U;
    app_event_clear(APP_EVT_LOGIN_LOCKED);
    ui_login_stop_lock_timer();
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
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);

    ui_add_sleep_button(panel, 160, 6);

    g_login_hint = lv_label_create(panel);
    lv_label_set_text(g_login_hint, UI_TEXT_LOGIN_HINT);
    lv_obj_set_style_text_font(g_login_hint, UI_FONT, 0);
    lv_obj_set_style_text_color(g_login_hint, lv_color_hex(0x6B7280), 0);
    lv_obj_align(g_login_hint, LV_ALIGN_TOP_MID, 0, 30);

    g_password_ta = lv_textarea_create(panel);
    lv_obj_set_size(g_password_ta, 164, 36);
    lv_obj_align(g_password_ta, LV_ALIGN_TOP_MID, 0, 50);
    lv_textarea_set_one_line(g_password_ta, true);
    lv_textarea_set_password_mode(g_password_ta, true);
    lv_textarea_set_max_length(g_password_ta, UI_LOGIN_PASSWORD_MAX_LENGTH);
    lv_textarea_set_accepted_chars(g_password_ta, "0123456789");
    lv_obj_set_style_text_font(g_password_ta, UI_FONT, 0);

    //“登录”按钮位置
    ui_button(panel, UI_TEXT_LOGIN_BUTTON, 52, 92, 80, 34, login_button_event_cb, NULL);

    lv_obj_t *keypad = lv_btnmatrix_create(panel);
    lv_btnmatrix_set_map(keypad, (const char **)key_map);
    lv_obj_set_size(keypad, 184, 124);
    lv_obj_align(keypad, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_set_style_text_font(keypad, UI_FONT, LV_PART_ITEMS);
    lv_obj_set_style_radius(keypad, 7, LV_PART_ITEMS);
    lv_obj_add_event_cb(keypad, login_keypad_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    ui_load_screen(scr);
}

static void ui_login_add_text(const char *text)
{
    if (g_password_ta == NULL || text == NULL) return;

    if (strlen(lv_textarea_get_text(g_password_ta)) >= UI_LOGIN_PASSWORD_MAX_LENGTH) {
        app_ui_record_discarded_event();
        return;
    }

    lv_textarea_add_text(g_password_ta, text);
    ui_login_refresh_hint();
}

static bool ui_login_is_locked(void)
{
    if (g_login_lock_until == 0U) {
        return false;
    }
    if (ui_login_lock_remaining_ms() > 0U) {
        return true;
    }

    g_login_lock_until = 0U;
    g_login_wrong_count = 0U;
    app_event_clear(APP_EVT_LOGIN_LOCKED);
    ui_login_stop_lock_timer();
    ui_login_refresh_hint();
    return false;
}

static uint32_t ui_login_lock_remaining_ms(void)
{
    uint32_t now;

    if (g_login_lock_until == 0U) {
        return 0U;
    }

    now = lv_tick_get();
    if ((int32_t)(g_login_lock_until - now) <= 0) {
        return 0U;
    }

    return g_login_lock_until - now;
}

static void ui_login_start_lock(void)
{
    g_login_lock_until = lv_tick_get() + UI_LOGIN_LOCK_MS;
    app_event_set(APP_EVT_LOGIN_LOCKED);
    lv_textarea_set_text(g_password_ta, "");
    ui_log(UI_TEXT_LOGIN_LOCKED);
    ui_login_refresh_hint();

    if (g_login_lock_timer == NULL) {
        g_login_lock_timer = lv_timer_create(login_lock_timer_cb, 250, NULL);
    }
}

static void ui_login_stop_lock_timer(void)
{
    if (g_login_lock_timer != NULL) {
        lv_timer_del(g_login_lock_timer);
        g_login_lock_timer = NULL;
    }
}

static void ui_login_refresh_hint(void)
{
    uint32_t remaining_ms;

    if (g_login_hint == NULL) return;

    remaining_ms = ui_login_lock_remaining_ms();
    if (remaining_ms > 0U) {
        uint32_t seconds = (remaining_ms + 999U) / 1000U;
        lv_label_set_text_fmt(g_login_hint, UI_TEXT_LOGIN_LOCKED_FMT,
                              (unsigned long)seconds);
    } else if (g_login_wrong_count > 0U) {
        lv_label_set_text_fmt(g_login_hint, UI_TEXT_LOGIN_WRONG_COUNT_FMT,
                              (unsigned)g_login_wrong_count);
    } else {
        lv_label_set_text(g_login_hint, UI_TEXT_LOGIN_HINT);
    }
}

static void login_lock_timer_cb(lv_timer_t *timer)
{
    (void)timer;

    if (!ui_login_is_locked()) {
        return;
    }

    ui_login_refresh_hint();
}
