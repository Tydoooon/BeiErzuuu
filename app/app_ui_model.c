#include "app_ui_internal.h"

void ui_model_init(void)
{
    static bool inited;

    if (inited) return;
    inited = true;

    ui_storage_load();
    ui_apply_brightness();
    ui_log(UI_TEXT_SYSTEM_START);
}

void ui_storage_load(void)
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

void ui_storage_commit(void)
{
    g_storage_dirty = false;
}

void ui_log(const char *text)
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

void ui_apply_brightness(void)
{
    HAL_GPIO_WritePin(TFT_BL_GPIO_Port, TFT_BL_Pin,
                      g_settings.screen_brightness == 0 ? GPIO_PIN_RESET : GPIO_PIN_SET);
}
