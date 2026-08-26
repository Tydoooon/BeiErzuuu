#include "app_mouse.h"

#include "bsp_lcd.h"
#include "usb_host.h"
#include "usbh_hid.h"
#include "usbh_hid_mouse.h"

#include <stdbool.h>
#include <stdint.h>

#define APP_MOUSE_INVERT_X 0
#define APP_MOUSE_INVERT_Y 0
#define APP_MOUSE_SPEED_X  1
#define APP_MOUSE_SPEED_Y  0.2
#define APP_MOUSE_SWAP_XY  0

static lv_indev_t *g_mouse_indev;
static lv_obj_t *g_mouse_cursor;
static int16_t g_mouse_x;
static int16_t g_mouse_y;
static bool g_mouse_pressed;
static volatile uint32_t g_mouse_seq;
static uint32_t g_mouse_seq_seen;
static volatile int16_t g_mouse_dx_raw;
static volatile int16_t g_mouse_dy_raw;
static volatile uint8_t g_mouse_button_raw;

static int16_t clamp_coord(int16_t value, int16_t max_value);
static void create_cursor(void);
static void apply_mouse_report(void);

void app_mouse_init(void)
{
    static lv_indev_drv_t indev_drv;

    g_mouse_x = (int16_t)(lcddev.width / 2U);
    g_mouse_y = (int16_t)(lcddev.height / 2U);
    g_mouse_pressed = false;
    g_mouse_seq_seen = 0U;

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

    g_mouse_dx_raw = (int16_t)(int8_t)info->x;
    g_mouse_dy_raw = (int16_t)(int8_t)info->y;
    g_mouse_button_raw = (uint8_t)(info->buttons[0] != 0U);
    g_mouse_seq++;
}

static void apply_mouse_report(void)
{
    uint32_t seq_before;
    uint32_t seq_after;
    int16_t dx;
    int16_t dy;
    uint8_t pressed;

    do {
        seq_before = g_mouse_seq;
        dx = g_mouse_dx_raw;
        dy = g_mouse_dy_raw;
        pressed = g_mouse_button_raw;
        seq_after = g_mouse_seq;
    } while (seq_before != seq_after);

    if (seq_after != g_mouse_seq_seen) {
        g_mouse_seq_seen = seq_after;

#if APP_MOUSE_INVERT_X
        dx = (int16_t)-dx;
#endif
#if APP_MOUSE_INVERT_Y
        dy = (int16_t)-dy;
#endif
#if APP_MOUSE_SWAP_XY
        int16_t tmp = dx;
        dx = dy;
        dy = tmp;
#endif

        g_mouse_x = clamp_coord((int16_t)(g_mouse_x + dx * APP_MOUSE_SPEED_X),
                                (int16_t)lcddev.width);
        g_mouse_y = clamp_coord((int16_t)(g_mouse_y + dy * APP_MOUSE_SPEED_Y),
                                (int16_t)lcddev.height);
        g_mouse_pressed = (pressed != 0U);

        if (g_mouse_cursor != NULL) {
            lv_obj_clear_flag(g_mouse_cursor, LV_OBJ_FLAG_HIDDEN);
            lv_obj_set_pos(g_mouse_cursor, g_mouse_x, g_mouse_y);
            lv_obj_move_foreground(g_mouse_cursor);
        }
    }
}

static int16_t clamp_coord(int16_t value, int16_t max_value)
{
    if (value < 0) {
        return 0;
    }
    if (value >= max_value) {
        return (int16_t)(max_value - 1);
    }
    return value;
}

static void create_cursor(void)
{
    lv_obj_t *sys_layer = lv_disp_get_layer_sys(lv_disp_get_default());

    if (sys_layer == NULL) {
        return;
    }

    g_mouse_cursor = lv_obj_create(sys_layer);
    if (g_mouse_cursor == NULL) {
        return;
    }

    lv_obj_remove_style_all(g_mouse_cursor);
    lv_obj_set_size(g_mouse_cursor, 8, 12);
    lv_obj_set_style_bg_color(g_mouse_cursor, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(g_mouse_cursor, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(g_mouse_cursor, lv_color_white(), 0);
    lv_obj_set_style_border_width(g_mouse_cursor, 1, 0);
    lv_obj_set_style_radius(g_mouse_cursor, 0, 0);
    lv_obj_clear_flag(g_mouse_cursor, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_flag(g_mouse_cursor, LV_OBJ_FLAG_IGNORE_LAYOUT | LV_OBJ_FLAG_FLOATING);
    lv_obj_set_pos(g_mouse_cursor, g_mouse_x, g_mouse_y);
}
