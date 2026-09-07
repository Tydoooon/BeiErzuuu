#include "app_ui_font.h"

#include "app_w25q128.h"
#include "../LVGL/GUI/lvgl/src/font/lv_font_loader.h"
#include "../LVGL/GUI/lvgl/src/misc/lv_fs.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define UI_FONT_FS_LETTER 'W'
#define UI_FONT_PATH      "W:/ui_font_16.bin"

typedef struct
{
    uint32_t pos;
} ui_font_file_t;

static lv_font_t *g_external_font;
static bool g_font_inited;
static bool g_font_fs_registered;
static app_w25q128_font_header_t g_font_header;

static bool ui_font_header_valid(const app_w25q128_font_header_t *header);
static uint32_t ui_font_crc32(const uint8_t *data, uint32_t len, uint32_t crc);
static bool ui_font_check_crc(void);
static void ui_font_register_fs(void);
static bool ui_font_fs_ready(lv_fs_drv_t *drv);
static void *ui_font_fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode);
static lv_fs_res_t ui_font_fs_close(lv_fs_drv_t *drv, void *file_p);
static lv_fs_res_t ui_font_fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br);
static lv_fs_res_t ui_font_fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence);
static lv_fs_res_t ui_font_fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p);

void ui_font_init(void)
{
    if (g_font_inited) {
        return;
    }
    g_font_inited = true;

    if (app_w25q128_init() != BSP_W25Q128_OK) {
        return;
    }

    if (app_w25q128_read(APP_W25Q128_FONT16_HEADER_ADDR, &g_font_header, sizeof(g_font_header)) != BSP_W25Q128_OK) {
        return;
    }

    if (!ui_font_header_valid(&g_font_header) || !ui_font_check_crc()) {
        return;
    }

    ui_font_register_fs();
    g_external_font = lv_font_load(UI_FONT_PATH);
}

const lv_font_t *ui_font_get(void)
{
    if (g_external_font != NULL) {
        return g_external_font;
    }

#if LV_FONT_SIMSUN_16_CJK
    return &lv_font_simsun_16_cjk;
#else
    return LV_FONT_DEFAULT;
#endif
}

static bool ui_font_header_valid(const app_w25q128_font_header_t *header)
{
    if (header->magic != APP_W25Q128_FONT_MAGIC) {
        return false;
    }
    if (header->version != APP_W25Q128_FONT_VERSION) {
        return false;
    }
    if (header->header_size != sizeof(app_w25q128_font_header_t)) {
        return false;
    }
    if (header->data_addr != APP_W25Q128_FONT16_DATA_ADDR) {
        return false;
    }
    if (header->data_size == 0U || header->data_size > APP_W25Q128_FONT16_MAX_SIZE) {
        return false;
    }
    if ((header->data_addr + header->data_size) > APP_W25Q128_RESOURCE_END) {
        return false;
    }

    return true;
}

static uint32_t ui_font_crc32(const uint8_t *data, uint32_t len, uint32_t crc)
{
    crc = ~crc;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t bit = 0; bit < 8U; bit++) {
            crc = (crc >> 1) ^ (0xEDB88320U & (uint32_t)-(int32_t)(crc & 1U));
        }
    }
    return ~crc;
}

static bool ui_font_check_crc(void)
{
    uint8_t buf[128];
    uint32_t remain = g_font_header.data_size;
    uint32_t addr = g_font_header.data_addr;
    uint32_t crc = 0;

    while (remain > 0U) {
        uint32_t chunk = remain > sizeof(buf) ? sizeof(buf) : remain;

        if (app_w25q128_read(addr, buf, chunk) != BSP_W25Q128_OK) {
            return false;
        }
        crc = ui_font_crc32(buf, chunk, crc);
        addr += chunk;
        remain -= chunk;
    }

    return crc == g_font_header.data_crc32;
}

static void ui_font_register_fs(void)
{
    static lv_fs_drv_t drv;

    if (g_font_fs_registered) {
        return;
    }

    lv_fs_drv_init(&drv);
    drv.letter = UI_FONT_FS_LETTER;
    drv.ready_cb = ui_font_fs_ready;
    drv.open_cb = ui_font_fs_open;
    drv.close_cb = ui_font_fs_close;
    drv.read_cb = ui_font_fs_read;
    drv.seek_cb = ui_font_fs_seek;
    drv.tell_cb = ui_font_fs_tell;
    lv_fs_drv_register(&drv);
    g_font_fs_registered = true;
}

static bool ui_font_fs_ready(lv_fs_drv_t *drv)
{
    (void)drv;
    return app_w25q128_is_ready();
}

static void *ui_font_fs_open(lv_fs_drv_t *drv, const char *path, lv_fs_mode_t mode)
{
    static ui_font_file_t file;

    (void)drv;
    if ((mode & LV_FS_MODE_RD) == 0U) {
        return NULL;
    }
    if (path == NULL || (strcmp(path, "/ui_font_16.bin") != 0 && strcmp(path, "ui_font_16.bin") != 0)) {
        return NULL;
    }

    file.pos = 0;
    return &file;
}

static lv_fs_res_t ui_font_fs_close(lv_fs_drv_t *drv, void *file_p)
{
    (void)drv;
    (void)file_p;
    return LV_FS_RES_OK;
}

static lv_fs_res_t ui_font_fs_read(lv_fs_drv_t *drv, void *file_p, void *buf, uint32_t btr, uint32_t *br)
{
    ui_font_file_t *file = (ui_font_file_t *)file_p;
    uint32_t readable;

    (void)drv;
    if (file == NULL || buf == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    readable = btr;
    if ((file->pos + readable) > g_font_header.data_size) {
        readable = g_font_header.data_size - file->pos;
    }

    if (readable > 0U &&
        app_w25q128_read(g_font_header.data_addr + file->pos, buf, readable) != BSP_W25Q128_OK) {
        return LV_FS_RES_HW_ERR;
    }

    file->pos += readable;
    if (br != NULL) {
        *br = readable;
    }

    return LV_FS_RES_OK;
}

static lv_fs_res_t ui_font_fs_seek(lv_fs_drv_t *drv, void *file_p, uint32_t pos, lv_fs_whence_t whence)
{
    ui_font_file_t *file = (ui_font_file_t *)file_p;
    uint32_t new_pos;

    (void)drv;
    if (file == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    if (whence == LV_FS_SEEK_SET) {
        new_pos = pos;
    } else if (whence == LV_FS_SEEK_CUR) {
        new_pos = file->pos + pos;
    } else {
        new_pos = g_font_header.data_size + pos;
    }

    if (new_pos > g_font_header.data_size) {
        return LV_FS_RES_INV_PARAM;
    }

    file->pos = new_pos;
    return LV_FS_RES_OK;
}

static lv_fs_res_t ui_font_fs_tell(lv_fs_drv_t *drv, void *file_p, uint32_t *pos_p)
{
    ui_font_file_t *file = (ui_font_file_t *)file_p;

    (void)drv;
    if (file == NULL || pos_p == NULL) {
        return LV_FS_RES_INV_PARAM;
    }

    *pos_p = file->pos;
    return LV_FS_RES_OK;
}
