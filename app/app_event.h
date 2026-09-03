#ifndef __APP_EVENT_H__
#define __APP_EVENT_H__

#include "FreeRTOS.h"
#include "event_groups.h"
#include <stdbool.h>
#include <stdint.h>

#define APP_EVT_UI_READY       (1U << 0)
#define APP_EVT_FLASH_READY    (1U << 1)
#define APP_EVT_USB_READY      (1U << 2)
#define APP_EVT_SCREEN_SLEEP   (1U << 3)
#define APP_EVT_LOGIN_LOCKED   (1U << 4)
#define APP_EVT_MUSIC_PLAYING  (1U << 5)
#define APP_EVT_STORAGE_DIRTY  (1U << 6)
#define APP_EVT_STORAGE_BUSY   (1U << 7)

extern EventGroupHandle_t g_app_events;

void app_event_init(void);
void app_event_set(uint32_t bits);
void app_event_clear(uint32_t bits);
bool app_event_wait(uint32_t bits, TickType_t ticks_to_wait);
uint32_t app_event_get_bits(void);
bool app_event_is_set(uint32_t bits);

#endif
