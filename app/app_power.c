#include "app_power.h"

#include "main.h"
#include "app_backlight.h"
#include "app_led.h"
#include "app_music_player.h"
#include "app_ui_internal.h"
#include "usb_host.h"

/* PE7~PE11 对应 EXTI7~11，引脚位掩码与 EXTI 行掩码数值相同 */
#define APP_POWER_KEY_PINS   (KEY_Pin | KEYE8_Pin | KEYE9_Pin | KEYE10_Pin | KEYE11_Pin)
#define APP_POWER_EXTI_PRIO  6U   /* 数值越大优先级越低；仅作唤醒，不调用 FreeRTOS API */

extern void SystemClock_Config(void);

static void app_power_config_keys_exti(void);
static void app_power_restore_keys(void);

bool app_power_can_sleep(void)
{
    app_music_player_state_t state = app_music_player_get_state();

    /* 正在播放/加载音乐时不能进入 STOP，否则 I2S/DMA/读卡全部停摆，音乐中断 */
    if (state == APP_MUSIC_PLAYER_STATE_PLAYING ||
        state == APP_MUSIC_PLAYER_STATE_LOADING) {
        return false;
    }

    /* 有活动的 USB 鼠标时进入 STOP 会导致 OTG 时钟停摆、鼠标断连，退回仅关背光 */
    if (Appli_state == APPLICATION_READY) {
        return false;
    }

    return true;
}

void app_power_enter_stop(void)
{
    /* 1. 关背光 */
    app_backlight_set_percent(0U);

    /* 2. 按键配为 EXTI 下降沿唤醒源 */
    app_power_config_keys_exti();

    /* 3. 关闭 SysTick(FreeRTOS) 与 TIM11(HAL/LVGL) 节拍中断，
     *    否则它们 1ms 一次会立即唤醒 STOP，达不到低功耗。 */
    SysTick->CTRL &= ~SysTick_CTRL_TICKINT_Msk;
    HAL_SuspendTick();

    /* 4. 清 EXTI 挂起位与 PWR 唤醒标志，确保只有“新的按键”才唤醒 */
    EXTI->PR = APP_POWER_KEY_PINS;
    __HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU);

    /* 5. 进入 STOP（WFI）。按键 EXTI 中断可唤醒 CPU。 */
    HAL_PWR_EnterSTOPMode(PWR_LOWPOWERREGULATOR_ON, PWR_STOPENTRY_WFI);

    /* ---- 唤醒后 ---- */

    /* 6. 恢复 168MHz 主时钟（STOP 唤醒后系统时钟回落到 HSI 16MHz；
     *    HAL_RCC_ClockConfig 内部会调用 HAL_InitTick 重配 TIM11 时基） */
    SystemClock_Config();

    /* 7. 恢复按键为普通输入并关闭 EXTI 中断（先于重开节拍，避免误触发中断） */
    app_power_restore_keys();

    /* 8. 复位按键沿检测，避免“唤醒按下的那颗键”被再次识别为一次新按键 */
    key_scan_reset();

    /* 9. 恢复节拍中断 */
    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
    HAL_ResumeTick();

    /* 10. 恢复背光亮度 */
    ui_apply_brightness();
}

static void app_power_config_keys_exti(void)
{
    GPIO_InitTypeDef gpio = {0};

    __HAL_RCC_SYSCFG_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();

    gpio.Pin   = APP_POWER_KEY_PINS;
    gpio.Mode  = GPIO_MODE_IT_FALLING;
    gpio.Pull  = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOE, &gpio);

    HAL_NVIC_SetPriority(EXTI9_5_IRQn, APP_POWER_EXTI_PRIO, 0U);
    HAL_NVIC_EnableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_SetPriority(EXTI15_10_IRQn, APP_POWER_EXTI_PRIO, 0U);
    HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);
}

static void app_power_restore_keys(void)
{
    GPIO_InitTypeDef gpio = {0};

    HAL_NVIC_DisableIRQ(EXTI9_5_IRQn);
    HAL_NVIC_DisableIRQ(EXTI15_10_IRQn);

    /* HAL_GPIO_Init 切回 INPUT 不会自动清 EXTI 的 IMR/FTSR/RTSR/PR，需手动清 */
    EXTI->IMR  &= ~APP_POWER_KEY_PINS;
    EXTI->FTSR &= ~APP_POWER_KEY_PINS;
    EXTI->RTSR &= ~APP_POWER_KEY_PINS;
    EXTI->PR    =  APP_POWER_KEY_PINS;

    gpio.Pin  = APP_POWER_KEY_PINS;
    gpio.Mode = GPIO_MODE_INPUT;
    gpio.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOE, &gpio);
}

/* EXTI 中断：仅清挂起标志；真正的唤醒后续逻辑在 app_power_enter_stop 里完成。
 * 正常流程中 restore_keys 会在重开节拍前关掉 EXTI，这两个 ISR 通常不会触发。 */
void EXTI9_5_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEY_Pin | KEYE8_Pin | KEYE9_Pin);
}

void EXTI15_10_IRQHandler(void)
{
    HAL_GPIO_EXTI_IRQHandler(KEYE10_Pin | KEYE11_Pin);
}
