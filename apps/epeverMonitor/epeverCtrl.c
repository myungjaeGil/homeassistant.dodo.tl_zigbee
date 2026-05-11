/*********************************************************************
 * @file    epeverCtrl.c
 * @brief   ZT3L EPever MPPT 모니터 — LED/버튼 제어
 *********************************************************************/

#include "tl_common.h"
#include "zcl_include.h"
#include "epeverMonitor.h"
#include "epeverCtrl.h"

extern void sampleLight_onOffInit(void);

/*====================================================================
 * LED_POWER 상태 관리
 *==================================================================*/
static led_pwr_state_t  s_led_pwr_state  = LED_PWR_STATE_BOOT;
static ev_timer_event_t *s_led_pwr_tmr   = NULL;
static bool              s_led_pwr_level = FALSE;

static s32 led_power_blink_cb(void *arg)
{
    u16 interval_ms = (u16)(u32)arg;
    s_led_pwr_level = !s_led_pwr_level;
    drv_gpio_write(LED_POWER, s_led_pwr_level ? 0 : 1);
    return interval_ms;
}

static void led_power_blink_start(u16 interval_ms)
{
    if (s_led_pwr_tmr) {
        TL_ZB_TIMER_CANCEL(&s_led_pwr_tmr);
    }
    s_led_pwr_level = TRUE;
    drv_gpio_write(LED_POWER, 0);
    s_led_pwr_tmr = TL_ZB_TIMER_SCHEDULE(led_power_blink_cb,
                                          (void*)(u32)interval_ms,
                                          interval_ms);
}

static void led_power_blink_stop(bool on)
{
    if (s_led_pwr_tmr) {
        TL_ZB_TIMER_CANCEL(&s_led_pwr_tmr);
        s_led_pwr_tmr = NULL;
    }
    drv_gpio_write(LED_POWER, on ? 0 : 1);
}

void led_power_set_state(led_pwr_state_t state)
{
    if (s_led_pwr_state == state) return;
    s_led_pwr_state = state;

    switch (state) {
    case LED_PWR_STATE_BOOT:
        led_power_blink_stop(FALSE);
        break;
    case LED_PWR_STATE_JOINED:
        led_power_blink_stop(TRUE);
        break;
    case LED_PWR_STATE_NOT_JOINED:
        led_power_blink_start(500);
        break;
    case LED_PWR_STATE_RESET:
        led_power_blink_start(200);
        break;
    }
}

void led_power_init(void)
{
    drv_gpio_func_set(LED_POWER);
    drv_gpio_output_en(LED_POWER, 1);
    drv_gpio_input_en(LED_POWER, 0);
    drv_gpio_write(LED_POWER, 1);   /* active low: 초기 OFF */

    drv_gpio_func_set(LED_PERMIT);
    drv_gpio_output_en(LED_PERMIT, 1);
    drv_gpio_input_en(LED_PERMIT, 0);
    drv_gpio_write(LED_PERMIT, 1);  /* active low: 초기 OFF */
}

/*====================================================================
 * epever_hw_init
 *==================================================================*/
void epever_hw_init(void)
{
    gpio_set_func(GPIO_PD7, AS_GPIO);
    gpio_set_output_en(GPIO_PD7, 1);
    gpio_set_input_en(GPIO_PD7, 0);
    gpio_write(GPIO_PD7, 1);

    led_power_init();
    led_power_set_state(LED_PWR_STATE_NOT_JOINED);
}

/*====================================================================
 * SDK 호환 스텁
 *==================================================================*/
void sampleLight_onOffInit(void)  { }
void sampleLight_onOffUpdate(u8 cmd) { (void)cmd; }
void light_refresh(u8 sta)        { (void)sta; }

void light_applyUpdate(u8 *curLevel, u16 *curLevel256, s32 *stepLevel256,
                       u16 *remainingTime, u8 minLevel, u8 maxLevel, bool wrap)
{
    (void)curLevel; (void)curLevel256; (void)stepLevel256;
    (void)remainingTime; (void)minLevel; (void)maxLevel; (void)wrap;
}

void light_applyUpdate_16(u16 *curLevel, u32 *curLevel256, s32 *stepLevel256,
                          u16 *remainingTime, u16 minLevel, u16 maxLevel, bool wrap)
{
    (void)curLevel; (void)curLevel256; (void)stepLevel256;
    (void)remainingTime; (void)minLevel; (void)maxLevel; (void)wrap;
}

/*====================================================================
 * LED Blink
 *==================================================================*/
static s32 blink_TimerEvtCb(void *arg)
{
    u32 interval = 0;

    gEpeverCtx.sta = !gEpeverCtx.sta;
    if (gEpeverCtx.sta) {
        drv_gpio_write(LED_POWER, 0);
        interval = gEpeverCtx.ledOnTime;
    } else {
        drv_gpio_write(LED_POWER, 1);
        interval = gEpeverCtx.ledOffTime;
    }

    if (gEpeverCtx.sta == gEpeverCtx.oriSta) {
        if (gEpeverCtx.times) {
            gEpeverCtx.times--;
            if (gEpeverCtx.times <= 0) {
                drv_gpio_write(LED_POWER, gEpeverCtx.oriSta ? 0 : 1);
                gEpeverCtx.timerLedEvt = NULL;
                return -1;
            }
        }
    }
    return interval;
}

void light_blink_start(u8 times, u16 ledOnTime, u16 ledOffTime)
{
    if (!gEpeverCtx.timerLedEvt) {
        gEpeverCtx.times      = times;
        gEpeverCtx.ledOnTime  = ledOnTime;
        gEpeverCtx.ledOffTime = ledOffTime;
        gEpeverCtx.oriSta     = 0;
        gEpeverCtx.sta        = 0;

        gEpeverCtx.timerLedEvt =
            TL_ZB_TIMER_SCHEDULE(blink_TimerEvtCb, NULL, ledOnTime);
    }
}

void light_blink_stop(void)
{
    if (gEpeverCtx.timerLedEvt) {
        TL_ZB_TIMER_CANCEL(&gEpeverCtx.timerLedEvt);
        gEpeverCtx.times = 0;
        drv_gpio_write(LED_POWER, 1);
    }
}
