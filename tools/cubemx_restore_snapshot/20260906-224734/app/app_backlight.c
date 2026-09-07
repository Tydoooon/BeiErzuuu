#include "app_backlight.h"

#include "tim.h"

#include <stdbool.h>

#define APP_BACKLIGHT_PWM_CHANNEL       TIM_CHANNEL_1
#define APP_BACKLIGHT_ACTIVE_LOW        0U
#define APP_BACKLIGHT_MIN_PERCENT       0U
#define APP_BACKLIGHT_MAX_PERCENT       100U

static bool g_backlight_pwm_started;

static uint8_t app_backlight_clamp_percent(uint8_t percent);
static uint32_t app_backlight_percent_to_pulse(uint8_t percent);

void app_backlight_init(uint8_t percent)
{
    app_backlight_set_percent(percent);
}

void app_backlight_set_percent(uint8_t percent)
{
    uint32_t pulse;

    percent = app_backlight_clamp_percent(percent);
    pulse = app_backlight_percent_to_pulse(percent);
    __HAL_TIM_SET_COMPARE(&htim5, APP_BACKLIGHT_PWM_CHANNEL, pulse);

    if (!g_backlight_pwm_started) {
        if (HAL_TIM_PWM_Start(&htim5, APP_BACKLIGHT_PWM_CHANNEL) == HAL_OK) {
            g_backlight_pwm_started = true;
        }
    }
}

static uint8_t app_backlight_clamp_percent(uint8_t percent)
{
    if (percent > APP_BACKLIGHT_MAX_PERCENT) {
        return APP_BACKLIGHT_MAX_PERCENT;
    }

    return percent;
}

static uint32_t app_backlight_percent_to_pulse(uint8_t percent)
{
    uint32_t period = __HAL_TIM_GET_AUTORELOAD(&htim5);
    uint32_t ticks = period + 1U;
    uint32_t pulse = (ticks * percent) / APP_BACKLIGHT_MAX_PERCENT;

#if APP_BACKLIGHT_ACTIVE_LOW
    pulse = ticks - pulse;
#endif

    if (pulse > ticks) {
        pulse = ticks;
    }

    return pulse;
}
