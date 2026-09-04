#ifndef __APP_MOUSE_H
#define __APP_MOUSE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../LVGL/GUI/lvgl/lvgl.h"
#include "FreeRTOS.h"
#include "queue.h"

#include <stdint.h>

typedef struct {
    int16_t dx;
    int16_t dy;
    uint8_t pressed;
    uint32_t tick;
} mouse_event_t;

void app_mouse_init(void);
void app_mouse_apply_settings(void);
void app_mouse_read(lv_indev_drv_t *drv, lv_indev_data_t *data);
int16_t app_mouse_take_wheel_delta(void);

#ifdef __cplusplus
}
#endif

#endif
