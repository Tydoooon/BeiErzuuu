#include "app_ui_internal.h"
#include "app_music_player.h"
#include "app_wav.h"

#define UI_MUSIC_MAX_TRACKS 12U
#define UI_MUSIC_SYNC_INTERVAL_MS 100U

static void ui_music_set_playing(bool playing);
static void ui_music_apply_play_state(bool playing);
static void ui_music_toggle(void);
static void ui_music_scan_tracks(void);
static void ui_music_refresh_list(void);
static void ui_music_refresh_detail(void);
static void ui_music_select_track(uint8_t track);
static void ui_music_select_next(void);
static void ui_music_select_prev(void);
static void ui_music_update_play_button(void);
static void ui_music_start_tone_test(void);
static const char *ui_music_state_text(app_music_player_state_t state);
static const char *ui_music_error_text(app_music_player_error_t error);
static const char *ui_music_wav_status_text(app_wav_status_t status);
static const char *ui_music_sd_text(bool enabled);
static const char *ui_music_test_stage_text(app_music_player_test_stage_t stage);

static app_wav_track_t g_music_tracks[UI_MUSIC_MAX_TRACKS];
static lv_obj_t *g_music_list;
static lv_obj_t *g_music_detail;
static lv_obj_t *g_music_play_button;
static size_t g_music_track_count;
static uint8_t g_music_selected;
static app_wav_status_t g_music_last_status = APP_WAV_OK;
static uint32_t g_music_last_sync_tick;

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

static void music_test_event_cb(lv_event_t *e)
{
    (void)e;
    g_input_events++;
    ui_music_start_tone_test();
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
    } else if (key_id == 4U) {
        ui_music_stop();
    }
}

void ui_create_music_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_MUSIC);

    g_music_list = lv_list_create(scr);
    lv_obj_set_size(g_music_list, 216, 122);
    lv_obj_align(g_music_list, LV_ALIGN_TOP_LEFT, 12, 46);
    lv_obj_set_style_text_font(g_music_list, UI_FONT, 0);
    lv_obj_set_scroll_dir(g_music_list, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_music_list, LV_SCROLLBAR_MODE_AUTO);

    g_music_detail = ui_label(scr, "", 12, 176);
    lv_obj_set_width(g_music_detail, 216);

    g_music_status = ui_label(scr, "", 12, 232);
    lv_obj_set_width(g_music_status, 216);

    g_music_play_button = ui_button(scr, g_music_playing ? UI_TEXT_MUSIC_STOP : UI_TEXT_MUSIC_PLAY,
                                    44, 260, 80, 32, music_play_event_cb, NULL);
    ui_button(scr, "Test", 132, 260, 64, 32, music_test_event_cb, NULL);
    ui_label(scr, "K1 Play  K2 Prev  K3 Next  K4 Exit  K5 Scan", 4, 304);

    ui_music_scan_tracks();
    ui_music_refresh_list();
    ui_music_refresh_detail();
    ui_load_screen(scr);
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

static void ui_music_start_tone_test(void)
{
    if (g_music_playing) {
        app_music_player_request_stop();
    }

    if (!app_music_player_request_tone_test()) {
        g_error_events++;
        ui_music_refresh_detail();
        return;
    }

    ui_log("Amp test tone");
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

    app_music_player_get_status(&status);
    playing = (status.state == APP_MUSIC_PLAYER_STATE_LOADING ||
               status.state == APP_MUSIC_PLAYER_STATE_PLAYING);

    if (playing != g_music_playing) {
        ui_music_apply_play_state(playing);
    }

    g_music_last_status = status.last_status;
    ui_music_refresh_detail();
}

static void ui_music_scan_tracks(void)
{
    if (app_music_player_get_state() == APP_MUSIC_PLAYER_STATE_ERROR) {
        app_music_player_request_stop();
    }

    g_music_last_status = app_wav_scan_root(g_music_tracks, UI_MUSIC_MAX_TRACKS, &g_music_track_count);
    if (g_music_last_status != APP_WAV_OK) {
        g_music_track_count = 0;
        g_music_selected = 0;
        g_error_events++;
        if (g_music_playing) {
            ui_music_set_playing(false);
        }
        return;
    }

    if (g_music_selected >= g_music_track_count) {
        g_music_selected = 0;
    }
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

    if (status.test_stage != APP_MUSIC_PLAYER_TEST_STAGE_NONE) {
        const char *wave_text = status.test_tx_sent ? "sent" : "pending";
        const char *sd_text = status.test_sd_seen_high ? "HIGH" : "LOW";

        switch (status.test_stage) {
        case APP_MUSIC_PLAYER_TEST_STAGE_PREP:
            snprintf(detail, sizeof(detail), "48kHz PCM selftest\nStage: %s\nSD seen: %s\nTX: %s",
                     ui_music_test_stage_text(status.test_stage),
                     sd_text,
                     wave_text);
            break;
        case APP_MUSIC_PLAYER_TEST_STAGE_SD_HIGH:
            snprintf(detail, sizeof(detail), "48kHz PCM selftest\nStage: %s\nSD seen: %s\nTX: %s",
                     ui_music_test_stage_text(status.test_stage),
                     sd_text,
                     wave_text);
            break;
        case APP_MUSIC_PLAYER_TEST_STAGE_TX_OK:
            snprintf(detail, sizeof(detail), "48kHz PCM selftest\nStage: %s\nSD seen: %s\nTX: %s",
                     ui_music_test_stage_text(status.test_stage),
                     sd_text,
                     wave_text);
            break;
        case APP_MUSIC_PLAYER_TEST_STAGE_DONE:
            snprintf(detail, sizeof(detail), "48kHz PCM selftest\nStage: %s\nSD seen: %s\nTX: %s",
                     ui_music_test_stage_text(status.test_stage),
                     sd_text,
                     wave_text);
            break;
        default:
            lv_label_set_text(g_music_detail, "48kHz PCM selftest");
            break;
        }
        if (status.state == APP_MUSIC_PLAYER_STATE_ERROR) {
            size_t used = strlen(detail);
            if (used < sizeof(detail)) {
                snprintf(&detail[used], sizeof(detail) - used, "\nError: %s / %s",
                         ui_music_error_text(status.last_error),
                         ui_music_wav_status_text(status.last_status));
            }
        }
        lv_label_set_text(g_music_detail, detail);
    } else if (status.state == APP_MUSIC_PLAYER_STATE_ERROR) {
        snprintf(detail, sizeof(detail), "Music error: %s\nWAV: %s",
                 ui_music_error_text(status.last_error),
                 ui_music_wav_status_text(status.last_status));
        lv_label_set_text(g_music_detail, detail);
    } else if (g_music_last_status != APP_WAV_OK) {
        snprintf(detail, sizeof(detail), "Track scan error\nWAV: %s",
                 ui_music_wav_status_text(g_music_last_status));
        lv_label_set_text(g_music_detail, detail);
    } else if (g_music_track_count == 0U) {
        lv_label_set_text(g_music_detail, "TF: no WAV file in root or /Music");
    } else if (status.state == APP_MUSIC_PLAYER_STATE_LOADING ||
               status.state == APP_MUSIC_PLAYER_STATE_PLAYING) {
        snprintf(detail, sizeof(detail), "%s\n%s  SD:%s\n%luHz %uch %ubit\n%lu/%luB",
                 status.track_name[0] != '\0' ? status.track_name : g_music_tracks[g_music_selected].name,
                 ui_music_state_text(status.state),
                 ui_music_sd_text(status.sd_enabled),
                 (unsigned long)status.wav_info.sample_rate,
                 (unsigned)status.wav_info.channels,
                 (unsigned)status.wav_info.bits_per_sample,
                 (unsigned long)status.bytes_played,
                 (unsigned long)status.wav_info.data_size);
        lv_label_set_text(g_music_detail, detail);
    } else {
        snprintf(detail, sizeof(detail), "%s\nSize: %luB",
                 g_music_tracks[g_music_selected].name,
                 (unsigned long)g_music_tracks[g_music_selected].size);
        lv_label_set_text(g_music_detail, detail);
    }

    lv_label_set_text_fmt(g_music_status, "%s  Vol:%u%%  Tracks:%u",
                          g_music_playing ? UI_TEXT_MUSIC_PLAYING : UI_TEXT_MUSIC_STOPPED,
                          g_settings.system_volume,
                          (unsigned)g_music_track_count);
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
        lv_label_set_text(label, g_music_playing ? UI_TEXT_MUSIC_STOP : UI_TEXT_MUSIC_PLAY);
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

static const char *ui_music_sd_text(bool enabled)
{
    return enabled ? "HIGH" : "LOW";
}

static const char *ui_music_test_stage_text(app_music_player_test_stage_t stage)
{
    switch (stage) {
    case APP_MUSIC_PLAYER_TEST_STAGE_PREP:
        return "PREP";
    case APP_MUSIC_PLAYER_TEST_STAGE_SD_HIGH:
        return "SD_HIGH";
    case APP_MUSIC_PLAYER_TEST_STAGE_TX_OK:
        return "TX_OK";
    case APP_MUSIC_PLAYER_TEST_STAGE_DONE:
        return "DONE";
    case APP_MUSIC_PLAYER_TEST_STAGE_NONE:
    default:
        return "NONE";
    }
}
