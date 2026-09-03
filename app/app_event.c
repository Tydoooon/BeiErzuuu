#include "app_event.h"

EventGroupHandle_t g_app_events;

void app_event_init(void)
{
    if (g_app_events == NULL) {
        g_app_events = xEventGroupCreate();
    }
}

void app_event_set(uint32_t bits)
{
    if (g_app_events == NULL) {
        app_event_init();
    }
    if (g_app_events != NULL) {
        (void)xEventGroupSetBits(g_app_events, bits);
    }
}

void app_event_clear(uint32_t bits)
{
    if (g_app_events == NULL) {
        app_event_init();
    }
    if (g_app_events != NULL) {
        (void)xEventGroupClearBits(g_app_events, bits);
    }
}

bool app_event_wait(uint32_t bits, TickType_t ticks_to_wait)
{
    EventBits_t got_bits;

    if (g_app_events == NULL) {
        app_event_init();
    }
    if (g_app_events == NULL) {
        return false;
    }

    got_bits = xEventGroupWaitBits(g_app_events,
                                   (EventBits_t)bits,
                                   pdFALSE,
                                   pdTRUE,
                                   ticks_to_wait);

    return (got_bits & (EventBits_t)bits) == (EventBits_t)bits;
}

uint32_t app_event_get_bits(void)
{
    if (g_app_events == NULL) {
        app_event_init();
    }
    if (g_app_events == NULL) {
        return 0U;
    }

    return (uint32_t)xEventGroupGetBits(g_app_events);
}

bool app_event_is_set(uint32_t bits)
{
    uint32_t got_bits = app_event_get_bits();

    return (got_bits & bits) == bits;
}
