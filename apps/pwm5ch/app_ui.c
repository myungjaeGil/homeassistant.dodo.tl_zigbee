/*********************************************************************
 * @file    app_ui.c
 * @brief   ZT3L Button1 (GPIO_PD4) 처리 — PWM 5CH 버전
 *
 *  - 길게 누름 (>= 5초) : Factory Reset
 *********************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "pwm5ch_monitor.h"
#include "app_ui.h"
#include "factory_reset.h"
#include "pwm5ch_ctrl.h"

#define BUTTON_LONG_PRESS_MS    5000

static u32  s_btn_press_start  = 0;
static bool s_btn_pressed      = FALSE;
static bool s_btn_long_handled = FALSE;

static bool button1_is_pressed(void)
{
    return (drv_gpio_read(BUTTON1) == 0);
}

void app_key_handler(void)
{
    bool pressed = button1_is_pressed();

    if (pressed && !s_btn_pressed) {
        s_btn_pressed      = TRUE;
        s_btn_long_handled = FALSE;
        s_btn_press_start  = clock_time();

    } else if (pressed && s_btn_pressed) {
        u32 held_ms = (u32)((clock_time() - s_btn_press_start)
                            / CLOCK_16M_SYS_TIMER_CLK_1MS);

        if (!s_btn_long_handled && held_ms >= BUTTON_LONG_PRESS_MS) {
            s_btn_long_handled = TRUE;
            printf("[UI] Long press -> Factory Reset\r\n");
            led_power_set_state(LED_PWR_STATE_RESET);
            zb_factoryReset();
        }

    } else if (!pressed && s_btn_pressed) {
        s_btn_pressed      = FALSE;
        s_btn_long_handled = FALSE;
    }
}

void localPermitJoinState(void)
{
    static bool s_permit = FALSE;
    bool permit = zb_getMacAssocPermit();

    if (permit != s_permit) {
        s_permit = permit;
        if (permit) {
            /* Permit Join 시작 → 100ms 빠른 블링크 */
            led_power_set_state(LED_PWR_STATE_PERMIT);
        } else {
            /* Permit Join 종료 → Joined 상태로 복귀 */
            led_power_set_state(LED_PWR_STATE_BOOT);  /* force re-apply */
            if (zb_isDeviceJoinedNwk()) {
                led_power_set_state(LED_PWR_STATE_JOINED);
            } else {
                led_power_set_state(LED_PWR_STATE_NOT_JOINED);
            }
        }
    }
}
