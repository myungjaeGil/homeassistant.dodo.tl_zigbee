/*********************************************************************
 * @file    app_ui.c
 *
 * @brief   ZT3L Button1 (GPIO_PD4) 처리 — JUNTEK BMS 버전
 *
 *  버튼 동작:
 *   - 짧게 누름 (200ms~5s) : 없음 (향후 확장)
 *   - 길게 누름 (>= 5초)   : Factory Reset
 *********************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "juntekMonitor.h"
#include "app_ui.h"
#include "factory_reset.h"
#include "juntekCtrl.h"

/*--------------------------------------------------------------------
 * 상수
 *------------------------------------------------------------------*/
#define BUTTON_SHORT_PRESS_MS   200
#define BUTTON_LONG_PRESS_MS    5000

/*--------------------------------------------------------------------
 * 상태
 *------------------------------------------------------------------*/
static u32  s_btn_press_start  = 0;
static bool s_btn_pressed      = FALSE;
static bool s_btn_long_handled = FALSE;

/*--------------------------------------------------------------------
 * 로컬 함수
 *------------------------------------------------------------------*/
static bool button1_is_pressed(void)
{
    return (drv_gpio_read(BUTTON1) == 0);   /* Active Low */
}

/*--------------------------------------------------------------------
 * app_key_handler
 *------------------------------------------------------------------*/
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
            printf("Button1 long press: factory reset\r\n");
            led_power_set_state(LED_PWR_STATE_RESET);
            zb_factoryReset();
        }

    } else if (!pressed && s_btn_pressed) {
        /* 짧게 뗌 — 향후 기능 추가 가능 */
        s_btn_pressed      = FALSE;
        s_btn_long_handled = FALSE;
    }
}

/*--------------------------------------------------------------------
 * localPermitJoinState
 *------------------------------------------------------------------*/
void localPermitJoinState(void)
{
    static bool s_permit = FALSE;
    bool permit = zb_getMacAssocPermit();

    if (permit != s_permit) {
        s_permit = permit;
        drv_gpio_write(LED_PERMIT, permit ? 1 : 0);
    }
}
