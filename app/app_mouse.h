#ifndef __APP_MOUSE_H
#define __APP_MOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../LVGL/GUI/lvgl/lvgl.h"

#include <stdbool.h>
#include <stdint.h>

void app_mouse_init(void);
void app_mouse_apply_settings(void);
void app_mouse_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
int16_t app_mouse_take_wheel_delta(void);
int16_t app_mouse_get_x(void);
int16_t app_mouse_get_y(void);
bool app_mouse_take_right_click(void);

#ifdef __cplusplus
}
#endif

#endif
