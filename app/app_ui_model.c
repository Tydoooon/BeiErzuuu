#include "app_ui_internal.h"

#include "app_backlight.h"
#include "app_fault.h"
#include "queue.h"

#include <stddef.h>

#define UI_STORAGE_MAGIC               0x31495355U
#define UI_STORAGE_VERSION             1U
#define UI_STORAGE_RESULT_QUEUE_LEN    4U

#define UI_LOG_MAGIC                   0x314C4F47U  /* "1LOG" */
#define UI_LOG_VERSION                 1U
/* 持久化最多保留最近 N 条日志（完整 72B 文本 × 32 条会占 ~4.9KB RAM，
 * 两个 A/B 记录更翻倍；掉电保护只需保留最近若干条即可）。 */
#define UI_LOG_PERSIST_MAX             16U

/* 存储任务请求类型：单槽队列承载，数据本身在各自的静态快照里 */
typedef enum {
    UI_STORAGE_REQ_COMMIT = 0,
    UI_STORAGE_REQ_LOG_SAVE,
} ui_storage_request_t;

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

typedef struct {
    ui_settings_t settings;
    ui_file_entry_t files[UI_FILE_MAX];
    uint8_t selected_file;
    uint8_t file_serial;
    uint8_t draw_slot;
    bool has_draw;
    uint32_t feedback;
    uint8_t draw_data[UI_DRAW_SAVE_BYTES];
} ui_storage_snapshot_t;

/* 日志持久化记录（双 A/B + CRC32），掉电不丢失 */
typedef struct {
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t sequence;
    uint8_t count;
    uint8_t reserved[3];
    ui_log_entry_t entries[UI_LOG_PERSIST_MAX];
    uint32_t crc32;
} ui_log_record_t;

static uint32_t g_storage_sequence;
static ui_storage_record_t g_storage_record_a;
static ui_storage_record_t g_storage_record_b;
static uint8_t g_storage_verify_chunk[256];
static ui_storage_snapshot_t g_storage_pending_snapshot __attribute__((section(".ccmram")));
static ui_storage_snapshot_t g_storage_active_snapshot __attribute__((section(".ccmram")));
static ui_log_record_t g_log_record_a;
static ui_log_record_t g_log_record_b;
static uint32_t g_log_sequence;
static bool g_storage_request_pending;
static QueueHandle_t g_storage_request_queue;
static QueueHandle_t g_storage_result_queue;
static StaticQueue_t g_storage_request_queue_buffer;
static StaticQueue_t g_storage_result_queue_buffer;
static ui_storage_request_t g_storage_request_queue_storage[1U];
static uint8_t g_storage_result_queue_storage[UI_STORAGE_RESULT_QUEUE_LEN * sizeof(ui_storage_result_t)];
static bool g_storage_retry_active;
static bool g_storage_retry_needs_draw;

static void ui_storage_load_defaults(void);
static bool ui_storage_load_flash(void);
static bool ui_storage_read_record(uint32_t addr, ui_storage_record_t *record);
static bool ui_storage_validate_record(const ui_storage_record_t *record);
static bool ui_storage_write_record(uint32_t addr, uint32_t sequence,
                                    const ui_storage_snapshot_t *snapshot);
static bool ui_storage_write_draw_slot(uint8_t slot, const uint8_t *data);
static bool ui_storage_read_draw_slots(void);
static uint32_t ui_storage_draw_addr(uint8_t slot);
static uint32_t ui_storage_crc32(const uint8_t *data, size_t len);
static uint32_t ui_storage_record_crc(const ui_storage_record_t *record);
static void ui_storage_sanitize_loaded_state(void);
static bool ui_storage_queue_snapshot(uint32_t feedback, bool include_draw, uint8_t draw_slot);
static bool ui_storage_take_pending_snapshot(ui_storage_snapshot_t *snapshot);
static void ui_storage_publish_result(uint32_t feedback, bool success);
static void ui_storage_sync_dirty_event(void);
static bool ui_log_load_persisted(void);
static void ui_log_save_to_flash(void);
static bool ui_log_read_record(uint32_t addr, ui_log_record_t *record);
static bool ui_log_validate_record(const ui_log_record_t *record);
static uint32_t ui_log_record_crc(const ui_log_record_t *record);

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
    g_storage_request_pending = false;
    g_storage_retry_active = false;
    g_storage_retry_needs_draw = false;
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

    /* 加载持久化日志（掉电不丢失），新会话再追加一条启动日志 */
    if (ui_log_load_persisted()) {
        ui_log(UI_TEXT_SYSTEM_START);
    }
    return true;
}

void app_ui_storage_service_init(void)
{
    if (g_storage_request_queue == NULL) {
        g_storage_request_queue = xQueueCreateStatic(1U,
                                                     sizeof(ui_storage_request_t),
                                                     g_storage_request_queue_storage,
                                                     &g_storage_request_queue_buffer);
    }
    if (g_storage_result_queue == NULL) {
        g_storage_result_queue = xQueueCreateStatic(UI_STORAGE_RESULT_QUEUE_LEN,
                                                    sizeof(ui_storage_result_t),
                                                    g_storage_result_queue_storage,
                                                    &g_storage_result_queue_buffer);
    }
}

void app_ui_storage_task(void *argument)
{
    ui_storage_request_t req;
    (void)argument;

    app_ui_storage_service_init();

    for (;;) {
        /* 有限超时等待写入请求，超时也要喂心跳，供故障监控任务判断存活 */
        if (xQueueReceive(g_storage_request_queue, &req, pdMS_TO_TICKS(200U)) != pdPASS) {
            app_fault_heartbeat(APP_FAULT_TASK_SAVE);
            continue;
        }

        app_fault_heartbeat(APP_FAULT_TASK_SAVE);

        if (req == UI_STORAGE_REQ_LOG_SAVE) {
            app_event_set(APP_EVT_STORAGE_BUSY);
            ui_log_save_to_flash();
            app_event_clear(APP_EVT_STORAGE_BUSY);
            continue;
        }

        app_event_set(APP_EVT_STORAGE_BUSY);
        for (;;) {
            uint32_t sequence;
            uint32_t addr;
            bool success = true;
            bool retrying = g_storage_retry_active;

            if (!retrying &&
                !ui_storage_take_pending_snapshot(&g_storage_active_snapshot)) {
                break;
            }
            if (!retrying) {
                g_storage_retry_needs_draw = g_storage_active_snapshot.has_draw;
            }

            if (sizeof(ui_storage_record_t) > W25Q128_SECTOR_SIZE) {
                success = false;
            }
            if (success &&
                !app_w25q128_is_ready() &&
                app_w25q128_init() != BSP_W25Q128_OK) {
                success = false;
            }
            if (success &&
                g_storage_active_snapshot.has_draw &&
                (!retrying || g_storage_retry_needs_draw)) {
                success = ui_storage_write_draw_slot(
                    g_storage_active_snapshot.draw_slot,
                    g_storage_active_snapshot.draw_data);
                g_storage_retry_needs_draw = !success;
            }

            sequence = g_storage_sequence + 1U;
            addr = (sequence & 1U) != 0U ?
                   APP_W25Q128_ADDR_FILE_TABLE_B :
                   APP_W25Q128_ADDR_FILE_TABLE_A;
            if (success) {
                success = ui_storage_write_record(addr,
                                                  sequence,
                                                  &g_storage_active_snapshot);
            }

            taskENTER_CRITICAL();
            if (success) {
                g_storage_retry_active = false;
                g_storage_retry_needs_draw = false;
                g_storage_sequence = sequence;
                if (!g_storage_request_pending) {
                    g_storage_dirty = false;
                }
            } else {
                g_storage_retry_active = true;
            }
            taskEXIT_CRITICAL();

            ui_storage_sync_dirty_event();
            ui_storage_publish_result(g_storage_active_snapshot.feedback, success);
            if (!success) {
                g_storage_active_snapshot.feedback = UI_STORAGE_FEEDBACK_NONE;
                break;
            }
        }
        app_event_clear(APP_EVT_STORAGE_BUSY);
    }
}

bool ui_storage_request_commit(uint32_t feedback)
{
    return ui_storage_queue_snapshot(feedback, false, 0U);
}

bool ui_storage_request_draw(uint8_t slot, uint32_t feedback)
{
    if (slot >= UI_FILE_MAX || slot >= APP_W25Q128_DRAW_SLOT_COUNT) {
        return false;
    }

    return ui_storage_queue_snapshot(feedback, true, slot);
}

bool ui_storage_draw_request_available(void)
{
    bool available;

    taskENTER_CRITICAL();
    available = !g_storage_request_pending || !g_storage_pending_snapshot.has_draw;
    taskEXIT_CRITICAL();

    return available;
}

bool ui_storage_take_result(ui_storage_result_t *result)
{
    if (result == NULL || g_storage_result_queue == NULL) {
        return false;
    }

    return xQueueReceive(g_storage_result_queue, result, 0U) == pdPASS;
}

static bool ui_storage_queue_snapshot(uint32_t feedback, bool include_draw, uint8_t draw_slot)
{
    ui_storage_request_t wake = UI_STORAGE_REQ_COMMIT;
    bool accepted = false;

    app_ui_storage_service_init();
    if (g_storage_request_queue == NULL) {
        return false;
    }

    vTaskSuspendAll();
    if (!include_draw ||
        !g_storage_request_pending ||
        !g_storage_pending_snapshot.has_draw) {
        if (!g_storage_request_pending) {
            g_storage_pending_snapshot.feedback = UI_STORAGE_FEEDBACK_NONE;
            g_storage_pending_snapshot.has_draw = false;
        }
        memcpy(&g_storage_pending_snapshot.settings, &g_settings, sizeof(g_settings));
        memcpy(g_storage_pending_snapshot.files, g_files, sizeof(g_files));
        g_storage_pending_snapshot.selected_file = g_selected_file;
        g_storage_pending_snapshot.file_serial = g_file_serial;
        g_storage_pending_snapshot.feedback |= feedback;
        if (include_draw) {
            g_storage_pending_snapshot.draw_slot = draw_slot;
            g_storage_pending_snapshot.has_draw = true;
            memcpy(g_storage_pending_snapshot.draw_data,
                   g_draw_slots[draw_slot],
                   UI_DRAW_SAVE_BYTES);
        }
        g_storage_request_pending = true;
        g_storage_dirty = true;
        accepted = true;
    }
    (void)xTaskResumeAll();

    if (!accepted) {
        return false;
    }

    app_event_set(APP_EVT_STORAGE_DIRTY);
    (void)xQueueOverwrite(g_storage_request_queue, &wake);
    return true;
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

static bool ui_storage_write_record(uint32_t addr, uint32_t sequence,
                                    const ui_storage_snapshot_t *snapshot)
{
    ui_storage_record_t *record = &g_storage_record_a;
    ui_storage_record_t *verify = &g_storage_record_b;

    if (snapshot == NULL) {
        return false;
    }

    memset(record, 0, sizeof(*record));
    record->magic = UI_STORAGE_MAGIC;
    record->version = UI_STORAGE_VERSION;
    record->header_size = sizeof(*record);
    record->sequence = sequence;
    memcpy(&record->settings, &snapshot->settings, sizeof(record->settings));
    memcpy(record->files, snapshot->files, sizeof(record->files));
    record->selected_file = snapshot->selected_file;
    record->file_serial = snapshot->file_serial;
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

static bool ui_storage_write_draw_slot(uint8_t slot, const uint8_t *data)
{
    uint32_t addr;
    size_t offset = 0U;

    if (data == NULL ||
        slot >= UI_FILE_MAX ||
        slot >= APP_W25Q128_DRAW_SLOT_COUNT ||
        UI_DRAW_SAVE_BYTES > APP_W25Q128_DRAW_SLOT_SIZE) {
        return false;
    }

    addr = ui_storage_draw_addr(slot);
    if (app_w25q128_erase_sector(addr) != BSP_W25Q128_OK) {
        return false;
    }
    if (app_w25q128_write(addr, data, UI_DRAW_SAVE_BYTES) != BSP_W25Q128_OK) {
        return false;
    }

    while (offset < UI_DRAW_SAVE_BYTES) {
        size_t remain = UI_DRAW_SAVE_BYTES - offset;
        size_t chunk = remain > sizeof(g_storage_verify_chunk) ?
                       sizeof(g_storage_verify_chunk) : remain;

        if (app_w25q128_read(addr + (uint32_t)offset,
                             g_storage_verify_chunk,
                             chunk) != BSP_W25Q128_OK) {
            return false;
        }
        if (memcmp(g_storage_verify_chunk, &data[offset], chunk) != 0) {
            return false;
        }
        offset += chunk;
    }

    return true;
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

static bool ui_storage_take_pending_snapshot(ui_storage_snapshot_t *snapshot)
{
    bool available = false;

    if (snapshot == NULL) {
        return false;
    }

    vTaskSuspendAll();
    if (g_storage_request_pending) {
        memcpy(snapshot, &g_storage_pending_snapshot, sizeof(*snapshot));
        g_storage_pending_snapshot.feedback = UI_STORAGE_FEEDBACK_NONE;
        g_storage_pending_snapshot.has_draw = false;
        g_storage_request_pending = false;
        available = true;
    }
    (void)xTaskResumeAll();

    return available;
}

static void ui_storage_publish_result(uint32_t feedback, bool success)
{
    ui_storage_result_t result = {
        .feedback = feedback,
        .success = success,
    };

    if (g_storage_result_queue == NULL || feedback == UI_STORAGE_FEEDBACK_NONE) {
        return;
    }

    if (xQueueSend(g_storage_result_queue, &result, 0U) != pdPASS) {
        ui_storage_result_t discarded;
        (void)xQueueReceive(g_storage_result_queue, &discarded, 0U);
        (void)xQueueSend(g_storage_result_queue, &result, 0U);
    }
}

static void ui_storage_sync_dirty_event(void)
{
    bool dirty;

    taskENTER_CRITICAL();
    dirty = g_storage_dirty;
    taskEXIT_CRITICAL();

    if (dirty) {
        app_event_set(APP_EVT_STORAGE_DIRTY);
        return;
    }

    app_event_clear(APP_EVT_STORAGE_DIRTY);

    taskENTER_CRITICAL();
    dirty = g_storage_dirty;
    taskEXIT_CRITICAL();
    if (dirty) {
        app_event_set(APP_EVT_STORAGE_DIRTY);
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

/* ================================================================== */
/* 日志持久化（掉电不丢失）                                            */
/* ================================================================== */

void ui_log_request_persist(void)
{
    ui_storage_request_t req = UI_STORAGE_REQ_LOG_SAVE;

    app_ui_storage_service_init();
    if (g_storage_request_queue == NULL) {
        return;
    }
    (void)xQueueOverwrite(g_storage_request_queue, &req);
}

static uint32_t ui_log_record_crc(const ui_log_record_t *record)
{
    return ui_storage_crc32((const uint8_t *)&record->version,
                            offsetof(ui_log_record_t, crc32) -
                            offsetof(ui_log_record_t, version));
}

static bool ui_log_validate_record(const ui_log_record_t *record)
{
    if (record == NULL) {
        return false;
    }
    if (record->magic != UI_LOG_MAGIC ||
        record->version != UI_LOG_VERSION ||
        record->header_size != sizeof(ui_log_record_t) ||
        record->count > UI_LOG_PERSIST_MAX) {
        return false;
    }
    return record->crc32 == ui_log_record_crc(record);
}

static bool ui_log_read_record(uint32_t addr, ui_log_record_t *record)
{
    if (record == NULL) {
        return false;
    }
    memset(record, 0, sizeof(*record));
    if (app_w25q128_read(addr, record, sizeof(*record)) != BSP_W25Q128_OK) {
        return false;
    }
    return ui_log_validate_record(record);
}

static bool ui_log_load_persisted(void)
{
    bool valid_a;
    bool valid_b;
    const ui_log_record_t *record;

    if (!app_w25q128_is_ready() && app_w25q128_init() != BSP_W25Q128_OK) {
        return false;
    }

    valid_a = ui_log_read_record(APP_W25Q128_ADDR_LOG_META_A, &g_log_record_a);
    valid_b = ui_log_read_record(APP_W25Q128_ADDR_LOG_META_B, &g_log_record_b);
    if (!valid_a && !valid_b) {
        return false;
    }

    if (valid_a && valid_b) {
        record = g_log_record_a.sequence >= g_log_record_b.sequence ?
                 &g_log_record_a : &g_log_record_b;
    } else {
        record = valid_a ? &g_log_record_a : &g_log_record_b;
    }

    g_log_count = record->count;
    memcpy(g_logs, record->entries, (size_t)record->count * sizeof(g_logs[0]));
    g_log_sequence = record->sequence;
    return true;
}

static void ui_log_save_to_flash(void)
{
    uint32_t sequence = g_log_sequence + 1U;
    uint32_t addr = (sequence & 1U) != 0U ?
                    APP_W25Q128_ADDR_LOG_META_B :
                    APP_W25Q128_ADDR_LOG_META_A;

    if (sizeof(ui_log_record_t) > W25Q128_SECTOR_SIZE) {
        return;
    }
    if (!app_w25q128_is_ready() && app_w25q128_init() != BSP_W25Q128_OK) {
        return;
    }

    /* 快照当前日志：避免写入期间被 UI 追加导致数据撕裂 */
    vTaskSuspendAll();
    {
        ui_log_record_t *record = &g_log_record_a;

        memset(record, 0, sizeof(*record));
        record->magic = UI_LOG_MAGIC;
        record->version = UI_LOG_VERSION;
        record->header_size = sizeof(*record);
        record->sequence = sequence;
        record->count = g_log_count > UI_LOG_PERSIST_MAX ? UI_LOG_PERSIST_MAX : g_log_count;
        /* 仅保留最近的 count 条（内存日志按追加顺序，尾部最新） */
        memcpy(record->entries, &g_logs[g_log_count - record->count],
               (size_t)record->count * sizeof(g_logs[0]));
        record->crc32 = ui_log_record_crc(record);
    }
    (void)xTaskResumeAll();

    if (app_w25q128_erase_sector(addr) != BSP_W25Q128_OK) {
        return;
    }
    if (app_w25q128_write(addr, &g_log_record_a, sizeof(g_log_record_a)) != BSP_W25Q128_OK) {
        return;
    }

    memset(&g_log_record_b, 0, sizeof(g_log_record_b));
    if (app_w25q128_read(addr, &g_log_record_b, sizeof(g_log_record_b)) != BSP_W25Q128_OK ||
        !ui_log_validate_record(&g_log_record_b)) {
        return;
    }

    g_log_sequence = sequence;
}

void ui_apply_brightness(void)
{
    if (g_settings.screen_brightness == 0U) {
        g_settings.screen_brightness = 80U;
    }

    app_backlight_set_percent(g_settings.screen_brightness);
}
