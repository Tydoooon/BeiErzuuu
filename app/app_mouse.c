#include "app_mouse.h"

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
#define APP_MOUSE_SPEED_X_FP  APP_MOUSE_FP_SCALE
#define APP_MOUSE_SPEED_Y_FP  ((APP_MOUSE_FP_SCALE + 2) / 5)
#define APP_MOUSE_SWAP_XY  0
#define APP_MOUSE_EVENT_QUEUE_LEN 16
#define APP_MOUSE_CURSOR_W 14
#define APP_MOUSE_CURSOR_H 20

static lv_indev_t *g_mouse_indev;
static lv_obj_t *g_mouse_cursor;
static lv_color_t g_mouse_cursor_buf[APP_MOUSE_CURSOR_W * APP_MOUSE_CURSOR_H];
static int16_t g_mouse_x;
static int16_t g_mouse_y;
static int32_t g_mouse_x_fp;
static int32_t g_mouse_y_fp;
static bool g_mouse_pressed;
static volatile int32_t g_mouse_dx_acc;
static volatile int32_t g_mouse_dy_acc;
static volatile uint8_t g_mouse_button_raw;

static QueueHandle_t g_mouse_event_queue = NULL;

static int32_t clamp_coord_fp(int32_t value, int16_t max_value);
static void create_cursor(void);
static void fill_cursor_buffer(void);
static void enqueue_mouse_event(const mouse_event_t *evt);
static void apply_mouse_report(void);

void app_mouse_init(void)
{
    static lv_indev_drv_t indev_drv;

    if (g_mouse_event_queue == NULL) {
        g_mouse_event_queue = xQueueCreate(APP_MOUSE_EVENT_QUEUE_LEN, sizeof(mouse_event_t));
    }

    g_mouse_x = (int16_t)(lcddev.width / 2U);
    g_mouse_y = (int16_t)(lcddev.height / 2U);
    g_mouse_x_fp = (int32_t)g_mouse_x * APP_MOUSE_FP_SCALE;
    g_mouse_y_fp = (int32_t)g_mouse_y * APP_MOUSE_FP_SCALE;
    g_mouse_pressed = false;
    g_mouse_dx_acc = 0;
    g_mouse_dy_acc = 0;
    g_mouse_button_raw = 0U;

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.disp = lv_disp_get_default();
    indev_drv.read_cb = app_mouse_read;
    g_mouse_indev = lv_indev_drv_register(&indev_drv);

    create_cursor();
}

void app_mouse_read(lv_indev_drv_t *drv, lv_indev_data_t *data)
{
    (void)drv;

    apply_mouse_report();

    data->point.x = g_mouse_x;
    data->point.y = g_mouse_y;
    data->state = g_mouse_pressed ? LV_INDEV_STATE_PR : LV_INDEV_STATE_REL;
}

//USB鼠标回调
void USBH_HID_EventCallback(USBH_HandleTypeDef *phost)
{
    if (Appli_state != APPLICATION_READY) {
        return;
    }
    if (USBH_HID_GetDeviceType(phost) != HID_MOUSE) {
        return;
    }

    HID_MOUSE_Info_TypeDef *info = USBH_HID_GetMouseInfo(phost);
    if (info == NULL) {
        return;
    }

    mouse_event_t evt;
    evt.dx = (int16_t)(int8_t)info->x;
    evt.dy = (int16_t)(int8_t)info->y;
    evt.pressed = (uint8_t)(info->buttons[0] != 0U);
    evt.tick = xTaskGetTickCount();

    enqueue_mouse_event(&evt);

    // //进入临界区
    // taskENTER_CRITICAL();
    // g_mouse_dx_acc += (int16_t)(int8_t)info->x;
    // g_mouse_dy_acc += (int16_t)(int8_t)info->y;
    // g_mouse_button_raw = (uint8_t)(info->buttons[0] != 0U);
    // taskEXIT_CRITICAL();
}

static void enqueue_mouse_event(const mouse_event_t *evt)
{
    mouse_event_t drop;

    if (g_mouse_event_queue == NULL || evt == NULL) {
        return;
    }

    //队列满
    if (xQueueSend(g_mouse_event_queue, evt, 0) != pdPASS) {
        (void)xQueueReceive(g_mouse_event_queue, &drop, 0);
        (void)xQueueSend(g_mouse_event_queue, evt, 0);
    }
}

static void apply_mouse_report(void)
{
    mouse_event_t evt;
    int32_t dx = 0;
    int32_t dy = 0;
    uint8_t pressed = g_mouse_pressed;

    while (g_mouse_event_queue != NULL &&
           xQueueReceive(g_mouse_event_queue, &evt, 0) == pdPASS) {
        dx += evt.dx;
        dy += evt.dy;
        pressed = evt.pressed;
    }

    if (dx == 0 && dy == 0 && g_mouse_pressed == (pressed != 0U)) {
        return;
    }
    
    // //进入临界区
    // taskENTER_CRITICAL();
    // dx = g_mouse_dx_acc;
    // dy = g_mouse_dy_acc;
    // pressed = g_mouse_button_raw;
    // g_mouse_dx_acc = 0;
    // g_mouse_dy_acc = 0;
    // taskEXIT_CRITICAL();

    if ((dx != 0) || (dy != 0) || (g_mouse_pressed != (pressed != 0U))) {

#if APP_MOUSE_INVERT_X
        dx = -dx;
#endif
#if APP_MOUSE_INVERT_Y
        dy = -dy;
#endif
#if APP_MOUSE_SWAP_XY
        int32_t tmp = dx;
        dx = dy;
        dy = tmp;
#endif

        g_mouse_x_fp = clamp_coord_fp(g_mouse_x_fp + dx * APP_MOUSE_SPEED_X_FP,
                                      (int16_t)lcddev.width);
        g_mouse_y_fp = clamp_coord_fp(g_mouse_y_fp + dy * APP_MOUSE_SPEED_Y_FP,
                                      (int16_t)lcddev.height);
        g_mouse_x = (int16_t)(g_mouse_x_fp >> APP_MOUSE_FP_SHIFT);
        g_mouse_y = (int16_t)(g_mouse_y_fp >> APP_MOUSE_FP_SHIFT);
        g_mouse_pressed = (pressed != 0U);

        if (g_mouse_cursor != NULL) {
            lv_obj_clear_flag(g_mouse_cursor, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(g_mouse_cursor, g_mouse_x, g_mouse_y);
        }
    }
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

    lv_obj_remove_style_all(g_mouse_cursor);
    lv_canvas_set_buffer(g_mouse_cursor, g_mouse_cursor_buf,
                         APP_MOUSE_CURSOR_W, APP_MOUSE_CURSOR_H,
                         LV_IMG_CF_TRUE_COLOR_CHROMA_KEYED);
    fill_cursor_buffer();

    lv_obj_clear_flag(g_mouse_cursor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_mouse_cursor, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(g_mouse_cursor, g_mouse_x, g_mouse_y);
    lv_obj_move_foreground(g_mouse_cursor);
}

static void fill_cursor_buffer(void)
{
    static const char * const shape[APP_MOUSE_CURSOR_H] = {
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

    for (uint8_t y = 0; y < APP_MOUSE_CURSOR_H; y++) {
        for (uint8_t x = 0; x < APP_MOUSE_CURSOR_W; x++) {
            char px = shape[y][x];
            if (px == 'B') {
                g_mouse_cursor_buf[(uint32_t)y * APP_MOUSE_CURSOR_W + x] = black;
            } else if (px == 'W') {
                g_mouse_cursor_buf[(uint32_t)y * APP_MOUSE_CURSOR_W + x] = white;
            } else {
                g_mouse_cursor_buf[(uint32_t)y * APP_MOUSE_CURSOR_W + x] = transparent;
            }
        }
    }
}
