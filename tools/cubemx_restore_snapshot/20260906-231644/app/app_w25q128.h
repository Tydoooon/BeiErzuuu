#ifndef __APP_W25Q128_H
#define __APP_W25Q128_H

#ifdef __cplusplus
extern "C" {
#endif

#include "bsp_w25q128.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define APP_W25Q128_ADDR_SYS_A          0x000000U
#define APP_W25Q128_ADDR_SYS_B          0x001000U
#define APP_W25Q128_ADDR_SETTINGS_A     0x002000U
#define APP_W25Q128_ADDR_SETTINGS_B     0x003000U
#define APP_W25Q128_ADDR_FILE_TABLE_A   0x004000U
#define APP_W25Q128_ADDR_FILE_TABLE_B   0x005000U
#define APP_W25Q128_ADDR_LOG_META_A     0x006000U
#define APP_W25Q128_ADDR_LOG_META_B     0x007000U
#define APP_W25Q128_ADDR_DRAW_META_A    0x008000U
#define APP_W25Q128_ADDR_DRAW_META_B    0x009000U
#define APP_W25Q128_ADDR_MONITOR_A      0x00A000U
#define APP_W25Q128_ADDR_MONITOR_B      0x00B000U
#define APP_W25Q128_ADDR_RESERVED_META  0x00C000U
#define APP_W25Q128_META_END            0x010000U

#define APP_W25Q128_LOG_START           0x010000U
#define APP_W25Q128_LOG_SIZE            0x010000U
#define APP_W25Q128_LOG_END             (APP_W25Q128_LOG_START + APP_W25Q128_LOG_SIZE)

#define APP_W25Q128_DRAW_START          0x020000U
#define APP_W25Q128_DRAW_SIZE           0x010000U
#define APP_W25Q128_DRAW_END            (APP_W25Q128_DRAW_START + APP_W25Q128_DRAW_SIZE)
#define APP_W25Q128_DRAW_SLOT_SIZE      W25Q128_SECTOR_SIZE
#define APP_W25Q128_DRAW_SLOT_COUNT     (APP_W25Q128_DRAW_SIZE / APP_W25Q128_DRAW_SLOT_SIZE)

#define APP_W25Q128_MUSIC_START         0x030000U
#define APP_W25Q128_MUSIC_SIZE          0x010000U
#define APP_W25Q128_MUSIC_END           (APP_W25Q128_MUSIC_START + APP_W25Q128_MUSIC_SIZE)

#define APP_W25Q128_MONITOR_START       0x040000U
#define APP_W25Q128_MONITOR_SIZE        0x010000U
#define APP_W25Q128_MONITOR_END         (APP_W25Q128_MONITOR_START + APP_W25Q128_MONITOR_SIZE)

#define APP_W25Q128_RESOURCE_START      0x050000U
#define APP_W25Q128_RESOURCE_SIZE       0x0B0000U
#define APP_W25Q128_RESOURCE_END        (APP_W25Q128_RESOURCE_START + APP_W25Q128_RESOURCE_SIZE)

#define APP_W25Q128_FONT16_HEADER_ADDR  APP_W25Q128_RESOURCE_START
#define APP_W25Q128_FONT16_DATA_ADDR    (APP_W25Q128_FONT16_HEADER_ADDR + W25Q128_SECTOR_SIZE)
#define APP_W25Q128_FONT16_MAX_SIZE     0x020000U

#define APP_W25Q128_FILE_DATA_START     0x100000U
#define APP_W25Q128_FILE_DATA_SIZE      0xE00000U
#define APP_W25Q128_FILE_DATA_END       (APP_W25Q128_FILE_DATA_START + APP_W25Q128_FILE_DATA_SIZE)

#define APP_W25Q128_RESERVED_START      0xF00000U
#define APP_W25Q128_RESERVED_SIZE       0x0FF000U
#define APP_W25Q128_RESERVED_END        (APP_W25Q128_RESERVED_START + APP_W25Q128_RESERVED_SIZE)

#define APP_W25Q128_TEST_ADDR           0xFFF000U

#define APP_W25Q128_RECORD_MAGIC        0x425A5731U
#define APP_W25Q128_RECORD_VERSION      1U
#define APP_W25Q128_FONT_MAGIC          0x46545731U
#define APP_W25Q128_FONT_VERSION        1U

//临时烧录字库，1烧/0不烧
#define APP_W25Q128_FONT_PROGRAMMER_ENABLE 0U

typedef struct
{
    bool ready;
    uint32_t jedec_id;
    uint32_t size_bytes;
    uint32_t sector_size;
    uint32_t page_size;
} app_w25q128_info_t;

typedef struct
{
    uint32_t magic;
    uint16_t version;
    uint16_t header_size;
    uint32_t data_addr;
    uint32_t data_size;
    uint32_t data_crc32;
    char name[32];
} app_w25q128_font_header_t;

bsp_w25q128_status_t app_w25q128_init(void);
bool app_w25q128_is_ready(void);
const app_w25q128_info_t *app_w25q128_get_info(void);

bsp_w25q128_status_t app_w25q128_read(uint32_t addr, void *data, size_t len);
bsp_w25q128_status_t app_w25q128_write(uint32_t addr, const void *data, size_t len);
bsp_w25q128_status_t app_w25q128_erase_sector(uint32_t addr);

bsp_w25q128_status_t app_w25q128_self_test(void);
bsp_w25q128_status_t app_w25q128_program_ui_font_image(void);
bsp_w25q128_status_t app_w25q128_verify_ui_font_image(void);
bsp_w25q128_status_t app_w25q128_get_last_font_program_status(void);

#ifdef __cplusplus
}
#endif

#endif
