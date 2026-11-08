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
 * level(0~254) → tick 변환
 *   PWM_GAMMA_ENABLE=1 : 감마 2.2 테이블 (0~65535 정규화)
 *   PWM_GAMMA_ENABLE=0 : 선형
 *   채널별 디밍 노출 구간 재매핑 → dimMinPct ~ dimMaxPct (속성 0xF000/0xF001)
 *   레벨 1~253 → [PWM_TICK_LO, PWM_TICK_HI], 254 → 100%
 *==================================================================*/
#if PWM_GAMMA_ENABLE
static const u16 s_gamma[255] = {
    0, 1, 2, 4, 7, 12, 17, 24, 33, 42,
    53, 66, 79, 95, 112, 130, 150, 171, 194, 218,
    244, 272, 301, 332, 365, 399, 435, 473, 512, 553,
    596, 641, 687, 735, 785, 837, 891, 946, 1003, 1062,
    1123, 1186, 1250, 1317, 1385, 1455, 1527, 1601, 1677, 1755,
    1835, 1916, 2000, 2086, 2173, 2263, 2354, 2448, 2543, 2641,
    2740, 2842, 2945, 3051, 3158, 3268, 3379, 3493, 3609, 3727,
    3846, 3968, 4092, 4218, 4347, 4477, 4609, 4744, 4880, 5019,
    5160, 5303, 5448, 5595, 5745, 5896, 6050, 6206, 6364, 6524,
    6686, 6851, 7017, 7186, 7357, 7531, 7706, 7884, 8064, 8246,
    8430, 8617, 8806, 8997, 9190, 9385, 9583, 9783, 9986, 10190,
    10397, 10606, 10817, 11031, 11247, 11465, 11685, 11908, 12133, 12361,
    12590, 12822, 13057, 13293, 13532, 13773, 14017, 14263, 14511, 14762,
    15015, 15270, 15528, 15787, 16050, 16314, 16582, 16851, 17123, 17397,
    17673, 17952, 18234, 18517, 18803, 19092, 19383, 19676, 19972, 20270,
    20570, 20873, 21178, 21486, 21796, 22109, 22424, 22741, 23061, 23384,
    23708, 24036, 24365, 24697, 25032, 25369, 25709, 26050, 26395, 26742,
    27091, 27443, 27797, 28154, 28513, 28875, 29239, 29606, 29975, 30347,
    30721, 31098, 31477, 31859, 32243, 32630, 33019, 33411, 33805, 34202,
    34602, 35004, 35408, 35815, 36225, 36637, 37051, 37468, 37888, 38310,
    38735, 39163, 39592, 40025, 40460, 40898, 41338, 41781, 42226, 42674,
    43124, 43577, 44033, 44491, 44952, 45415, 45881, 46350, 46821, 47295,
    47772, 48251, 48732, 49216, 49703, 50193, 50685, 51179, 51677, 52177,
    52679, 53185, 53692, 54203, 54716, 55232, 55750, 56271, 56795, 57321,
    57850, 58382, 58916, 59453, 59992, 60535, 61080, 61627, 62177, 62730,
    63286, 63844, 64405, 64969, 65535,
};
#endif

u16 pwm_level_to_tick(u8 ch, u8 lv)
{
    u32 g, span, t;

    if (lv == 0)    return 0;
    if (lv > 254)   lv = 254;

/* 채널별 디밍 노출 구간 재매핑: 1~254 → dimMin%~dimMax% (Zigbee 속성으로 변경 가능) */
    if (ch < PWM_PHY_CH_COUNT) {
        u32 mn = g_pwmDimMinPct[ch];
        u32 mx = g_pwmDimMaxPct[ch];
        if (mx > 100) mx = 100;                 /* 저장 전 순간값 방어 */
        if (mn >= mx) mn = (mx > 0) ? mx - 1 : 0;
        if (mn > 0 || mx < 100) {
            u32 lv_lo = (254u * mn + 50) / 100;
            u32 lv_hi = (254u * mx + 50) / 100;
            if (lv_lo < 1)     lv_lo = 1;
            if (lv_hi < lv_lo) lv_hi = lv_lo;
            lv = (u8)(lv_lo + ((u32)(lv - 1) * (lv_hi - lv_lo) + 126) / 253);
        }
    }

    if (lv >= 254)  return (u16)PWM_MAX_TICK;      /* 100% = 완전 ON */

#if PWM_GAMMA_ENABLE
    g = s_gamma[lv];                                 /* 0~65535 */
#else
    g = ((u32)lv * 65535 + 127) / 254;
#endif

    /* [ON_MIN, 주기-OFF_MIN] 구간으로 매핑 — 출력단 지연/꼬리 보상 */
    span = (u32)(PWM_TICK_HI - PWM_TICK_LO);
    t    = (u32)PWM_TICK_LO + (g * span + 32767) / 65535;

    if (t < 1)           t = 1;
    if (t > PWM_TICK_HI) t = PWM_TICK_HI;
    return (u16)t;
}

/*====================================================================
 * PWM 페이드 — 채널별 현재 tick → 목표 tick 점진적 이동
 *   FADE_FULL_MS   : 0% → 100% 전체 구간 페이드 시간
 *   FADE_STEP_MS   : 스텝 간 간격(ms)
 *   FADE_STEP_TICK : PWM_MAX_TICK 에 비례 (주파수를 바꿔도 페이드 시간 동일)
 *==================================================================*/
#define FADE_FULL_MS        1000
#define FADE_STEP_MS        10
#define FADE_STEP_TICK      ((PWM_MAX_TICK * FADE_STEP_MS / FADE_FULL_MS) ? \
                             (PWM_MAX_TICK * FADE_STEP_MS / FADE_FULL_MS) : 1)

static u16               s_cur_tick[5]    = {0};   /* 채널별 현재 출력 tick */
static u16               s_target_tick[5] = {0};   /* 채널별 목표 tick */
static ev_timer_event_t *s_fade_tmr       = NULL;

static s32 pwm_fade_cb(void *arg)
{
    (void)arg;
    bool any_active = FALSE;
    u8   i;

    for (i = 0; i < 5; i++) {
        if (s_cur_tick[i] == s_target_tick[i]) continue;

        if (s_cur_tick[i] < s_target_tick[i]) {
            u16 diff = s_target_tick[i] - s_cur_tick[i];
            s_cur_tick[i] += (diff < FADE_STEP_TICK) ? diff : (u16)FADE_STEP_TICK;
        } else {
            u16 diff = s_cur_tick[i] - s_target_tick[i];
            s_cur_tick[i] -= (diff < FADE_STEP_TICK) ? diff : (u16)FADE_STEP_TICK;
        }

        pwm_set_cmp(s_pwm_map[i].pwm_ch, s_cur_tick[i]);   /* duty 만 갱신 (주기 재설정 X) */
        any_active = TRUE;
    }

    if (!any_active) {
        s_fade_tmr = NULL;
        return -1;   /* 타이머 종료 */
    }
    return FADE_STEP_MS;
}

static void pwm_fade_kick(void)
{
    if (!s_fade_tmr) {
        s_fade_tmr = TL_ZB_TIMER_SCHEDULE(pwm_fade_cb, NULL, FADE_STEP_MS);
    }
}

/*====================================================================
 * 채널 i 의 최종 출력 tick
 *   마스터(EP6) OFF 또는 채널 OFF → 0
 *   그 외 → 채널 레벨 × 마스터 레벨 / 254
 *==================================================================*/
static u16 pwm_calc_tick(u8 i)
{
    u32 combined;

    if (!g_pwmChAttrs[5].onOff || !g_pwmChAttrs[i].onOff) return 0;

    combined = (u32)g_pwmChAttrs[i].currentLevel * g_pwmChAttrs[5].currentLevel / 254;
    if (combined == 0) combined = 1;   /* 둘 다 ON 이면 최소 레벨 유지 */
    return LEVEL_TO_TICK(i, (u8)combined);
}

/*====================================================================
 * PWM 하드웨어 출력 적용 — 목표 tick 설정 후 페이드 타이머가 점진 이동
 *   ep_idx 0~4 : 해당 채널만 재계산 (마스터 반영)
 *   ep_idx 5   : 전 채널 재계산
 *==================================================================*/
void pwm_hw_apply(u8 ep_idx)
{
    u8 i;

    if (ep_idx < 5) {
        s_target_tick[ep_idx] = pwm_calc_tick(ep_idx);
    } else {
        for (i = 0; i < 5; i++) {
            s_target_tick[i] = pwm_calc_tick(i);
        }
    }

#if PWM_CAL_LOG
    for (i = 0; i < 5; i++) {
        u32 t = s_target_tick[i];
        DBG_LOG("[CAL] CH%d tick=%d/%d on=%d.%02dus\r\n", (int)(i + 1),
                (int)t, (int)PWM_MAX_TICK,
                (int)(t / PWM_TICK_PER_US),
                (int)((t % PWM_TICK_PER_US) * 100 / PWM_TICK_PER_US));
    }
#endif
    pwm_fade_kick();
}

/*====================================================================
 * PWM 채널 초기화
 *==================================================================*/
static void pwm_channel_init(void)
{
    u8 i;
    PWM_ALL_CHANNEL_SET();

    /* [수정] 기존 pwm_set_clk(0, 0) → 0÷0 로 분주 레지스터가 비정상값이 됨.
     *        B85 드라이버: reg_pwm_clk = system_clock_hz / pwm_clk - 1
     *        같은 값을 넣어 분주 1 (PWM 클럭 = 48MHz) */
    pwm_set_clk(CLOCK_SYS_CLOCK_HZ, CLOCK_SYS_CLOCK_HZ);

    for (i = 0; i < 5; i++) {
        u8 ch = s_pwm_map[i].pwm_ch;
        pwm_set_mode(ch, PWM_NORMAL_MODE);
        pwm_set_cycle_and_duty(ch, (u16)PWM_MAX_TICK, 0);
        pwm_start(ch);
    }
    DBG_LOG("[PWM] HW init: 5ch %dHz, hw=%d, on_min=%dus off_min=%dus, gamma=%d\r\n",
            (int)PWM_FREQ_HZ, (int)PWM_HW_TYPE,
            (int)PWM_ON_MIN_US, (int)PWM_OFF_MIN_US, (int)PWM_GAMMA_ENABLE);
}

/*====================================================================
 * LED_POWER 상태 관리 — 단일 타이머, active HIGH
 *==================================================================*/
static led_pwr_state_t   s_led_pwr_state = LED_PWR_STATE_BOOT;
static ev_timer_event_t *s_led_tmr       = NULL;
static bool              s_led_level     = FALSE;

/* 블링크 파라미터 — 상태 블링크(무한) / Identify 블링크(횟수 제한) 공용 */
static u16 s_blink_on_ms   = 500;
static u16 s_blink_off_ms  = 500;
static u16 s_blink_toggles = 0;     /* 0 = 무한, N = 남은 토글 수 */

static void led_timer_cancel(void)
{
    if (s_led_tmr) {
        TL_ZB_TIMER_CANCEL(&s_led_tmr);
        s_led_tmr = NULL;
    }
}

static void led_state_reapply(void);

static s32 led_blink_cb(void *arg)
{
    (void)arg;
    s_led_level = !s_led_level;
    drv_gpio_write(LED_POWER, s_led_level ? 1 : 0);  /* active high */

    if (s_blink_toggles) {
        if (--s_blink_toggles == 0) {
            /* 횟수 블링크 종료 → 원래 상태 표시로 복귀 */
            s_led_tmr = NULL;
            led_state_reapply();
            return -1;
        }
    }
    return s_led_level ? s_blink_on_ms : s_blink_off_ms;
}

static void led_blink_run(u16 on_ms, u16 off_ms, u16 toggles)
{
    led_timer_cancel();
    s_blink_on_ms   = on_ms  ? on_ms  : 1;
    s_blink_off_ms  = off_ms ? off_ms : 1;
    s_blink_toggles = toggles;
    s_led_level     = TRUE;
    drv_gpio_write(LED_POWER, 1);  /* ON */
    s_led_tmr = TL_ZB_TIMER_SCHEDULE(led_blink_cb, NULL, s_blink_on_ms);
}

static void led_blink_stop(bool on)
{
    led_timer_cancel();
    drv_gpio_write(LED_POWER, on ? 1 : 0);  /* active high */
}

static void led_state_apply(led_pwr_state_t state)
{
    switch (state) {
    case LED_PWR_STATE_BOOT:       led_blink_stop(FALSE);        break;
    case LED_PWR_STATE_JOINED:     led_blink_stop(TRUE);         break;
    case LED_PWR_STATE_NOT_JOINED: led_blink_run(500, 500, 0);   break;
    case LED_PWR_STATE_RESET:      led_blink_run(200, 200, 0);   break;
    case LED_PWR_STATE_PERMIT:     led_blink_run(100, 100, 0);   break;
    }
}

static void led_state_reapply(void)
{
    led_state_apply(s_led_pwr_state);
}

void led_power_set_state(led_pwr_state_t state)
{
    /* JOINED는 타이머 잔존 방지를 위해 항상 재적용 */
    if (s_led_pwr_state == state && state != LED_PWR_STATE_JOINED) return;
    s_led_pwr_state = state;
    led_state_apply(state);
}

void led_power_init(void)
{
    drv_gpio_func_set(LED_POWER);
    drv_gpio_output_en(LED_POWER, 1);
    drv_gpio_input_en(LED_POWER, 0);
    drv_gpio_write(LED_POWER, 0);       /* 초기 OFF */
    /* LED_PERMIT 은 LED_POWER(PC0) 와 같은 핀 — 별도 초기화 불필요 */
}

/*====================================================================
 * pwm_hw_init
 *==================================================================*/
void pwm_hw_init(void)
{
    /* PD7 — DEBUG soft-TX idle HIGH */
    gpio_set_func(GPIO_PD7, AS_GPIO);
    gpio_set_output_en(GPIO_PD7, 1);
    gpio_set_input_en(GPIO_PD7, 0);
    gpio_write(GPIO_PD7, 1);

    led_power_init();
    led_power_set_state(LED_PWR_STATE_NOT_JOINED);

    pwm_channel_init();
}

/*====================================================================
 * Identify 블링크 — times 회 깜빡인 후 원래 LED 상태로 자동 복귀
 *   times = 0 이면 light_blink_stop() 호출 전까지 계속
 *==================================================================*/
void light_blink_start(u8 times, u16 ledOnTime, u16 ledOffTime)
{
    led_blink_run(ledOnTime, ledOffTime, (u16)times * 2);
}

void light_blink_stop(void)
{
    /* Identify 종료 → 현재 state 재적용 */
    led_timer_cancel();
    led_state_reapply();
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
