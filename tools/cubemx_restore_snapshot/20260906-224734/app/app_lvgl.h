#ifndef __APP_LVGL_H
#define __APP_LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../LVGL/GUI/lvgl/lvgl.h"

/**
 * @brief 初始化 LVGL 显示端口，并把 LVGL 的绘图缓冲区接到当前 LCD 驱动。
 */
void lv_port_disp_init(void);

#ifdef __cplusplus
}
#endif

#endif
