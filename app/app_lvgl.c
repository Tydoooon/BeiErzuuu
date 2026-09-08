#include "app_lvgl.h"
#include "bsp_lcd.h"

#define APP_LVGL_DRAW_BUF_LINES 20
#define APP_LVGL_DRAW_BUF_SIZE  (X_MAX_PIXEL * APP_LVGL_DRAW_BUF_LINES)

static void disp_init(void);
static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p);

void lv_port_disp_init(void)
{
    disp_init();

    static lv_disp_draw_buf_t draw_buf_dsc;
    static lv_color_t draw_buf[APP_LVGL_DRAW_BUF_SIZE];
    lv_disp_draw_buf_init(&draw_buf_dsc, draw_buf, NULL, APP_LVGL_DRAW_BUF_SIZE);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = lcddev.width;
    disp_drv.ver_res = lcddev.height;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc;

    lv_disp_drv_register(&disp_drv);
}

static void disp_init(void)
{
    LCD_Init();
    LCD_Clear(BLACK);
}

static void disp_flush(lv_disp_drv_t *disp_drv, const lv_area_t *area, lv_color_t *color_p)
{
    const int32_t area_width = area->x2 - area->x1 + 1;

    if (area->x2 < 0 || area->y2 < 0 ||
        area->x1 >= lcddev.width || area->y1 >= lcddev.height)
    {
        lv_disp_flush_ready(disp_drv);
        return;
    }

    lv_area_t clipped_area = *area;
    if (clipped_area.x1 < 0) clipped_area.x1 = 0;
    if (clipped_area.y1 < 0) clipped_area.y1 = 0;
    if (clipped_area.x2 >= lcddev.width) clipped_area.x2 = lcddev.width - 1;
    if (clipped_area.y2 >= lcddev.height) clipped_area.y2 = lcddev.height - 1;

    const int32_t w = clipped_area.x2 - clipped_area.x1 + 1;
    const int32_t h = clipped_area.y2 - clipped_area.y1 + 1;

    Lcd_SetRegion((u16)clipped_area.x1, (u16)clipped_area.y1,
                  (u16)clipped_area.x2, (u16)clipped_area.y2);

    if (clipped_area.x1 == area->x1 && clipped_area.y1 == area->y1 &&
        clipped_area.x2 == area->x2 && clipped_area.y2 == area->y2)
    {
        /* 区域完全在屏内（LVGL 常规情形）：整块连续流式发送，远快于逐像素 */
        Lcd_WriteColorBulk((const u16 *)color_p, (u32)(w * h));
    }
    else
    {
        /* 区域部分越界（罕见兜底）：逐行发送，源地址按原 area 步进 */
        for (int32_t y = clipped_area.y1; y <= clipped_area.y2; y++)
        {
            const lv_color_t *src = color_p + (y - area->y1) * area_width + (clipped_area.x1 - area->x1);
            Lcd_WriteColorBulk((const u16 *)src, (u32)w);
        }
    }

    lv_disp_flush_ready(disp_drv);
}
