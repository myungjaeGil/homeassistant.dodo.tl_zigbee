/*********************************************************************
 * @file    app_ui.c
 * @brief   ZT3L 무시동히터 리모컨 — BUTTON1 (GPIO_PD4) 처리
 *
 *  - 짧게 누름 (50ms ~ 1초)  : 히터 전원 토글 (HEATER_BUTTON_TOGGLE_ENABLE)
 *  - 길게 누름 (>= 5초)      : Factory Reset
 *  - 1~5초 사이에서 떼면 아무 동작 없음 (오조작 방지)
 *********************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "heater_main.h"
#include "app_ui.h"
#include "factory_reset.h"

#define BUTTON_DEBOUNCE_MS      50
#define BUTTON_SHORT_MAX_MS     1000
#define BUTTON_LONG_PRESS_MS    5000

static u32  s_btn_press_start  = 0;
static bool s_btn_pressed      = FALSE;
static bool s_btn_long_handled = FALSE;

static bool button1_is_pressed(void)
{
    return (drv_gpio_read(BUTTON1) == 0);
}

static u32 held_ms(void)
{
    return (u32)((clock_time() - s_btn_press_start) / CLOCK_16M_SYS_TIMER_CLK_1MS);
}

void app_key_handler(void)
{
    bool pressed = button1_is_pressed();

    if (pressed && !s_btn_pressed) {
        s_btn_pressed      = TRUE;
        s_btn_long_handled = FALSE;
        s_btn_press_start  = clock_time();

    } else if (pressed && s_btn_pressed) {
        if (!s_btn_long_handled && held_ms() >= BUTTON_LONG_PRESS_MS) {
            s_btn_long_handled = TRUE;
            DBG_LOG("[UI] Long press -> Factory Reset\r\n");
            led_power_set_state(LED_PWR_STATE_RESET);
            zb_factoryReset();
        }

    } else if (!pressed && s_btn_pressed) {
        u32 ms = held_ms();
        s_btn_pressed = FALSE;

#if HEATER_BUTTON_TOGGLE_ENABLE
        if (!s_btn_long_handled && ms >= BUTTON_DEBOUNCE_MS && ms < BUTTON_SHORT_MAX_MS) {
            DBG_LOG("[UI] Short press -> heater toggle\r\n");
            heater_power_toggle();
        }
#else
        (void)ms;
#endif
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
            led_power_set_state(LED_PWR_STATE_PERMIT);
        } else {
            led_power_set_state(LED_PWR_STATE_BOOT);  /* force re-apply */
            led_power_set_state(zb_isDeviceJoinedNwk() ? LED_PWR_STATE_JOINED
                                                       : LED_PWR_STATE_NOT_JOINED);
        }
    }
}
