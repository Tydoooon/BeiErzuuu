#ifndef __APP_BACKLIGHT_H
#define __APP_BACKLIGHT_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

void app_backlight_init(uint8_t percent);
void app_backlight_set_percent(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif
