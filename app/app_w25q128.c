#include "app_w25q128.h"

#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"

#include <string.h>

#define APP_W25Q128_PROGRAM_CHUNK_SIZE 256U
#define APP_W25Q128_LOCK_TIMEOUT_MS    1000U

#if APP_W25Q128_FONT_PROGRAMMER_ENABLE
extern const uint8_t g_ui_font_w25q128_img[];
extern const uint8_t g_ui_font_w25q128_img_end[];
#endif

static app_w25q128_info_t g_w25q128_info = {
    .ready = false,
    .jedec_id = 0,
    .size_bytes = W25Q128_SIZE_BYTES,
    .sector_size = W25Q128_SECTOR_SIZE,
    .page_size = W25Q128_PAGE_SIZE,
};
static bsp_w25q128_status_t g_last_font_program_status = BSP_W25Q128_NOT_FOUND;
static StaticSemaphore_t g_w25q128_mutex_buf;
static SemaphoreHandle_t g_w25q128_mutex;

static SemaphoreHandle_t app_w25q128_get_mutex(void);
static bool app_w25q128_lock(void);
static void app_w25q128_unlock(void);

bsp_w25q128_status_t app_w25q128_init(void)
{
    bsp_w25q128_status_t ret;
    uint32_t id = 0;

    if (g_w25q128_info.ready) {
        return BSP_W25Q128_OK;
    }
    if (!app_w25q128_lock()) {
        return BSP_W25Q128_TIMEOUT;
    }
    if (g_w25q128_info.ready) {
        app_w25q128_unlock();
        return BSP_W25Q128_OK;
    }

    ret = BSP_W25Q128_Init();
    if (ret != BSP_W25Q128_OK) {
        g_w25q128_info.ready = false;
        g_w25q128_info.jedec_id = 0;
        app_w25q128_unlock();
        return ret;
    }

    ret = BSP_W25Q128_ReadJedecID(&id);
    if (ret != BSP_W25Q128_OK) {
        g_w25q128_info.ready = false;
        g_w25q128_info.jedec_id = 0;
        app_w25q128_unlock();
        return ret;
    }

    g_w25q128_info.ready = true;
    g_w25q128_info.jedec_id = id;
    app_w25q128_unlock();
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
    bsp_w25q128_status_t ret;

    if (!g_w25q128_info.ready) {
        return BSP_W25Q128_NOT_FOUND;
    }

    if (!app_w25q128_lock()) {
        return BSP_W25Q128_TIMEOUT;
    }
    ret = BSP_W25Q128_Read(addr, (uint8_t *)data, len);
    app_w25q128_unlock();
    return ret;
}

bsp_w25q128_status_t app_w25q128_write(uint32_t addr, const void *data, size_t len)
{
    bsp_w25q128_status_t ret;

    if (!g_w25q128_info.ready) {
        return BSP_W25Q128_NOT_FOUND;
    }

    if (!app_w25q128_lock()) {
        return BSP_W25Q128_TIMEOUT;
    }
    ret = BSP_W25Q128_Write(addr, (const uint8_t *)data, len);
    app_w25q128_unlock();
    return ret;
}

bsp_w25q128_status_t app_w25q128_erase_sector(uint32_t addr)
{
    bsp_w25q128_status_t ret;

    if (!g_w25q128_info.ready) {
        return BSP_W25Q128_NOT_FOUND;
    }

    if (!app_w25q128_lock()) {
        return BSP_W25Q128_TIMEOUT;
    }
    ret = BSP_W25Q128_EraseSector(addr);
    app_w25q128_unlock();
    return ret;
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

#if APP_W25Q128_FONT_PROGRAMMER_ENABLE
static bsp_w25q128_status_t app_w25q128_record_font_program_status(bsp_w25q128_status_t status)
{
    g_last_font_program_status = status;
    return status;
}

static size_t app_w25q128_font_image_size(void)
{
    return (size_t)(g_ui_font_w25q128_img_end - g_ui_font_w25q128_img);
}

static bool app_w25q128_font_image_range_valid(size_t image_size)
{
    if (image_size == 0U || image_size > APP_W25Q128_RESOURCE_SIZE) {
        return false;
    }

    return image_size <= (size_t)(APP_W25Q128_RESOURCE_END - APP_W25Q128_FONT16_HEADER_ADDR);
}

static bsp_w25q128_status_t app_w25q128_erase_range(uint32_t addr, size_t len)
{
    uint32_t erase_addr = addr - (addr % W25Q128_SECTOR_SIZE);
    uint32_t erase_end = addr + (uint32_t)len;
    bsp_w25q128_status_t ret;

    while (erase_addr < erase_end) {
        ret = app_w25q128_erase_sector(erase_addr);
        if (ret != BSP_W25Q128_OK) {
            return ret;
        }
        erase_addr += W25Q128_SECTOR_SIZE;
    }

    return BSP_W25Q128_OK;
}
#endif

bsp_w25q128_status_t app_w25q128_program_ui_font_image(void)
{
#if APP_W25Q128_FONT_PROGRAMMER_ENABLE
    size_t image_size = app_w25q128_font_image_size();
    bsp_w25q128_status_t ret;

    if (!app_w25q128_font_image_range_valid(image_size)) {
        return app_w25q128_record_font_program_status(BSP_W25Q128_INVALID_PARAM);
    }

    if (!g_w25q128_info.ready) {
        ret = app_w25q128_init();
        if (ret != BSP_W25Q128_OK) {
            return app_w25q128_record_font_program_status(ret);
        }
    }

    ret = app_w25q128_verify_ui_font_image();
    if (ret == BSP_W25Q128_OK) {
        return app_w25q128_record_font_program_status(BSP_W25Q128_OK);
    }

    ret = app_w25q128_erase_range(APP_W25Q128_FONT16_HEADER_ADDR, image_size);
    if (ret != BSP_W25Q128_OK) {
        return app_w25q128_record_font_program_status(ret);
    }

    ret = app_w25q128_write(APP_W25Q128_FONT16_HEADER_ADDR, g_ui_font_w25q128_img, image_size);
    if (ret != BSP_W25Q128_OK) {
        return app_w25q128_record_font_program_status(ret);
    }

    return app_w25q128_record_font_program_status(app_w25q128_verify_ui_font_image());
#else
    g_last_font_program_status = BSP_W25Q128_INVALID_PARAM;
    return BSP_W25Q128_INVALID_PARAM;
#endif
}

bsp_w25q128_status_t app_w25q128_verify_ui_font_image(void)
{
#if APP_W25Q128_FONT_PROGRAMMER_ENABLE
    uint8_t readback[APP_W25Q128_PROGRAM_CHUNK_SIZE];
    size_t image_size = app_w25q128_font_image_size();
    size_t offset = 0U;
    bsp_w25q128_status_t ret;

    if (!app_w25q128_font_image_range_valid(image_size)) {
        return BSP_W25Q128_INVALID_PARAM;
    }

    if (!g_w25q128_info.ready) {
        ret = app_w25q128_init();
        if (ret != BSP_W25Q128_OK) {
            return ret;
        }
    }

    while (offset < image_size) {
        size_t remain = image_size - offset;
        size_t chunk = remain > sizeof(readback) ? sizeof(readback) : remain;

        ret = app_w25q128_read(APP_W25Q128_FONT16_HEADER_ADDR + (uint32_t)offset, readback, chunk);
        if (ret != BSP_W25Q128_OK) {
            return ret;
        }

        if (memcmp(g_ui_font_w25q128_img + offset, readback, chunk) != 0) {
            return BSP_W25Q128_ERROR;
        }

        offset += chunk;
    }

    return BSP_W25Q128_OK;
#else
    return BSP_W25Q128_INVALID_PARAM;
#endif
}

bsp_w25q128_status_t app_w25q128_get_last_font_program_status(void)
{
    return g_last_font_program_status;
}

static SemaphoreHandle_t app_w25q128_get_mutex(void)
{
    if (g_w25q128_mutex == NULL) {
        taskENTER_CRITICAL();
        if (g_w25q128_mutex == NULL) {
            g_w25q128_mutex = xSemaphoreCreateMutexStatic(&g_w25q128_mutex_buf);
        }
        taskEXIT_CRITICAL();
    }

    return g_w25q128_mutex;
}

static bool app_w25q128_lock(void)
{
    SemaphoreHandle_t mutex = app_w25q128_get_mutex();

    if (mutex == NULL) {
        return false;
    }
    if (xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return true;
    }

    return xSemaphoreTake(mutex, pdMS_TO_TICKS(APP_W25Q128_LOCK_TIMEOUT_MS)) == pdTRUE;
}

static void app_w25q128_unlock(void)
{
    if (g_w25q128_mutex == NULL ||
        xTaskGetSchedulerState() == taskSCHEDULER_NOT_STARTED) {
        return;
    }

    (void)xSemaphoreGive(g_w25q128_mutex);
}
