#ifndef __APP_MOUSE_H
#define __APP_MOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../LVGL/GUI/lvgl/lvgl.h"

void app_mouse_init(void);
void app_mouse_read(lv_indev_drv_t *drv, lv_indev_data_t *data);

#ifdef __cplusplus
}
#endif

#endif
