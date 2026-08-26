#include "app_ui_internal.h"

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

void ui_create_music_app(void)
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
