#include "app_ui_internal.h"
#include "app_music_player.h"
#include "app_wav.h"

#define UI_MUSIC_MAX_TRACKS 12U
#define UI_MUSIC_SYNC_INTERVAL_MS 200U
#define UI_MUSIC_WHEEL_STEP 24

static void ui_music_set_playing(bool playing);
static void ui_music_apply_play_state(bool playing);
static void ui_music_toggle(void);
static void ui_music_scan_tracks(void);
static void ui_music_poll_scan(void);
static void ui_music_refresh_list(void);
static void ui_music_refresh_detail(void);
static void ui_music_refresh_progress(void);
static void ui_music_select_track(uint8_t track);
static void ui_music_select_next(void);
static void ui_music_select_prev(void);
static void ui_music_update_play_button(void);
static const char *ui_music_state_text(app_music_player_state_t state);
static const char *ui_music_error_text(app_music_player_error_t error);
static const char *ui_music_wav_status_text(app_wav_status_t status);
static void ui_music_set_label_text(lv_obj_t *label, const char *text);

static app_wav_track_t g_music_tracks[UI_MUSIC_MAX_TRACKS];
static lv_obj_t *g_music_list;
static lv_obj_t *g_music_detail;
static lv_obj_t *g_music_progress;
static lv_obj_t *g_music_play_button;
static size_t g_music_track_count;
static uint8_t g_music_selected;
static app_wav_status_t g_music_last_status = APP_WAV_OK;
static uint32_t g_music_last_sync_tick;
static uint32_t g_music_scan_seq;

static void music_track_event_cb(lv_event_t *e)
{
    uint8_t track = (uint8_t)(uintptr_t)lv_event_get_user_data(e);

    g_input_events++;
    ui_music_select_track(track);
}

static void music_play_event_cb(lv_event_t *e)
{
    (void)e;
    g_input_events++;
    ui_music_toggle();
    ui_log(g_music_playing ? UI_TEXT_MUSIC_LOG_PLAY : UI_TEXT_MUSIC_LOG_STOP);
}

static void ui_music_progress_event_cb(lv_event_t *e)
{
    app_music_player_status_t status;
    lv_indev_t *indev = lv_indev_get_act();
    lv_obj_t *bar = lv_event_get_target(e);
    lv_point_t point;
    lv_area_t area;
    int32_t x;
    int32_t w;
    uint32_t total;
    uint32_t offset;

    if (indev == NULL || bar == NULL) {
        return;
    }

    app_music_player_get_status(&status);
    if (status.state != APP_MUSIC_PLAYER_STATE_PLAYING &&
        status.state != APP_MUSIC_PLAYER_STATE_LOADING) {
        return;   /* 未在播放，不响应跳转 */
    }
    total = status.wav_info.data_size;
    if (total == 0U) {
        return;
    }

    lv_indev_get_point(indev, &point);
    lv_obj_get_coords(bar, &area);
    w = (int32_t)(area.x2 - area.x1) + 1;
    if (w <= 0) {
        return;
    }
    x = point.x - area.x1;
    if (x < 0) {
        x = 0;
    } else if (x > w) {
        x = w;
    }

    /* 点击位置 -> 字节偏移（帧对齐在 app_wav_seek_data 内完成） */
    offset = (uint32_t)(((uint64_t)total * (uint32_t)x) / (uint32_t)w);

    g_input_events++;
    app_ui_notify_activity();
    app_music_player_request_seek(offset);
}

void ui_music_handle_key(uint8_t key_id)
{
    if (key_id == 1U) {
        ui_music_toggle();
        ui_log(g_music_playing ? UI_TEXT_MUSIC_LOG_PLAY : UI_TEXT_MUSIC_LOG_STOP);
    } else if (key_id == 2U) {
        ui_music_select_prev();
    } else if (key_id == 3U) {
        ui_music_select_next();
    } else if (key_id == 5U) {
        ui_music_scan_tracks();
        ui_music_refresh_list();
        ui_music_refresh_detail();
    } /* K4：由上层退出到主界面，音乐后台继续播放，不在此停止 */
}

void ui_create_music_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_MUSIC);
    app_music_player_status_t status;

    /* 重进页面时使用播放器真实状态，不能沿用上次页面留下的按钮状态。 */
    app_music_player_get_status(&status);
    g_music_playing = (status.state == APP_MUSIC_PLAYER_STATE_LOADING ||
                       status.state == APP_MUSIC_PLAYER_STATE_PLAYING);
    g_music_last_sync_tick = 0;

    g_music_list = lv_list_create(scr);
    lv_obj_set_size(g_music_list, 216, 122);
    lv_obj_align(g_music_list, LV_ALIGN_TOP_LEFT, 12, 46);
    lv_obj_set_style_text_font(g_music_list, UI_FONT, 0);
    lv_obj_set_scroll_dir(g_music_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_music_list, LV_SCROLLBAR_MODE_AUTO);

    g_music_detail = ui_label(scr, "", 12, 174);
    lv_obj_set_width(g_music_detail, 216);

    g_music_progress = lv_bar_create(scr);
    lv_obj_set_size(g_music_progress, 216, 10);
    lv_obj_align(g_music_progress, LV_ALIGN_TOP_LEFT, 12, 232);
    lv_bar_set_range(g_music_progress, 0, 100);
    lv_bar_set_value(g_music_progress, 0, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(g_music_progress, lv_color_hex(0xD1D5DB), LV_PART_MAIN);
    lv_obj_set_style_bg_color(g_music_progress, lv_color_hex(0x2F80ED), LV_PART_INDICATOR);
    lv_obj_add_flag(g_music_progress, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(g_music_progress, ui_music_progress_event_cb, LV_EVENT_CLICKED, NULL);

    g_music_status = ui_label(scr, "", 12, 248);
    lv_obj_set_width(g_music_status, 216);

    g_music_play_button = ui_button(scr, g_music_playing ? UI_TEXT_MUSIC_STOP : UI_TEXT_MUSIC_PLAY,
                                    44, 268, 80, 32, music_play_event_cb, NULL);
    ui_label(scr, "K1 Play  K2 Prev  K3 Next  K4 Exit  K5 Scan", 4, 304);

    ui_music_scan_tracks();
    ui_music_refresh_list();
    ui_music_refresh_detail();
    ui_load_screen(scr);
}

void ui_music_handle_wheel(int16_t wheel_delta)
{
    if (g_music_list == NULL || wheel_delta == 0) {
        return;
    }

    lv_obj_scroll_by_bounded(g_music_list, 0,
                             (lv_coord_t)wheel_delta * UI_MUSIC_WHEEL_STEP, LV_ANIM_OFF);
}

static void ui_music_set_playing(bool playing)
{
    if (playing) {
        if (g_music_track_count == 0U) {
            g_music_last_status = APP_WAV_OPEN_FAILED;
            g_error_events++;
            ui_music_refresh_detail();
            return;
        }

        if (!app_music_player_request_play(&g_music_tracks[g_music_selected])) {
            g_music_last_status = APP_WAV_ERROR;
            g_error_events++;
            ui_music_refresh_detail();
            return;
        }
        ui_music_apply_play_state(true);
    } else {
        app_music_player_request_stop();
        ui_music_apply_play_state(false);
    }

    ui_music_refresh_detail();
}

static void ui_music_toggle(void)
{
    ui_music_set_playing(!g_music_playing);
}

void ui_music_stop(void)
{
    ui_music_set_playing(false);
}

void ui_music_sync_state(void)
{
    app_music_player_status_t status;
    bool playing;
    uint32_t now;

    if (g_current_app != UI_APP_MUSIC) {
        return;
    }

    now = lv_tick_get();
    if ((uint32_t)(now - g_music_last_sync_tick) < UI_MUSIC_SYNC_INTERVAL_MS) {
        return;
    }
    g_music_last_sync_tick = now;

    ui_music_poll_scan();

    app_music_player_get_status(&status);
    playing = (status.state == APP_MUSIC_PLAYER_STATE_LOADING ||
               status.state == APP_MUSIC_PLAYER_STATE_PLAYING);

    if (playing != g_music_playing) {
        ui_music_apply_play_state(playing);
    }

    g_music_last_status = status.last_status;
    ui_music_refresh_detail();
    ui_music_refresh_progress();
}

static void ui_music_scan_tracks(void)
{
    app_music_player_state_t state = app_music_player_get_state();

    if (state == APP_MUSIC_PLAYER_STATE_PLAYING ||
        state == APP_MUSIC_PLAYER_STATE_LOADING) {
        return;
    }

    if (state == APP_MUSIC_PLAYER_STATE_ERROR) {
        app_music_player_request_stop();
    }

    /* 请求后台扫描：由音频任务分步执行，不阻塞 UI；
     * 播放中音频任务会跳过扫描（避免 remount 中断音乐）。 */
    app_music_player_request_scan();
}

static void ui_music_poll_scan(void)
{
    app_wav_status_t status;
    size_t count;

    if (!app_music_player_get_scan(&g_music_scan_seq, &status,
                                   g_music_tracks, UI_MUSIC_MAX_TRACKS,
                                   &count)) {
        return;
    }

    g_music_track_count = count;
    g_music_last_status = status;
    if (g_music_last_status != APP_WAV_OK) {
        g_music_track_count = 0;
        g_music_selected = 0;
        g_error_events++;
        if (g_music_playing) {
            ui_music_set_playing(false);
        }
    } else if (g_music_selected >= g_music_track_count) {
        g_music_selected = 0;
    }

    ui_music_refresh_list();
    ui_music_refresh_detail();
}

static void ui_music_refresh_list(void)
{
    char text[96];

    if (g_music_list == NULL) {
        return;
    }

    lv_obj_clean(g_music_list);

    if (g_music_track_count == 0U) {
        ui_label(g_music_list, "No WAV files", 8, 8);
        return;
    }

    for (uint8_t i = 0; i < g_music_track_count; i++) {
        const char *mark = (i == g_music_selected) ? "> " : "  ";
        lv_obj_t *btn;

        snprintf(text, sizeof(text), "%s%.*s", mark, APP_WAV_NAME_MAX - 1, g_music_tracks[i].name);
        btn = lv_list_add_btn(g_music_list, NULL, text);
        lv_obj_set_style_text_font(btn, UI_FONT, 0);
        lv_obj_add_event_cb(btn, music_track_event_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
}

static void ui_music_refresh_detail(void)
{
    app_music_player_status_t status;
    char detail[192];

    if (g_music_detail == NULL || g_music_status == NULL) {
        return;
    }

    app_music_player_get_status(&status);
    g_music_last_status = status.last_status;

    if (status.state == APP_MUSIC_PLAYER_STATE_ERROR) {
        snprintf(detail, sizeof(detail), "%s", ui_music_error_text(status.last_error));
    } else if (g_music_last_status != APP_WAV_OK) {
        snprintf(detail, sizeof(detail), "Track scan error\nWAV: %s",
                 ui_music_wav_status_text(g_music_last_status));
    } else if (g_music_track_count == 0U) {
        snprintf(detail, sizeof(detail), "TF: no WAV file in root or /Music");
    } else if (status.state == APP_MUSIC_PLAYER_STATE_LOADING ||
               status.state == APP_MUSIC_PLAYER_STATE_PLAYING) {
        snprintf(detail, sizeof(detail), "%s\n%s\n%luHz %uch %ubit",
                 status.track_name[0] != '\0' ? status.track_name : g_music_tracks[g_music_selected].name,
                 ui_music_state_text(status.state),
                  (unsigned long)status.wav_info.sample_rate,
                  (unsigned)status.wav_info.channels,
                  (unsigned)status.wav_info.bits_per_sample);
    } else {
        snprintf(detail, sizeof(detail), "%s\nSize: %luB",
                 g_music_tracks[g_music_selected].name,
                 (unsigned long)g_music_tracks[g_music_selected].size);
    }
    ui_music_set_label_text(g_music_detail, detail);

    snprintf(detail, sizeof(detail), "%s  Vol:%u%%  Tracks:%u",
             g_music_playing ? UI_TEXT_MUSIC_PLAYING : UI_TEXT_MUSIC_STOPPED,
             g_settings.system_volume,
             (unsigned)g_music_track_count);
    ui_music_set_label_text(g_music_status, detail);
}

static void ui_music_refresh_progress(void)
{
    app_music_player_status_t status;
    bool playing;
    uint32_t total;
    uint32_t played;
    int32_t pct;

    if (g_music_progress == NULL) {
        return;
    }

    app_music_player_get_status(&status);
    playing = (status.state == APP_MUSIC_PLAYER_STATE_LOADING ||
               status.state == APP_MUSIC_PLAYER_STATE_PLAYING);
    total = status.wav_info.data_size;
    played = status.bytes_played;

    if (!playing || total == 0U) {
        if (lv_bar_get_value(g_music_progress) != 0) {
            lv_bar_set_value(g_music_progress, 0, LV_ANIM_OFF);
        }
        return;
    }

    pct = (int32_t)(((uint64_t)played * 100U) / total);
    if (pct > 100) {
        pct = 100;
    } else if (pct < 0) {
        pct = 0;
    }

    if (lv_bar_get_value(g_music_progress) != pct) {
        lv_bar_set_value(g_music_progress, pct, LV_ANIM_OFF);
    }
}

static void ui_music_select_track(uint8_t track)
{
    if (track >= g_music_track_count) {
        return;
    }

    if (app_music_player_get_state() == APP_MUSIC_PLAYER_STATE_ERROR) {
        app_music_player_request_stop();
    }

    if (g_music_playing) {
        ui_music_set_playing(false);
    }

    g_music_selected = track;
    g_music_last_status = APP_WAV_OK;
    ui_music_refresh_list();
    ui_music_refresh_detail();
}

static void ui_music_select_next(void)
{
    if (g_music_track_count == 0U) {
        return;
    }

    ui_music_select_track((uint8_t)((g_music_selected + 1U) % g_music_track_count));
}

static void ui_music_select_prev(void)
{
    if (g_music_track_count == 0U) {
        return;
    }

    if (g_music_selected == 0U) {
        ui_music_select_track((uint8_t)(g_music_track_count - 1U));
    } else {
        ui_music_select_track((uint8_t)(g_music_selected - 1U));
    }
}

static void ui_music_apply_play_state(bool playing)
{
    g_music_playing = playing;

    if (playing) {
        app_event_set(APP_EVT_MUSIC_PLAYING);
    } else {
        app_event_clear(APP_EVT_MUSIC_PLAYING);
    }

    ui_music_update_play_button();
}

static void ui_music_update_play_button(void)
{
    lv_obj_t *label;

    if (g_music_play_button == NULL) {
        return;
    }

    label = lv_obj_get_child(g_music_play_button, 0);
    if (label != NULL) {
        ui_music_set_label_text(label, g_music_playing ? UI_TEXT_MUSIC_STOP : UI_TEXT_MUSIC_PLAY);
    }
}

static void ui_music_set_label_text(lv_obj_t *label, const char *text)
{
    const char *current;

    if (label == NULL || text == NULL) {
        return;
    }

    current = lv_label_get_text(label);
    if (current == NULL || strcmp(current, text) != 0) {
        lv_label_set_text(label, text);
    }
}

static const char *ui_music_state_text(app_music_player_state_t state)
{
    switch (state) {
    case APP_MUSIC_PLAYER_STATE_LOADING:
        return "Loading";
    case APP_MUSIC_PLAYER_STATE_PLAYING:
        return "Playing";
    case APP_MUSIC_PLAYER_STATE_ERROR:
        return "Error";
    case APP_MUSIC_PLAYER_STATE_IDLE:
    default:
        return "Idle";
    }
}

static const char *ui_music_error_text(app_music_player_error_t error)
{
    switch (error) {
    case APP_MUSIC_PLAYER_ERROR_INVALID_PARAM:
        return "invalid track";
    case APP_MUSIC_PLAYER_ERROR_QUEUE_FULL:
        return "command queue full";
    case APP_MUSIC_PLAYER_ERROR_WAV_MOUNT:
        return "TF mount failed";
    case APP_MUSIC_PLAYER_ERROR_WAV_OPEN:
        return "file open failed";
    case APP_MUSIC_PLAYER_ERROR_WAV_UNSUPPORTED:
        return "unsupported WAV";
    case APP_MUSIC_PLAYER_ERROR_WAV_READ:
        return "read failed";
    case APP_MUSIC_PLAYER_ERROR_WAV_SEEK:
        return "seek failed";
    case APP_MUSIC_PLAYER_ERROR_I2S_INIT:
        return "I2S init failed";
    case APP_MUSIC_PLAYER_ERROR_I2S_TRANSMIT:
        return "I2S transmit failed";
    case APP_MUSIC_PLAYER_ERROR_SD_LOW:
        return "SD pin low";
    case APP_MUSIC_PLAYER_ERROR_NONE:
    default:
        return "none";
    }
}

static const char *ui_music_wav_status_text(app_wav_status_t status)
{
    switch (status) {
    case APP_WAV_OK:
        return "ok";
    case APP_WAV_ERROR:
        return "generic error";
    case APP_WAV_INVALID_PARAM:
        return "invalid param";
    case APP_WAV_MOUNT_FAILED:
        return "mount failed";
    case APP_WAV_OPEN_FAILED:
        return "open failed";
    case APP_WAV_UNSUPPORTED:
        return "unsupported";
    case APP_WAV_READ_FAILED:
        return "read failed";
    case APP_WAV_SEEK_FAILED:
        return "seek failed";
    default:
        return "unknown";
    }
}
