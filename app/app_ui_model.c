#include "app_ui_internal.h"

#include "app_backlight.h"

#include <stddef.h>

#define UI_STORAGE_MAGIC               0x31495355U
#define UI_STORAGE_VERSION             1U

typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t sequence;
    ui_settings_t settings;
    ui_file_entry_t files[UI_FILE_MAX];
    uint8_t selected_file;
    uint8_t file_serial;
    uint8_t reserved[2];
    uint32_t crc32;
} ui_storage_record_t;

static uint32_t g_storage_sequence;
static ui_storage_record_t g_storage_record_a;
static ui_storage_record_t g_storage_record_b;
static uint8_t g_storage_verify_chunk[256];

static void ui_storage_load_defaults(void);
static bool ui_storage_load_flash(void);
static bool ui_storage_read_record(uint32_t addr, ui_storage_record_t *record);
static bool ui_storage_validate_record(const ui_storage_record_t *record);
static bool ui_storage_write_record(uint32_t addr, uint32_t sequence);
static bool ui_storage_read_draw_slots(void);
static uint32_t ui_storage_draw_addr(uint8_t slot);
static uint32_t ui_storage_crc32(const uint8_t *data, size_t len);
static uint32_t ui_storage_record_crc(const ui_storage_record_t *record);
static void ui_storage_sanitize_loaded_state(void);

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
    ui_storage_load_defaults();
    g_storage_dirty = false;
    app_event_clear(APP_EVT_STORAGE_DIRTY | APP_EVT_STORAGE_BUSY);
}

bool ui_storage_load_persisted(void)
{
    bool loaded;

    if (g_input_events != 0U || g_storage_dirty) {
        return false;
    }
    app_event_set(APP_EVT_STORAGE_BUSY);
    loaded = ui_storage_load_flash();
    app_event_clear(APP_EVT_STORAGE_BUSY);
    if (!loaded) return false;

    ui_apply_brightness();
    g_storage_dirty = false;
    app_event_clear(APP_EVT_STORAGE_DIRTY);
    return true;
}

void ui_storage_mark_dirty(void)
{
    g_storage_dirty = true;
    app_event_set(APP_EVT_STORAGE_DIRTY);
}

void ui_storage_commit(void)
{
    uint32_t sequence;
    uint32_t addr;

    if (!g_storage_dirty) {
        return;
    }

    app_event_set(APP_EVT_STORAGE_BUSY);

    if (sizeof(ui_storage_record_t) > W25Q128_SECTOR_SIZE) {
        app_event_clear(APP_EVT_STORAGE_BUSY);
        return;
    }

    if (!app_w25q128_is_ready() && app_w25q128_init() != BSP_W25Q128_OK) {
        app_event_clear(APP_EVT_STORAGE_BUSY);
        return;
    }

    sequence = g_storage_sequence + 1U;
    addr = (sequence & 1U) != 0U ?
           APP_W25Q128_ADDR_FILE_TABLE_B :
           APP_W25Q128_ADDR_FILE_TABLE_A;

    if (ui_storage_write_record(addr, sequence)) {
        g_storage_sequence = sequence;
        g_storage_dirty = false;
        app_event_clear(APP_EVT_STORAGE_DIRTY);
    }
    app_event_clear(APP_EVT_STORAGE_BUSY);
}

bool ui_storage_save_draw_slot(uint8_t slot)
{
    uint32_t addr;
    size_t offset = 0U;
    bool ok = false;

    if (slot >= UI_FILE_MAX || slot >= APP_W25Q128_DRAW_SLOT_COUNT) {
        return false;
    }
    if (UI_DRAW_SAVE_BYTES > APP_W25Q128_DRAW_SLOT_SIZE) {
        return false;
    }

    app_event_set(APP_EVT_STORAGE_BUSY);

    if (!app_w25q128_is_ready() && app_w25q128_init() != BSP_W25Q128_OK) {
        goto done;
    }

    addr = ui_storage_draw_addr(slot);
    if (app_w25q128_erase_sector(addr) != BSP_W25Q128_OK) {
        goto done;
    }
    if (app_w25q128_write(addr, g_draw_slots[slot], UI_DRAW_SAVE_BYTES) != BSP_W25Q128_OK) {
        goto done;
    }

    while (offset < UI_DRAW_SAVE_BYTES) {
        size_t remain = UI_DRAW_SAVE_BYTES - offset;
        size_t chunk = remain > sizeof(g_storage_verify_chunk) ?
                       sizeof(g_storage_verify_chunk) : remain;

        if (app_w25q128_read(addr + (uint32_t)offset,
                             g_storage_verify_chunk,
                             chunk) != BSP_W25Q128_OK) {
            goto done;
        }
        if (memcmp(g_storage_verify_chunk,
                   &g_draw_slots[slot][offset],
                   chunk) != 0) {
            goto done;
        }
        offset += chunk;
    }

    ok = true;

done:
    app_event_clear(APP_EVT_STORAGE_BUSY);
    return ok;
}

static void ui_storage_load_defaults(void)
{
    memset(g_files, 0, sizeof(g_files));
    memset(g_logs, 0, sizeof(g_logs));
    memset(g_draw_save, 0, sizeof(g_draw_save));
    memset(g_draw_slots, 0, sizeof(g_draw_slots));

    g_settings.cursor_sensitivity = 50;
    g_settings.cursor_size = 12;
    g_settings.screen_brightness = 80;
    g_settings.system_volume = 50;
    g_settings.screen_timeout_s = 60;

    snprintf(g_files[0].name, sizeof(g_files[0].name), "readme.txt");
    snprintf(g_files[0].type, sizeof(g_files[0].type), "txt");
    snprintf(g_files[0].content, sizeof(g_files[0].content), "LVGL file table demo.");
    g_files[0].size = strlen(g_files[0].content);
    g_files[0].storage_addr = APP_W25Q128_FILE_DATA_START;
    g_files[0].state = UI_FILE_STATE_NORMAL;

    snprintf(g_files[1].name, sizeof(g_files[1].name), "paint.bin");
    snprintf(g_files[1].type, sizeof(g_files[1].type), "bin");
    snprintf(g_files[1].content, sizeof(g_files[1].content), "Drawing save slot.");
    g_files[1].size = UI_DRAW_SAVE_BYTES;
    g_files[1].storage_addr = APP_W25Q128_FILE_DATA_START + UI_STORAGE_SLOT_SIZE;
    g_files[1].state = UI_FILE_STATE_NORMAL;

    g_selected_file = 0;
    g_file_serial = 1;
    g_storage_sequence = 0;
    g_storage_dirty = false;
    app_event_clear(APP_EVT_STORAGE_DIRTY);
}

static bool ui_storage_load_flash(void)
{
    bool valid_a;
    bool valid_b;
    const ui_storage_record_t *record;

    if (!app_w25q128_is_ready() && app_w25q128_init() != BSP_W25Q128_OK) {
        return false;
    }

    valid_a = ui_storage_read_record(APP_W25Q128_ADDR_FILE_TABLE_A, &g_storage_record_a);
    valid_b = ui_storage_read_record(APP_W25Q128_ADDR_FILE_TABLE_B, &g_storage_record_b);
    if (!valid_a && !valid_b) {
        return false;
    }

    if (valid_a && valid_b) {
        record = g_storage_record_a.sequence >= g_storage_record_b.sequence ?
                 &g_storage_record_a : &g_storage_record_b;
    } else {
        record = valid_a ? &g_storage_record_a : &g_storage_record_b;
    }

    memcpy(&g_settings, &record->settings, sizeof(g_settings));
    memcpy(g_files, record->files, sizeof(g_files));
    g_selected_file = record->selected_file;
    g_file_serial = record->file_serial;
    g_storage_sequence = record->sequence;
    ui_storage_sanitize_loaded_state();
    (void)ui_storage_read_draw_slots();
    return true;
}

static bool ui_storage_read_record(uint32_t addr, ui_storage_record_t *record)
{
    if (record == NULL) {
        return false;
    }
    memset(record, 0, sizeof(*record));
    if (app_w25q128_read(addr, record, sizeof(*record)) != BSP_W25Q128_OK) {
        return false;
    }

    return ui_storage_validate_record(record);
}

static bool ui_storage_validate_record(const ui_storage_record_t *record)
{
    if (record == NULL) {
        return false;
    }
    if (record->magic != UI_STORAGE_MAGIC ||
        record->version != UI_STORAGE_VERSION ||
        record->header_size != sizeof(ui_storage_record_t)) {
        return false;
    }

    return record->crc32 == ui_storage_record_crc(record);
}

static bool ui_storage_write_record(uint32_t addr, uint32_t sequence)
{
    ui_storage_record_t *record = &g_storage_record_a;
    ui_storage_record_t *verify = &g_storage_record_b;

    memset(record, 0, sizeof(*record));
    record->magic = UI_STORAGE_MAGIC;
    record->version = UI_STORAGE_VERSION;
    record->header_size = sizeof(*record);
    record->sequence = sequence;
    memcpy(&record->settings, &g_settings, sizeof(record->settings));
    memcpy(record->files, g_files, sizeof(record->files));
    record->selected_file = g_selected_file;
    record->file_serial = g_file_serial;
    record->crc32 = ui_storage_record_crc(record);

    if (app_w25q128_erase_sector(addr) != BSP_W25Q128_OK) {
        return false;
    }
    if (app_w25q128_write(addr, record, sizeof(*record)) != BSP_W25Q128_OK) {
        return false;
    }
    memset(verify, 0, sizeof(*verify));
    if (app_w25q128_read(addr, verify, sizeof(*verify)) != BSP_W25Q128_OK) {
        return false;
    }

    return ui_storage_validate_record(verify);
}

static bool ui_storage_read_draw_slots(void)
{
    bool all_ok = true;

    memset(g_draw_slots, 0, sizeof(g_draw_slots));
    for (uint8_t i = 0; i < UI_FILE_MAX; i++) {
        if (g_files[i].state != UI_FILE_STATE_NORMAL) continue;
        if (strcmp(g_files[i].type, "draw") != 0) continue;

        g_files[i].storage_addr = ui_storage_draw_addr(i);
        if (app_w25q128_read(g_files[i].storage_addr,
                             g_draw_slots[i],
                             UI_DRAW_SAVE_BYTES) != BSP_W25Q128_OK) {
            memset(g_draw_slots[i], 0, UI_DRAW_SAVE_BYTES);
            all_ok = false;
        }
    }

    return all_ok;
}

static uint32_t ui_storage_draw_addr(uint8_t slot)
{
    return APP_W25Q128_DRAW_START + (uint32_t)slot * APP_W25Q128_DRAW_SLOT_SIZE;
}

static uint32_t ui_storage_crc32(const uint8_t *data, size_t len)
{
    uint32_t crc = 0xFFFFFFFFU;

    while (len-- > 0U) {
        crc ^= *data++;
        for (uint8_t bit = 0; bit < 8U; bit++) {
            if ((crc & 1U) != 0U) {
                crc = (crc >> 1U) ^ 0xEDB88320U;
            } else {
                crc >>= 1U;
            }
        }
    }

    return ~crc;
}

static uint32_t ui_storage_record_crc(const ui_storage_record_t *record)
{
    return ui_storage_crc32((const uint8_t *)&record->version,
                            offsetof(ui_storage_record_t, crc32) -
                            offsetof(ui_storage_record_t, version));
}

static void ui_storage_sanitize_loaded_state(void)
{
    if (g_settings.cursor_sensitivity < 1U || g_settings.cursor_sensitivity > 100U) {
        g_settings.cursor_sensitivity = 50U;
    }
    if (g_settings.cursor_size < 4U || g_settings.cursor_size > 32U) {
        g_settings.cursor_size = 12U;
    }
    if (g_settings.screen_brightness < 1U || g_settings.screen_brightness > 100U) {
        g_settings.screen_brightness = 80U;
    }
    if (g_settings.system_volume > 100U) {
        g_settings.system_volume = 50U;
    }
    if (g_settings.screen_timeout_s < 10U || g_settings.screen_timeout_s > 300U) {
        g_settings.screen_timeout_s = 60U;
    }

    if (g_selected_file >= UI_FILE_MAX) {
        g_selected_file = 0;
    }
    if (g_file_serial == 0U) {
        g_file_serial = 1U;
    }

    for (uint8_t i = 0; i < UI_FILE_MAX; i++) {
        g_files[i].name[UI_FILE_NAME_LEN - 1U] = '\0';
        g_files[i].type[UI_FILE_TYPE_LEN - 1U] = '\0';
        g_files[i].content[UI_FILE_CONTENT_LEN - 1U] = '\0';
        if (g_files[i].state > UI_FILE_STATE_DELETED) {
            memset(&g_files[i], 0, sizeof(g_files[i]));
        }
        if (g_files[i].state == UI_FILE_STATE_NORMAL &&
            strcmp(g_files[i].type, "draw") == 0) {
            g_files[i].size = UI_DRAW_SAVE_BYTES;
            g_files[i].storage_addr = ui_storage_draw_addr(i);
        }
    }
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
    if (g_settings.screen_brightness == 0U) {
        g_settings.screen_brightness = 80U;
    }

    app_backlight_set_percent(g_settings.screen_brightness);
}
