/**
 * @file app_lvgl.c
 *
 * LVGL display port for the project LCD.
 */

#include "app_lvgl.h"
#include "bsp_lcd.h"

#define APP_LVGL_DRAW_BUF_LINES 10
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
    int32_t x;
    int32_t y;
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

    Lcd_SetRegion((u16)clipped_area.x1, (u16)clipped_area.y1,
                  (u16)clipped_area.x2, (u16)clipped_area.y2);

    for (y = clipped_area.y1; y <= clipped_area.y2; y++)
    {
        for (x = clipped_area.x1; x <= clipped_area.x2; x++)
        {
            const lv_color_t *pixel = color_p + (y - area->y1) * area_width + (x - area->x1);
            Lcd_WriteData_16Bit((u16)pixel->full);
        }
    }

    lv_disp_flush_ready(disp_drv);
}
