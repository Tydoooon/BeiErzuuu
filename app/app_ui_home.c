#include "app_ui_internal.h"

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

void ui_create_main(void)
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
