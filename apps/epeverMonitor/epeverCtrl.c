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
 * 상태 LED (PC0 한 개) 관리
 *
 *   No | 상태                      | PC0 LED 표시
 *  ----+---------------------------+----------------------------------
 *   1  | 공장초기화 중 (RESET)     | 100ms 점멸 (매우 빠름)
 *   2  | 미가입 / 가입 시도 중     | 500ms 점멸 (일정한 깜빡임)
 *   3  | 가입됨 + Z2M 응답 없음    | 두 번 깜빡 + 1.2초 쉼 (더블 블링크)
 *   4  | 가입됨 + Z2M 연결됨       | 계속 켜짐
 *   5  | 부팅 직후 (BOOT)          | 꺼짐
 *  ----+---------------------------+----------------------------------
 *   - 여러 조건이 겹치면 순위가 높은(숫자가 작은) 표시가 우선
 *   - Permit Join 열림은 LED 에 표시하지 않음 (로그만 출력)
 *   - Identify 명령 수신 중에는 identify 점멸이 우선, 끝나면 위 표시로 복귀
 *   - Z2M 끊김 판단: 30초마다 코디네이터로 heartbeat 리포트 전송,
 *     Default Response 5초 내 미수신 LINK_MISS_MAX(3)회 연속 → 3번 표시
 *     (zb_appCb.c), 코디네이터로부터 ZCL 수신 시 즉시 4번으로 복귀
 *   - 극성은 board_8258_zt3l.h 의 LED_ACTIVE_LEVEL 로 설정
 *==================================================================*/
static led_pwr_state_t  s_led_pwr_state  = LED_PWR_STATE_BOOT;
static ev_timer_event_t *s_led_pwr_tmr   = NULL;
static bool              s_led_permit    = FALSE;
static bool              s_led_identify  = FALSE;
static bool              s_led_link_ok   = TRUE;

/* 점멸 패턴: {켜짐ms, 꺼짐ms, 켜짐ms, 꺼짐ms, ...} 를 반복 */
static const u16 LED_PAT_RESET[]      = { 100, 100 };
static const u16 LED_PAT_NOT_JOINED[] = { 500, 500 };
static const u16 LED_PAT_Z2M_LOST[]   = { 150, 150, 150, 1200 };

static const u16 *s_led_pat     = NULL;
static u8         s_led_pat_len = 0;
static u8         s_led_pat_idx = 0;

#define LED_PAT(p)  (p), (u8)(sizeof(p) / sizeof((p)[0]))

static s32 led_pattern_cb(void *arg)
{
    (void)arg;
    s_led_pat_idx++;
    if (s_led_pat_idx >= s_led_pat_len) {
        s_led_pat_idx = 0;
    }
    /* 짝수 인덱스 = 켜짐 구간, 홀수 인덱스 = 꺼짐 구간 */
    drv_gpio_write(LED_POWER, (s_led_pat_idx & 1) ? LED_LEVEL_OFF : LED_LEVEL_ON);
    return s_led_pat[s_led_pat_idx];
}

static void led_pattern_stop(void)
{
    if (s_led_pwr_tmr) {
        TL_ZB_TIMER_CANCEL(&s_led_pwr_tmr);
        s_led_pwr_tmr = NULL;
    }
    s_led_pat = NULL;
}

static void led_pattern_start(const u16 *pat, u8 len)
{
    if (s_led_pat == pat && s_led_pwr_tmr) {
        return;                     /* 같은 패턴이 이미 동작 중 */
    }
    led_pattern_stop();
    s_led_pat     = pat;
    s_led_pat_len = len;
    s_led_pat_idx = 0;
    drv_gpio_write(LED_POWER, LED_LEVEL_ON);
    s_led_pwr_tmr = TL_ZB_TIMER_SCHEDULE(led_pattern_cb, NULL, pat[0]);
}

static void led_solid(bool on)
{
    led_pattern_stop();
    drv_gpio_write(LED_POWER, on ? LED_LEVEL_ON : LED_LEVEL_OFF);
}

/* 현재 상태 조합에 맞게 LED 표시를 다시 적용 */
static void led_power_refresh(void)
{
    if (s_led_identify) return;   /* identify 점멸 중에는 건드리지 않음 */

    if (s_led_pwr_state == LED_PWR_STATE_RESET) {
        led_pattern_start(LED_PAT(LED_PAT_RESET));
    } else if (s_led_pwr_state == LED_PWR_STATE_NOT_JOINED) {
        led_pattern_start(LED_PAT(LED_PAT_NOT_JOINED));
    } else if (s_led_pwr_state == LED_PWR_STATE_JOINED && !s_led_link_ok) {
        led_pattern_start(LED_PAT(LED_PAT_Z2M_LOST));
    } else if (s_led_pwr_state == LED_PWR_STATE_JOINED) {
        led_solid(TRUE);
    } else {
        led_solid(FALSE);
    }
}

void led_power_set_state(led_pwr_state_t state)
{
    if (s_led_pwr_state == state) return;
    s_led_pwr_state = state;
    printf("[LED] state=%d (0=BOOT 1=JOINED 2=NOT_JOINED 3=RESET) z2m=%d\r\n",
           (int)state, (int)s_led_link_ok);
    led_power_refresh();
}

/* Permit Join 상태는 LED 표시에 쓰지 않음 — 상태만 기록 */
void led_power_set_permit(bool open)
{
    s_led_permit = open;
}

void led_power_set_link(bool ok)
{
    if (s_led_link_ok == ok) return;
    s_led_link_ok = ok;
    printf("[LED] z2m link=%d state=%d\r\n", (int)ok, (int)s_led_pwr_state);
    led_power_refresh();
}

void led_power_init(void)
{
    drv_gpio_func_set(LED_STATUS);
    drv_gpio_output_en(LED_STATUS, 1);
    drv_gpio_input_en(LED_STATUS, 0);
    drv_gpio_write(LED_STATUS, LED_LEVEL_OFF);   /* 초기 OFF */
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
        drv_gpio_write(LED_POWER, LED_LEVEL_ON);
        interval = gEpeverCtx.ledOnTime;
    } else {
        drv_gpio_write(LED_POWER, LED_LEVEL_OFF);
        interval = gEpeverCtx.ledOffTime;
    }

    if (gEpeverCtx.sta == gEpeverCtx.oriSta) {
        if (gEpeverCtx.times) {
            gEpeverCtx.times--;
            if (gEpeverCtx.times <= 0) {
                gEpeverCtx.timerLedEvt = NULL;
                s_led_identify = FALSE;
                led_power_refresh();        /* 원래 상태 표시로 복귀 */
                return -1;
            }
        }
    }
    return interval;
}

void light_blink_start(u8 times, u16 ledOnTime, u16 ledOffTime)
{
    if (!gEpeverCtx.timerLedEvt) {
        /* 상태 표시 타이머 일시 정지 */
        led_pattern_stop();
        s_led_identify = TRUE;

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
    }
    s_led_identify = FALSE;
    led_power_refresh();                /* 원래 상태 표시로 복귀 */
}
