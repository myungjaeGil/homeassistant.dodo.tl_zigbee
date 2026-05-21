/*********************************************************************
 * @file    pwm5ch_ctrl.c
 * @brief   ZT3L PWM 5채널 컨트롤러 — PWM HW 제어 + LED 상태머신
 *
 * LED_POWER(PC0) — active HIGH (1 = ON, 0 = OFF)
 *   Not Joined  : 500ms 블링크
 *   Permit Join : 100ms 블링크
 *   Joined      : 상시 점등
 *   Reset       : 200ms 블링크
 *********************************************************************/

#include "tl_common.h"
#include "zcl_include.h"
#include "pwm5ch_ep.h"
#include "pwm5ch_ctrl.h"

/*====================================================================
 * PWM 채널 매핑 테이블
 *==================================================================*/
typedef struct {
    u8  pwm_ch;
    u32 pin;
} pwm_hw_map_t;

static const pwm_hw_map_t s_pwm_map[5] = {
    { PWM_CH0_CH, PWM_CH0_PIN },
    { PWM_CH1_CH, PWM_CH1_PIN },
    { PWM_CH2_CH, PWM_CH2_PIN },
    { PWM_CH3_CH, PWM_CH3_PIN },
    { PWM_CH4_CH, PWM_CH4_PIN },
};

/*====================================================================
 * PWM 하드웨어 출력 적용
 *==================================================================*/
void pwm_hw_apply(u8 ep_idx)
{
    if (ep_idx < 5) {
        bool  on   = g_pwmChAttrs[ep_idx].onOff;
        u8    lv   = g_pwmChAttrs[ep_idx].currentLevel;
        u16   tick = on ? LEVEL_TO_TICK(lv) : 0;
        u8    ch   = s_pwm_map[ep_idx].pwm_ch;
        pwm_set_cycle_and_duty(ch, (u16)PWM_MAX_TICK, tick);
        printf("[PWM] CH%d -> onOff=%d lv=%d tick=%d\r\n",
               ep_idx, (int)on, (int)lv, (int)tick);
    } else {
        bool mst_on = g_pwmChAttrs[5].onOff;
        u8   mst_lv = g_pwmChAttrs[5].currentLevel;
        u8   i;
        printf("[PWM] MASTER -> onOff=%d lv=%d\r\n", (int)mst_on, (int)mst_lv);
        for (i = 0; i < 5; i++) {
            u16 tick;
            if (!mst_on || !g_pwmChAttrs[i].onOff) {
                tick = 0;
            } else {
                u32 combined = (u32)g_pwmChAttrs[i].currentLevel * mst_lv / 254;
                tick = LEVEL_TO_TICK((u8)combined);
            }
            pwm_set_cycle_and_duty(s_pwm_map[i].pwm_ch, (u16)PWM_MAX_TICK, tick);
        }
    }
}

/*====================================================================
 * PWM 채널 초기화
 *==================================================================*/
static void pwm_channel_init(void)
{
    u8 i;
    PWM_ALL_CHANNEL_SET();
    pwm_set_clk(PWM_CLK_DIV, 0);
    for (i = 0; i < 5; i++) {
        u8 ch = s_pwm_map[i].pwm_ch;
        pwm_set_mode(ch, PWM_NORMAL_MODE);
        pwm_set_cycle_and_duty(ch, (u16)PWM_MAX_TICK, 0);
        pwm_start(ch);
    }
    printf("[PWM] HW init: 5ch started (~%dkHz)\r\n",
           (int)(CLOCK_SYS_CLOCK_HZ / PWM_MAX_TICK / 1000));
}

/*====================================================================
 * LED_POWER 상태 관리 — 단일 타이머, active HIGH
 *   drv_gpio_write(LED_POWER, 1) = ON
 *   drv_gpio_write(LED_POWER, 0) = OFF
 *==================================================================*/
static led_pwr_state_t   s_led_pwr_state = LED_PWR_STATE_BOOT;
static ev_timer_event_t *s_led_tmr       = NULL;
static bool              s_led_level     = FALSE;

static s32 led_blink_cb(void *arg)
{
    u16 ms = (u16)(u32)arg;
    s_led_level = !s_led_level;
    drv_gpio_write(LED_POWER, s_led_level ? 1 : 0);  /* active high */
    return ms;
}

static void led_blink_start(u16 ms)
{
    if (s_led_tmr) {
        TL_ZB_TIMER_CANCEL(&s_led_tmr);
        s_led_tmr = NULL;
    }
    s_led_level = TRUE;
    drv_gpio_write(LED_POWER, 1);  /* ON */
    s_led_tmr = TL_ZB_TIMER_SCHEDULE(led_blink_cb, (void*)(u32)ms, ms);
}

static void led_blink_stop(bool on)
{
    if (s_led_tmr) {
        TL_ZB_TIMER_CANCEL(&s_led_tmr);
        s_led_tmr = NULL;
    }
    drv_gpio_write(LED_POWER, on ? 1 : 0);  /* active high */
}

void led_power_set_state(led_pwr_state_t state)
{
    /* JOINED는 타이머 잔존 방지를 위해 항상 재적용 */
    if (s_led_pwr_state == state && state != LED_PWR_STATE_JOINED) return;
    s_led_pwr_state = state;

    switch (state) {
    case LED_PWR_STATE_BOOT:
        led_blink_stop(FALSE);          /* OFF */
        break;
    case LED_PWR_STATE_JOINED:
        led_blink_stop(TRUE);           /* 타이머 취소 + 상시 점등 */
        break;
    case LED_PWR_STATE_NOT_JOINED:
        led_blink_start(500);           /* 500ms 블링크 */
        break;
    case LED_PWR_STATE_RESET:
        led_blink_start(200);           /* 200ms 블링크 */
        break;
    case LED_PWR_STATE_PERMIT:
        led_blink_start(100);           /* 100ms 블링크 */
        break;
    }
}

void led_power_init(void)
{
    drv_gpio_func_set(LED_POWER);
    drv_gpio_output_en(LED_POWER, 1);
    drv_gpio_input_en(LED_POWER, 0);
    drv_gpio_write(LED_POWER, 0);       /* 초기 OFF */

    /* LED_PERMIT(PA0) — 미사용, OFF 고정 */
    drv_gpio_func_set(LED_PERMIT);
    drv_gpio_output_en(LED_PERMIT, 1);
    drv_gpio_input_en(LED_PERMIT, 0);
    drv_gpio_write(LED_PERMIT, 0);
}

/*====================================================================
 * pwm_hw_init
 *==================================================================*/
void pwm_hw_init(void)
{
    gpio_set_func(GPIO_PD7, AS_GPIO);
    gpio_set_output_en(GPIO_PD7, 1);
    gpio_set_input_en(GPIO_PD7, 0);
    gpio_write(GPIO_PD7, 1);

    led_power_init();
    led_power_set_state(LED_PWR_STATE_NOT_JOINED);

    pwm_channel_init();
}

/*====================================================================
 * Identify 블링크 — 동일 s_led_tmr 재사용
 *==================================================================*/
void light_blink_start(u8 times, u16 ledOnTime, u16 ledOffTime)
{
    (void)times;
    (void)ledOffTime;
    led_blink_start(ledOnTime);
}

void light_blink_stop(void)
{
    /* Identify 종료 → 현재 state 재적용 */
    led_pwr_state_t cur = s_led_pwr_state;
    s_led_pwr_state = LED_PWR_STATE_BOOT;
    led_power_set_state(cur);
}

/*====================================================================
 * SDK 호환 스텁
 *==================================================================*/
void sampleLight_onOffInit(void)     { }
void sampleLight_onOffUpdate(u8 cmd) { (void)cmd; }
void light_refresh(u8 sta)           { (void)sta; }

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
