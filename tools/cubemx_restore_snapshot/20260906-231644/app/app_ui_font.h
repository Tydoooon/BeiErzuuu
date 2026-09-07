#ifndef __APP_UI_FONT_H
#define __APP_UI_FONT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../LVGL/GUI/lvgl/lvgl.h"

void ui_font_init(void);
const lv_font_t *ui_font_get(void);

#ifdef __cplusplus
}
#endif

#endif
