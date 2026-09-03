#include "app_ui_internal.h"

static void ui_music_set_playing(bool playing);
static void ui_music_toggle(void);
static void music_play_event_cb(lv_event_t *e)
{
    (void)e;
    g_input_events++;
    ui_music_toggle();
    ui_log(g_music_playing ? UI_TEXT_MUSIC_LOG_PLAY : UI_TEXT_MUSIC_LOG_STOP);
}

void ui_music_handle_key(uint8_t key_id)
{
    if (key_id == 1U) {
        ui_music_toggle();
        ui_log(g_music_playing ? UI_TEXT_MUSIC_LOG_PLAY : UI_TEXT_MUSIC_LOG_STOP);
    }
}

void ui_create_music_app(void)
{
    lv_obj_t *scr = ui_create_app_frame(UI_TEXT_APP_MUSIC);

    ui_label(scr, UI_TEXT_MUSIC_TRACK, 28, 64);
    g_music_status = ui_label(scr, "", 28, 96);
    lv_label_set_text_fmt(g_music_status, UI_TEXT_MUSIC_STATUS_FMT,
                          g_music_playing ? UI_TEXT_MUSIC_PLAYING : UI_TEXT_MUSIC_STOPPED,
                          g_settings.system_volume);
    ui_button(scr, g_music_playing ? UI_TEXT_MUSIC_STOP : UI_TEXT_MUSIC_PLAY,
              70, 142, 100, 44, music_play_event_cb, NULL);
    ui_label(scr, UI_TEXT_MUSIC_EXIT_TIP, 34, 216);
    ui_label(scr, "K1 Play/Stop  K4 Exit", 42, 266);

    ui_load_screen(scr);
}

static void ui_music_set_playing(bool playing)
{
    g_music_playing = playing;
    if (playing) {
        app_event_set(APP_EVT_MUSIC_PLAYING);
    } else {
        app_event_clear(APP_EVT_MUSIC_PLAYING);
    }

    if (g_music_status != NULL) {
        lv_label_set_text_fmt(g_music_status, UI_TEXT_MUSIC_STATUS_FMT,
                              g_music_playing ? UI_TEXT_MUSIC_PLAYING : UI_TEXT_MUSIC_STOPPED,
                              g_settings.system_volume);
    }
}

static void ui_music_toggle(void)
{
    ui_music_set_playing(!g_music_playing);
}
