#include "app_mouse.h"

#include "app_ui.h"
#include "bsp_lcd.h"
#include "FreeRTOS.h"
#include "task.h"
#include "usb_host.h"
#include "usbh_hid.h"
#include "usbh_hid_mouse.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_MOUSE_INVERT_X 0
#define APP_MOUSE_INVERT_Y 0
#define APP_MOUSE_FP_SHIFT 8
#define APP_MOUSE_FP_SCALE (1 << APP_MOUSE_FP_SHIFT)
#define APP_MOUSE_DEFAULT_SENSITIVITY 50U
#define APP_MOUSE_SPEED_Y_NUM 1
#define APP_MOUSE_SPEED_Y_DEN 3
#define APP_MOUSE_SWAP_XY  0
#define APP_MOUSE_CURSOR_SRC_W 14
#define APP_MOUSE_CURSOR_SRC_H 20
#define APP_MOUSE_CURSOR_MIN_SIZE 4U
#define APP_MOUSE_CURSOR_MAX_SIZE 32U

static lv_indev_t *g_mouse_indev;
static lv_obj_t *g_mouse_cursor;
static lv_color_t g_mouse_cursor_buf[APP_MOUSE_CURSOR_MAX_SIZE * APP_MOUSE_CURSOR_MAX_SIZE];
static uint8_t g_mouse_cursor_w;
static uint8_t g_mouse_cursor_h;
static uint8_t g_mouse_cursor_size;
static int16_t g_mouse_x;
static int16_t g_mouse_y;
static int32_t g_mouse_x_fp;
static int32_t g_mouse_y_fp;
static bool g_mouse_pressed;
static volatile int16_t g_mouse_wheel_acc;
static volatile int32_t g_mouse_pending_dx;
static volatile int32_t g_mouse_pending_dy;
static volatile uint8_t g_mouse_pending_pressed;
static volatile uint8_t g_mouse_event_pending;

static int32_t clamp_coord_fp(int32_t value, int16_t max_value);
static void create_cursor(void);
static uint8_t get_cursor_size(void);
static uint8_t cursor_width_from_size(uint8_t size);
static int32_t scale_dx_fp(int32_t dx);
static int32_t scale_dy_fp(int32_t dy);
static void fill_cursor_buffer(void);
static void apply_mouse_report(void);

void app_mouse_init(void)
{
    static lv_indev_drv_t indev_drv;

    g_mouse_x = (int16_t)(lcddev.width / 2U);
    g_mouse_y = (int16_t)(lcddev.height / 2U);
    g_mouse_x_fp = (int32_t)g_mouse_x * APP_MOUSE_FP_SCALE;
    g_mouse_y_fp = (int32_t)g_mouse_y * APP_MOUSE_FP_SCALE;
    g_mouse_pressed = false;
    g_mouse_wheel_acc = 0;
    g_mouse_pending_dx = 0;
    g_mouse_pending_dy = 0;
    g_mouse_pending_pressed = 0U;
    g_mouse_event_pending = 0U;

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = lv_disp_get_default();
    indev_drv.read_cb = app_mouse_read;
    g_mouse_indev = lv_indev_drv_register(&indev_drv);

    create_cursor();
}

void app_mouse_apply_settings(void)
{
    uint8_t size = get_cursor_size();

    if (g_mouse_cursor == NULL || size == g_mouse_cursor_size) {
        return;
    }

    g_mouse_cursor_size = size;
    g_mouse_cursor_h = size;
    g_mouse_cursor_w = cursor_width_from_size(size);

    lv_canvas_set_buffer(g_mouse_cursor, g_mouse_cursor_buf,
                         g_mouse_cursor_w, g_mouse_cursor_h,
                         LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED);
    lv_obj_set_size(g_mouse_cursor, g_mouse_cursor_w, g_mouse_cursor_h);
    fill_cursor_buffer();
    lv_obj_set_pos(g_mouse_cursor, g_mouse_x, g_mouse_y);
    lv_obj_invalidate(g_mouse_cursor);
}

void app_mouse_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    app_mouse_apply_settings();
    apply_mouse_report();

    data->point.x = g_mouse_x;
    data->point.y = g_mouse_y;
    data->state = g_mouse_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

int16_t app_mouse_take_wheel_delta(void)
{
    int16_t wheel;

    taskENTER_CRITICAL();
    wheel = g_mouse_wheel_acc;
    g_mouse_wheel_acc = 0;
    taskEXIT_CRITICAL();

    return wheel;
}

void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
    HID_MOUSE_Info_TypeDef *info;
    int16_t dx;
    int16_t dy;
    int16_t wheel;

    if (Appli_state != APPLICATION_READY) {
        return;
    }
    if (USBH_HID_GetDeviceType(phost) != HID_MOUSE) {
        return;
    }

    info = USBH_HID_GetMouseInfo(phost);
    if (info == NULL) {
        return;
    }

    dx = (int16_t)(int8_t)info->x;
    dy = (int16_t)(int8_t)info->y;
    wheel = (int16_t)info->wheel;
    if (wheel != 0) {
        dx = 0;
        dy = 0;
    }

    taskENTER_CRITICAL();
    if (wheel != 0) {
        g_mouse_wheel_acc += wheel;
    }
    g_mouse_pending_dx += dx;
    g_mouse_pending_dy += dy;
    g_mouse_pending_pressed = (uint8_t)(info->buttons[0] != 0U);
    g_mouse_event_pending = 1U;
    taskEXIT_CRITICAL();
}

static void apply_mouse_report(void)
{
    int32_t dx = 0;
    int32_t dy = 0;
    uint8_t pressed = g_mouse_pressed;

    taskENTER_CRITICAL();
    if (g_mouse_event_pending != 0U) {
        dx = g_mouse_pending_dx;
        dy = g_mouse_pending_dy;
        pressed = g_mouse_pending_pressed;
        g_mouse_pending_dx = 0;
        g_mouse_pending_dy = 0;
        g_mouse_event_pending = 0U;
    }
    taskEXIT_CRITICAL();

    if (dx == 0 && dy == 0 && g_mouse_pressed == (pressed != 0U)) {
        return;
    }

    app_ui_notify_activity();

#if APP_MOUSE_INVERT_X
    dx = -dx;
#endif
#if APP_MOUSE_INVERT_Y
    dy = -dy;
#endif
#if APP_MOUSE_SWAP_XY
    {
        int32_t tmp = dx;
        dx = dy;
        dy = tmp;
    }
#endif

    g_mouse_x_fp = clamp_coord_fp(g_mouse_x_fp + scale_dx_fp(dx),
                                  (int16_t)lcddev.width);
    g_mouse_y_fp = clamp_coord_fp(g_mouse_y_fp + scale_dy_fp(dy),
                                  (int16_t)lcddev.height);
    g_mouse_x = (int16_t)(g_mouse_x_fp >> APP_MOUSE_FP_SHIFT);
    g_mouse_y = (int16_t)(g_mouse_y_fp >> APP_MOUSE_FP_SHIFT);
    g_mouse_pressed = (pressed != 0U);

    if (g_mouse_cursor != NULL) {
        lv_obj_clear_flag(g_mouse_cursor, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_pos(g_mouse_cursor, g_mouse_x, g_mouse_y);
    }
}

static uint8_t get_cursor_size(void)
{
    uint8_t size = app_ui_get_cursor_size();

    if (size < APP_MOUSE_CURSOR_MIN_SIZE) {
        return APP_MOUSE_CURSOR_MIN_SIZE;
    }
    if (size > APP_MOUSE_CURSOR_MAX_SIZE) {
        return APP_MOUSE_CURSOR_MAX_SIZE;
    }

    return size;
}

static uint8_t cursor_width_from_size(uint8_t size)
{
    uint8_t width = (uint8_t)(((uint16_t)size * APP_MOUSE_CURSOR_SRC_W +
                               APP_MOUSE_CURSOR_SRC_H / 2U) /
                              APP_MOUSE_CURSOR_SRC_H);

    return width < APP_MOUSE_CURSOR_MIN_SIZE ? APP_MOUSE_CURSOR_MIN_SIZE : width;
}

static int32_t scale_dx_fp(int32_t dx)
{
    uint8_t sensitivity = app_ui_get_cursor_sensitivity();

    return (dx * (int32_t)sensitivity * APP_MOUSE_FP_SCALE) /
           (int32_t)APP_MOUSE_DEFAULT_SENSITIVITY;
}

static int32_t scale_dy_fp(int32_t dy)
{
    return (scale_dx_fp(dy) * APP_MOUSE_SPEED_Y_NUM) / APP_MOUSE_SPEED_Y_DEN;
}

static int32_t clamp_coord_fp(int32_t value, int16_t max_value)
{
    int32_t max_fp;

    if (value < 0) {
        return 0;
    }
    max_fp = (int32_t)(max_value - 1) * APP_MOUSE_FP_SCALE;
    if (value > max_fp) {
        return max_fp;
    }
    return value;
}

static void create_cursor(void)
{
    lv_obj_t *sys_layer = lv_disp_get_layer_sys(lv_disp_get_default());

    if (sys_layer == NULL) {
        return;
    }

    g_mouse_cursor = lv_canvas_create(sys_layer);
    if (g_mouse_cursor == NULL) {
        return;
    }

    g_mouse_cursor_size = get_cursor_size();
    g_mouse_cursor_h = g_mouse_cursor_size;
    g_mouse_cursor_w = cursor_width_from_size(g_mouse_cursor_size);

    lv_obj_remove_style_all(g_mouse_cursor);
    lv_canvas_set_buffer(g_mouse_cursor, g_mouse_cursor_buf,
                         g_mouse_cursor_w, g_mouse_cursor_h,
                         LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED);
    lv_obj_set_size(g_mouse_cursor, g_mouse_cursor_w, g_mouse_cursor_h);
    fill_cursor_buffer();

    lv_obj_clear_flag(g_mouse_cursor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_mouse_cursor, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(g_mouse_cursor, g_mouse_x, g_mouse_y);
    lv_obj_move_foreground(g_mouse_cursor);
}

static void fill_cursor_buffer(void)
{
    static const char * const shape[APP_MOUSE_CURSOR_SRC_H] = {
        "W.............",
        "WB............",
        "WBB...........",
        "WBBB..........",
        "WBBBB.........",
        "WBBBBB........",
        "WBBBBBB.......",
        "WBBBBBBB......",
        "WBBBBBBBB.....",
        "WBBBBBWWWW....",
        "WBBBW.........",
        "WBBW..........",
        "WB.WB.........",
        "W..WB.........",
        "...WBB........",
        "...WBB........",
        "....WBB.......",
        "....WBB.......",
        ".....WW.......",
        "..............",
    };
    lv_color_t transparent = LV_COLOR_CHROMA_KEY;
    lv_color_t black = lv_color_black();
    lv_color_t white = lv_color_white();

    for (uint16_t i = 0U; i < (uint16_t)APP_MOUSE_CURSOR_MAX_SIZE * APP_MOUSE_CURSOR_MAX_SIZE; i++) {
        g_mouse_cursor_buf[i] = transparent;
    }

    for (uint8_t y = 0; y < g_mouse_cursor_h; y++) {
        uint8_t src_y = (uint8_t)(((uint16_t)y * APP_MOUSE_CURSOR_SRC_H) / g_mouse_cursor_h);

        for (uint8_t x = 0; x < g_mouse_cursor_w; x++) {
            uint8_t src_x = (uint8_t)(((uint16_t)x * APP_MOUSE_CURSOR_SRC_W) / g_mouse_cursor_w);
            char px = shape[src_y][src_x];

            if (px == 'B') {
                g_mouse_cursor_buf[(uint32_t)y * g_mouse_cursor_w + x] = black;
            } else if (px == 'W') {
                g_mouse_cursor_buf[(uint32_t)y * g_mouse_cursor_w + x] = white;
            } else {
                g_mouse_cursor_buf[(uint32_t)y * g_mouse_cursor_w + x] = transparent;
            }
        }
    }
}
