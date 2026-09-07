#include "bsp_buzzer.h"

#include "tim.h"

#define BSP_BUZZER_TIM                  htim5
#define BSP_BUZZER_CHANNEL              TIM_CHANNEL_4
#define BSP_BUZZER_BACKLIGHT_CHANNEL    TIM_CHANNEL_1
#define BSP_BUZZER_MIN_FREQ_HZ          40U
#define BSP_BUZZER_MAX_FREQ_HZ          20000U
#define BSP_BUZZER_MAX_DUTY_PERCENT     90U

static uint32_t g_buzzer_default_prescaler;
static uint32_t g_buzzer_default_period;
static bool g_buzzer_initialized;
static bool g_buzzer_running;

static uint32_t buzzer_get_tim5_clock_hz(void);
static void buzzer_set_timer(uint32_t prescaler, uint32_t period);
static uint32_t buzzer_capture_channel_ratio(uint32_t channel, uint32_t ticks);
static void buzzer_restore_channel_ratio(uint32_t channel, uint32_t ticks, uint32_t ratio);

bsp_buzzer_status_t BSP_Buzzer_Init(void)
{
    if (!g_buzzer_initialized) {
        g_buzzer_default_prescaler = BSP_BUZZER_TIM.Init.Prescaler;
        g_buzzer_default_period = BSP_BUZZER_TIM.Init.Period;
        g_buzzer_initialized = true;
    }

    BSP_Buzzer_Stop();
    return BSP_BUZZER_OK;
}

bsp_buzzer_status_t BSP_Buzzer_PlayTone(uint32_t frequency_hz, uint8_t duty_percent)
{
    uint32_t timer_hz;
    uint32_t period;
    uint32_t ticks;
    uint32_t pulse;
    uint32_t old_ticks;
    uint32_t backlight_ratio;

    if (frequency_hz < BSP_BUZZER_MIN_FREQ_HZ || frequency_hz > BSP_BUZZER_MAX_FREQ_HZ) {
        return BSP_BUZZER_INVALID_PARAM;
    }

    if (duty_percent > BSP_BUZZER_MAX_DUTY_PERCENT) {
        duty_percent = BSP_BUZZER_MAX_DUTY_PERCENT;
    }

    if (!g_buzzer_initialized) {
        (void)BSP_Buzzer_Init();
    }

    timer_hz = buzzer_get_tim5_clock_hz();
    if (timer_hz == 0U) {
        return BSP_BUZZER_ERROR;
    }

    ticks = timer_hz / frequency_hz;
    if (ticks < 2U) {
        ticks = 2U;
    }
    period = ticks - 1U;

    old_ticks = __HAL_TIM_GET_AUTORELOAD(&BSP_BUZZER_TIM) + 1U;
    backlight_ratio = buzzer_capture_channel_ratio(BSP_BUZZER_BACKLIGHT_CHANNEL, old_ticks);
    buzzer_set_timer(0U, period);
    buzzer_restore_channel_ratio(BSP_BUZZER_BACKLIGHT_CHANNEL, ticks, backlight_ratio);

    pulse = (ticks * duty_percent) / 100U;
    if (pulse >= ticks) {
        pulse = ticks - 1U;
    }
    __HAL_TIM_SET_COMPARE(&BSP_BUZZER_TIM, BSP_BUZZER_CHANNEL, pulse);

    if (!g_buzzer_running) {
        if (HAL_TIM_PWM_Start(&BSP_BUZZER_TIM, BSP_BUZZER_CHANNEL) != HAL_OK) {
            __HAL_TIM_SET_COMPARE(&BSP_BUZZER_TIM, BSP_BUZZER_CHANNEL, 0U);
            return BSP_BUZZER_ERROR;
        }
        g_buzzer_running = true;
    }

    return BSP_BUZZER_OK;
}

void BSP_Buzzer_Mute(void)
{
    __HAL_TIM_SET_COMPARE(&BSP_BUZZER_TIM, BSP_BUZZER_CHANNEL, 0U);
}

void BSP_Buzzer_Stop(void)
{
    uint32_t old_ticks;
    uint32_t default_ticks;
    uint32_t backlight_ratio;

    if (g_buzzer_running) {
        (void)HAL_TIM_PWM_Stop(&BSP_BUZZER_TIM, BSP_BUZZER_CHANNEL);
        g_buzzer_running = false;
    }

    __HAL_TIM_SET_COMPARE(&BSP_BUZZER_TIM, BSP_BUZZER_CHANNEL, 0U);

    if (g_buzzer_initialized) {
        old_ticks = __HAL_TIM_GET_AUTORELOAD(&BSP_BUZZER_TIM) + 1U;
        default_ticks = g_buzzer_default_period + 1U;
        backlight_ratio = buzzer_capture_channel_ratio(BSP_BUZZER_BACKLIGHT_CHANNEL, old_ticks);
        buzzer_set_timer(g_buzzer_default_prescaler, g_buzzer_default_period);
        buzzer_restore_channel_ratio(BSP_BUZZER_BACKLIGHT_CHANNEL, default_ticks, backlight_ratio);
    }
}

bool BSP_Buzzer_IsRunning(void)
{
    return g_buzzer_running;
}

static uint32_t buzzer_get_tim5_clock_hz(void)
{
    uint32_t pclk1 = HAL_RCC_GetPCLK1Freq();

    if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1) {
        pclk1 *= 2U;
    }

    return pclk1;
}

static void buzzer_set_timer(uint32_t prescaler, uint32_t period)
{
    __HAL_TIM_SET_PRESCALER(&BSP_BUZZER_TIM, prescaler);
    __HAL_TIM_SET_AUTORELOAD(&BSP_BUZZER_TIM, period);
    BSP_BUZZER_TIM.Init.Prescaler = prescaler;
    BSP_BUZZER_TIM.Init.Period = period;
    __HAL_TIM_SET_COUNTER(&BSP_BUZZER_TIM, 0U);
    BSP_BUZZER_TIM.Instance->EGR = TIM_EGR_UG;
}

static uint32_t buzzer_capture_channel_ratio(uint32_t channel, uint32_t ticks)
{
    uint32_t compare;

    if (ticks == 0U) {
        return 0U;
    }

    compare = __HAL_TIM_GET_COMPARE(&BSP_BUZZER_TIM, channel);
    if (compare > ticks) {
        compare = ticks;
    }

    return (compare * 10000U) / ticks;
}

static void buzzer_restore_channel_ratio(uint32_t channel, uint32_t ticks, uint32_t ratio)
{
    uint32_t compare = (ticks * ratio) / 10000U;

    if (compare > ticks) {
        compare = ticks;
    }

    __HAL_TIM_SET_COMPARE(&BSP_BUZZER_TIM, channel, compare);
}
