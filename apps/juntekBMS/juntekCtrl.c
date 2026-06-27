/*********************************************************************
 * @file    juntekCtrl.c
 * @brief   ZT3L JUNTEK JUNTEK BMS 모니터 — LED/버튼 제어
 *
 * PWM 채널 제어 완전 제거
 * LED_POWER 상태 관리 + 버튼 blink 유지
 *********************************************************************/

#include "tl_common.h"
#include "zcl_include.h"
#include "juntekMonitor.h"
#include "juntekCtrl.h"

extern void sampleLight_onOffInit(void);
#if ZCL_LEVEL_CTRL_SUPPORT
extern void sampleLight_levelInit(void);
#endif

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
    drv_gpio_write(LED_POWER, s_led_pwr_level ? 1 : 0);
    return interval_ms;
}

static void led_power_blink_start(u16 interval_ms)
{
    if (s_led_pwr_tmr) {
        TL_ZB_TIMER_CANCEL(&s_led_pwr_tmr);
    }
    s_led_pwr_level = TRUE;
    drv_gpio_write(LED_POWER, 1);
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
    drv_gpio_write(LED_POWER, on ? 1 : 0);
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
    /* PC0 — LED_POWER */
    drv_gpio_func_set(LED_POWER);
    drv_gpio_output_en(LED_POWER, 1);
    drv_gpio_input_en(LED_POWER, 0);
    drv_gpio_write(LED_POWER, 0);   /* active high: 초기 OFF */

    /* PA0 — LED_PERMIT (LED_POWER와 다른 핀) */
    drv_gpio_func_set(LED_PERMIT);
    drv_gpio_output_en(LED_PERMIT, 1);
    drv_gpio_input_en(LED_PERMIT, 0);
    drv_gpio_write(LED_PERMIT, 0);  /* active high: 초기 OFF */
}

/*====================================================================
 * D2핀 릴레이 출력 (GPIO_PD2) — EP6: 주행충전 ON/OFF
 *  active HIGH: 1=주행충전 ON, 0=OFF
 *==================================================================*/

void relay_drvchg_init(void)
{
    drv_gpio_func_set(RELAY_DRV_CHG);
    drv_gpio_output_en(RELAY_DRV_CHG, 1);
    drv_gpio_input_en(RELAY_DRV_CHG, 0);
    drv_gpio_write(RELAY_DRV_CHG, 0);   /* 초기 OFF */
    printf("[relay_drvchg_init] PD2 GPIO output configured\r\n");
}

void relay_drvchg_set(bool on)
{
    printf("[relay_drvchg_set] PD2 <= %d\r\n", on ? 1 : 0);
    drv_gpio_write(RELAY_DRV_CHG, on ? 1 : 0);
}

/*====================================================================
 * C3핀 릴레이 출력 (GPIO_PC3) — EP7: 충전전류 Full/Half
 *  [수정] 기존 "BMS 충전/방전 상태 자동 출력" 핀에서 충전전류
 *         제어용으로 용도 변경 (실제 하드웨어 회로 기준)
 *  active HIGH: 1=Half, 0=Full
 *==================================================================*/

void relay_chgcur_init(void)
{
    drv_gpio_func_set(RELAY_CHG_CUR);
    drv_gpio_output_en(RELAY_CHG_CUR, 1);
    drv_gpio_input_en(RELAY_CHG_CUR, 0);
    drv_gpio_write(RELAY_CHG_CUR, 0);   /* 초기 Full(OFF) */
    printf("[relay_chgcur_init] PC3 GPIO output configured\r\n");
}

void relay_chgcur_set(bool on)
{
    printf("[relay_chgcur_set] PC3 <= %d\r\n", on ? 1 : 0);
    drv_gpio_write(RELAY_CHG_CUR, on ? 1 : 0);
}

/*====================================================================
 * juntek_hw_init — LED + 릴레이 초기화 (PWM 없음)
 *==================================================================*/
void juntek_hw_init(void)
{
    /* DEBUG TX 핀 강제 초기화 */
    gpio_set_func(GPIO_PD7, AS_GPIO);
    gpio_set_output_en(GPIO_PD7, 1);
    gpio_set_input_en(GPIO_PD7, 0);
    gpio_write(GPIO_PD7, 1);

    led_power_init();
    relay_drvchg_init();
    relay_chgcur_init();
    led_power_set_state(LED_PWR_STATE_NOT_JOINED);
    //printf("juntek_hw_init done\r\n");
}

/*====================================================================
 * SDK 호환 스텁 — OnOff/Level 콜백 없음, 빈 구현
 *==================================================================*/
void sampleLight_onOffInit(void)  { /* BMS: 미사용 */ }
void sampleLight_onOffUpdate(u8 cmd)
{
    /* [디버그] 이 로그가 찍히면 SDK의 zcl_onoff.c가 EP별 cb 대신
     * 이 전역 함수를 직접 호출하고 있다는 뜻 — juntek_drvChgCb/
     * juntek_chgCurCb가 호출되지 않는 진짜 원인일 수 있음 */
    printf("[sampleLight_onOffUpdate] HIT! cmd=0x%02x (전역 스텁이 호출됨)\r\n", cmd);
}
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
 * LED Blink (Identify/OTA 등 일시적 표시용)
 *
 * [수정] 깜빡임이 끝나면 무조건 OFF로 복귀하던 기존 동작을 제거.
 *        대신 led_power_set_state() 가 관리하는 "정상 상태"
 *        (JOINED=ON / NOT_JOINED=깜빡임)로 복원한다.
 *        그렇지 않으면 조인 직후 Identify/OTA 트리거가 들어왔을 때
 *        깜빡임 종료 시 LED가 OFF로 떨어지는 문제가 발생함
 *        (join 성공 LED ON 상태를 덮어씀).
 *==================================================================*/

static void led_power_restore_steady(void)
{
    switch (s_led_pwr_state) {
    case LED_PWR_STATE_JOINED:
        drv_gpio_write(LED_POWER, 1);      /* ON (active high) */
        break;
    case LED_PWR_STATE_NOT_JOINED:
        led_power_blink_start(500);        /* 미조인 깜빡임 재개 */
        break;
    case LED_PWR_STATE_RESET:
        led_power_blink_start(200);
        break;
    case LED_PWR_STATE_BOOT:
    default:
        drv_gpio_write(LED_POWER, 0);      /* OFF (active high) */
        break;
    }
}

static s32 blink_TimerEvtCb(void *arg)
{
    u32 interval = 0;

    gJuntekCtx.sta = !gJuntekCtx.sta;
    if (gJuntekCtx.sta) {
        drv_gpio_write(LED_POWER, 1);
        interval = gJuntekCtx.ledOnTime;
    } else {
        drv_gpio_write(LED_POWER, 0);
        interval = gJuntekCtx.ledOffTime;
    }

    if (gJuntekCtx.sta == gJuntekCtx.oriSta) {
        if (gJuntekCtx.times) {
            gJuntekCtx.times--;
            if (gJuntekCtx.times <= 0) {
                gJuntekCtx.timerLedEvt = NULL;
                led_power_restore_steady();
                return -1;
            }
        }
    }
    return interval;
}

void light_blink_start(u8 times, u16 ledOnTime, u16 ledOffTime)
{
    if (!gJuntekCtx.timerLedEvt) {
        gJuntekCtx.times      = times;
        gJuntekCtx.ledOnTime  = ledOnTime;
        gJuntekCtx.ledOffTime = ledOffTime;
        gJuntekCtx.oriSta     = 0;
        gJuntekCtx.sta        = 0;

        gJuntekCtx.timerLedEvt =
            TL_ZB_TIMER_SCHEDULE(blink_TimerEvtCb, NULL, ledOnTime);
    }
}

void light_blink_stop(void)
{
    if (gJuntekCtx.timerLedEvt) {
        TL_ZB_TIMER_CANCEL(&gJuntekCtx.timerLedEvt);
        gJuntekCtx.times = 0;
    }
    led_power_restore_steady();
}
