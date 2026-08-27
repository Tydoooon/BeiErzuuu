#include "app_w25q128.h"

#include <string.h>

static app_w25q128_info_t g_w25q128_info = {
    .ready = false,
    .jedec_id = 0,
    .size_bytes = W25Q128_SIZE_BYTES,
    .sector_size = W25Q128_SECTOR_SIZE,
    .page_size = W25Q128_PAGE_SIZE,
};

bsp_w25q128_status_t app_w25q128_init(void)
{
    bsp_w25q128_status_t ret;
    uint32_t id = 0;

    ret = BSP_W25Q128_Init();
    if (ret != BSP_W25Q128_OK) {
        g_w25q128_info.ready = false;
        g_w25q128_info.jedec_id = 0;
        return ret;
    }

    ret = BSP_W25Q128_ReadJedecID(&id);
    if (ret != BSP_W25Q128_OK) {
        g_w25q128_info.ready = false;
        g_w25q128_info.jedec_id = 0;
        return ret;
    }

    g_w25q128_info.ready = true;
    g_w25q128_info.jedec_id = id;
    return BSP_W25Q128_OK;
}

bool app_w25q128_is_ready(void)
{
    return g_w25q128_info.ready;
}

const app_w25q128_info_t *app_w25q128_get_info(void)
{
    return &g_w25q128_info;
}

bsp_w25q128_status_t app_w25q128_read(uint32_t addr, void *data, size_t len)
{
    if (!g_w25q128_info.ready) {
        return BSP_W25Q128_NOT_FOUND;
    }

    return BSP_W25Q128_Read(addr, (uint8_t *)data, len);
}

bsp_w25q128_status_t app_w25q128_write(uint32_t addr, const void *data, size_t len)
{
    if (!g_w25q128_info.ready) {
        return BSP_W25Q128_NOT_FOUND;
    }

    return BSP_W25Q128_Write(addr, (const uint8_t *)data, len);
}

bsp_w25q128_status_t app_w25q128_erase_sector(uint32_t addr)
{
    if (!g_w25q128_info.ready) {
        return BSP_W25Q128_NOT_FOUND;
    }

    return BSP_W25Q128_EraseSector(addr);
}

bsp_w25q128_status_t app_w25q128_self_test(void)
{
    static const uint8_t pattern[] = {
        0x42, 0x65, 0x69, 0x45, 0x72, 0x5A, 0x75, 0x00,
        0xA5, 0x5A, 0x12, 0x21, 0x34, 0x43, 0x56, 0x65,
        0x78, 0x87, 0x9A, 0xA9, 0xBC, 0xCB, 0xDE, 0xED,
        0xF0, 0x0F, 0x55, 0xAA, 0xCC, 0x33, 0x96, 0x69,
    };
    uint8_t readback[sizeof(pattern)];
    bsp_w25q128_status_t ret;

    if (!g_w25q128_info.ready) {
        ret = app_w25q128_init();
        if (ret != BSP_W25Q128_OK) {
            return ret;
        }
    }

    ret = app_w25q128_erase_sector(APP_W25Q128_TEST_ADDR);
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    ret = app_w25q128_write(APP_W25Q128_TEST_ADDR, pattern, sizeof(pattern));
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    memset(readback, 0, sizeof(readback));
    ret = app_w25q128_read(APP_W25Q128_TEST_ADDR, readback, sizeof(readback));
    if (ret != BSP_W25Q128_OK) {
        return ret;
    }

    return memcmp(pattern, readback, sizeof(pattern)) == 0 ? BSP_W25Q128_OK : BSP_W25Q128_ERROR;
}
