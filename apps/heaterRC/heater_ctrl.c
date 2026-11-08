/*********************************************************************
 * @file    heater_ctrl.c
 * @brief   ZT3L 무시동히터 리모컨 — 양방향 버튼 라인 + LED 상태머신
 *
 * 버튼 라인 (핀 하나 = 버튼 하나, 양방향):
 *   대기 : 입력 + 풀 저항 → 외부(원래 리모컨/히터 본체)의 눌림 감시
 *   누름 : 눌림 레벨 출력 pulseMs → 뗌: 출력 끄고 다시 입력 → gapMs 대기 → 다음 요청
 *   - 대기열로 순차 실행, 한 번에 한 라인만 누름
 *   - 직접 누른 라인은 누르는 동안 + HEATER_IN_BLANK_MS 동안 입력 감시 제외
 *     (자기 펄스를 외부 입력으로 다시 세지 않음)
 *
 * 상태 갱신 (직접 누름 / 외부 감지 공통 — heater_state_apply):
 *   ON → 전원 ON, OFF → 전원 OFF, UP → 레벨 +1, DOWN → 레벨 -1 (0~9 범위 고정)
 *   단일 전원 버튼 모드의 외부 POWER 눌림 → 전원 토글
 *
 * LED_POWER(PC0) — active HIGH
 *   Not Joined  : 500ms 블링크
 *   Permit Join : 100ms 블링크
 *   Joined      : 상시 점등
 *   Reset       : 200ms 블링크
 *********************************************************************/

#include "tl_common.h"
#include "zcl_include.h"
#include "heater_ep.h"
#include "heater_ctrl.h"

/*====================================================================
 * 동작 → 라인 핀
 *==================================================================*/
#if HEATER_POWER_SINGLE_BUTTON
static const u32 s_actPin[HEATER_ACT_COUNT] = {
    HEATER_PIN_POWER,   /* ON   */
    HEATER_PIN_POWER,   /* OFF  */
    HEATER_PIN_UP,
    HEATER_PIN_DOWN,
};
#else
static const u32 s_actPin[HEATER_ACT_COUNT] = {
    HEATER_PIN_ON,
    HEATER_PIN_OFF,
    HEATER_PIN_UP,
    HEATER_PIN_DOWN,
};
#endif

static const char * const s_actName[HEATER_ACT_COUNT + 1] = {
    "ON", "OFF", "UP", "DOWN", "-",
};

/*====================================================================
 * 라인 제어 — 대기 = 입력, 누름 = 출력
 *==================================================================*/
static void line_idle_init(u32 pin)
{
    drv_gpio_func_set(pin);
    drv_gpio_output_en(pin, 0);                         /* Hi-Z */
    drv_gpio_write(pin, HEATER_ACTIVE_LEVEL);           /* 출력 켤 때 바로 눌림 레벨 */
    drv_gpio_up_down_resistor(pin, HEATER_IDLE_PULL);   /* 대기 레벨로 당김 */
    drv_gpio_input_en(pin, 1);                          /* 항상 읽기 가능 */
}

static inline void line_press(u32 pin)
{
    drv_gpio_write(pin, HEATER_ACTIVE_LEVEL);
    drv_gpio_output_en(pin, 1);
}

static inline void line_release(u32 pin)
{
    /* 반대 레벨을 직접 출력하지 않고 Hi-Z 로 — 외부 버튼과 충돌 없음 */
    drv_gpio_output_en(pin, 0);
}

static inline bool line_is_active(u32 pin)
{
    return ((drv_gpio_read(pin) ? 1 : 0) == HEATER_ACTIVE_LEVEL);
}

void heater_out_init(void)
{
    line_idle_init(HEATER_PIN_ON);
    line_idle_init(HEATER_PIN_OFF);
    line_idle_init(HEATER_PIN_UP);
    line_idle_init(HEATER_PIN_DOWN);

    /* 사용 안 하는 PD2 (Pin5) — 출력 끔, 풀 저항 없음 */
    drv_gpio_func_set(HEATER_PIN_UNUSED);
    drv_gpio_output_en(HEATER_PIN_UNUSED, 0);
    drv_gpio_up_down_resistor(HEATER_PIN_UNUSED, PM_PIN_UP_DOWN_FLOAT);
    drv_gpio_input_en(HEATER_PIN_UNUSED, 1);
}

/*====================================================================
 * 외부 입력 감시 채널
 *==================================================================*/
#define IN_ACT_POWER_TOGGLE     HEATER_ACT_COUNT    /* 단일 전원 버튼 외부 눌림 */

typedef struct {
    u32  pin;
    u8   act;           /* heater_act_t 또는 IN_ACT_POWER_TOGGLE */
    u8   raw;           /* 마지막 원시 읽기 (1 = 눌림) */
    u8   stable;        /* 디바운스된 상태 */
    u8   blank;         /* 1 = 직접 누르는 중 / 뗀 직후 → 감시 제외 */
    u32  rawTick;       /* raw 가 바뀐 시각 */
    u32  blankTick;     /* 뗀 시각 */
} in_ch_t;

static in_ch_t s_in[] = {
#if HEATER_POWER_SINGLE_BUTTON
    { HEATER_PIN_POWER, IN_ACT_POWER_TOGGLE, 0, 0, 0, 0, 0 },
#else
    { HEATER_PIN_ON,    HEATER_ACT_ON,       0, 0, 0, 0, 0 },
    { HEATER_PIN_OFF,   HEATER_ACT_OFF,      0, 0, 0, 0, 0 },
#endif
    { HEATER_PIN_UP,    HEATER_ACT_UP,       0, 0, 0, 0, 0 },
    { HEATER_PIN_DOWN,  HEATER_ACT_DOWN,     0, 0, 0, 0, 0 },
};
#define IN_CH_NUM   (sizeof(s_in) / sizeof(s_in[0]))

static in_ch_t *in_find(u32 pin)
{
    u8 i;
    for (i = 0; i < IN_CH_NUM; i++) {
        if (s_in[i].pin == pin) return &s_in[i];
    }
    return NULL;
}

/*====================================================================
 * 상태 갱신 — 직접 누름 / 외부 감지 공통
 *==================================================================*/
static void heater_state_apply(heater_act_t act, u8 src)
{
    switch (act) {
    case HEATER_ACT_ON:
        g_heaterOnOff[HEATER_IDX_POWER] = TRUE;
        break;
    case HEATER_ACT_OFF:
        g_heaterOnOff[HEATER_IDX_POWER] = FALSE;
        break;
    case HEATER_ACT_UP:
        if (g_heaterLevel < HEATER_LEVEL_MAX) g_heaterLevel++;
        break;
    case HEATER_ACT_DOWN:
        if (g_heaterLevel > HEATER_LEVEL_MIN) g_heaterLevel--;
        break;
    default:
        return;
    }
    g_heaterLastAction = (u8)act;
    g_heaterLastSource = src;
    heater_state_save();

    DBG_LOG("[HTR] %s %s -> power=%d level=%d\r\n",
            (src == HEATER_SRC_EXTERNAL) ? "ext" : "rc", s_actName[act],
            (int)g_heaterOnOff[HEATER_IDX_POWER], (int)g_heaterLevel);
}

/* 외부에서 라인이 눌림 레벨이 됨 (디바운스 완료) */
static void heater_external_press(u8 act)
{
    if (act == IN_ACT_POWER_TOGGLE) {
        act = g_heaterOnOff[HEATER_IDX_POWER] ? HEATER_ACT_OFF : HEATER_ACT_ON;
    }
#if HEATER_UPDN_REQUIRE_ON
    if ((act == HEATER_ACT_UP || act == HEATER_ACT_DOWN) &&
        !g_heaterOnOff[HEATER_IDX_POWER]) {
        /* 꺼진 히터는 UP/DOWN 에 반응하지 않으므로 레벨은 그대로, 입력 기록만 */
        g_heaterLastAction = act;
        g_heaterLastSource = HEATER_SRC_EXTERNAL;
        DBG_LOG("[HTR] ext %s while off — level kept\r\n", s_actName[act]);
        return;
    }
#endif
    heater_state_apply((heater_act_t)act, HEATER_SRC_EXTERNAL);
}

/*====================================================================
 * 진단 — 라인 원시 레벨 변화 로그 (HEATER_IN_RAW_LOG)
 *==================================================================*/
#if HEATER_IN_RAW_LOG
static const char *in_ch_name(const in_ch_t *ch)
{
    return (ch->act == IN_ACT_POWER_TOGGLE) ? "POWER" : s_actName[ch->act];
}

static void raw_log(const in_ch_t *ch, u8 raw)
{
    static u32 s_winTick  = 0;
    static u16 s_winCount = 0;
    static u16 s_dropped  = 0;

    if (clock_time_exceed(s_winTick, 1000 * 1000)) {
        if (s_dropped) {
            DBG_LOG("[RAW] ... %d more edges in last 1s (line floating/noisy?)\r\n", (int)s_dropped);
        }
        s_winTick  = clock_time();
        s_winCount = 0;
        s_dropped  = 0;
    }
    if (++s_winCount > 30) {
        s_dropped++;
        return;
    }
    DBG_LOG("[RAW] %s %d%s t=%dms\r\n", in_ch_name(ch), (int)raw,
            ch->blank ? " (self)" : "",
            (int)(clock_time() / CLOCK_16M_SYS_TIMER_CLK_1MS));
}

static void raw_log_boot(void)
{
    static bool s_done = FALSE;
    u8 i;
    if (s_done) return;
    s_done = TRUE;
    for (i = 0; i < IN_CH_NUM; i++) {
        DBG_LOG("[RAW] boot %s=%d\r\n", in_ch_name(&s_in[i]),
                line_is_active(s_in[i].pin) ? 1 : 0);
    }
}
#endif

/*====================================================================
 * heater_in_poll — 메인 루프에서 호출, 외부 입력 감지
 *==================================================================*/
void heater_in_poll(void)
{
#if HEATER_IN_MONITOR_ENABLE
    u8 i;
#if HEATER_IN_RAW_LOG
    raw_log_boot();
#endif
    for (i = 0; i < IN_CH_NUM; i++) {
        in_ch_t *ch = &s_in[i];
        u8 raw = line_is_active(ch->pin) ? 1 : 0;

        if (raw != ch->raw) {
            ch->raw     = raw;
            ch->rawTick = clock_time();
#if HEATER_IN_RAW_LOG
            raw_log(ch, raw);
#endif
        }

        if (ch->blank) {
            /* 직접 누르는 중(blankTick==0) 또는 뗀 뒤 BLANK 시간 동안 — 엣지로 세지 않음 */
            ch->stable = raw;
            if (ch->blankTick &&
                clock_time_exceed(ch->blankTick, HEATER_IN_BLANK_MS * 1000) && !raw) {
                ch->blank = 0;
            }
            continue;
        }

        if (raw != ch->stable &&
            clock_time_exceed(ch->rawTick, HEATER_IN_DEBOUNCE_MS * 1000)) {
            ch->stable = raw;
            if (raw) heater_external_press(ch->act);    /* 눌림 엣지 1회 */
        }
    }
#endif
}

/*====================================================================
 * 대기열
 *==================================================================*/
static u8  s_q[HEATER_QUEUE_SIZE];
static u8  s_qHead  = 0;
static u8  s_qCount = 0;

static bool q_push(heater_act_t act)
{
    if (s_qCount >= HEATER_QUEUE_SIZE) return FALSE;
    s_q[(s_qHead + s_qCount) % HEATER_QUEUE_SIZE] = (u8)act;
    s_qCount++;
    return TRUE;
}

static bool q_pop(heater_act_t *act)
{
    if (s_qCount == 0) return FALSE;
    *act = (heater_act_t)s_q[s_qHead];
    s_qHead = (s_qHead + 1) % HEATER_QUEUE_SIZE;
    s_qCount--;
    return TRUE;
}

/*====================================================================
 * 펄스 실행 상태머신
 *==================================================================*/
typedef enum {
    OUT_PHASE_IDLE = 0,
    OUT_PHASE_PRESS,    /* 라인 출력 (버튼 누름) */
    OUT_PHASE_GAP,      /* 라인 입력 복귀, 다음 명령 전 대기 */
} out_phase_t;

static ev_timer_event_t *s_outTmr   = NULL;
static u8                s_phase    = OUT_PHASE_IDLE;
static heater_act_t      s_curAct   = HEATER_ACT_COUNT;

/* 동작 완료 알림 — heater_main.c 에서 구현 (UP/DOWN EP 자동 Off 등) */
extern void heater_onActDone(heater_act_t act);

static u16 act_pulse_ms(heater_act_t act)
{
    return (act == HEATER_ACT_ON || act == HEATER_ACT_OFF)
           ? g_heaterCfg.powerPulseMs : g_heaterCfg.btnPulseMs;
}

static void press_start(heater_act_t act)
{
    u32 pin = s_actPin[act];
    in_ch_t *ch = in_find(pin);

    if (ch) {               /* 자기 펄스는 입력 감시 제외 */
        ch->blank     = 1;
        ch->blankTick = 0;
    }
    s_curAct = act;
    s_phase  = OUT_PHASE_PRESS;
    line_press(pin);
    DBG_LOG("[HTR] press %s (%dms)\r\n", s_actName[act], (int)act_pulse_ms(act));
}

static void press_end(heater_act_t act)
{
    u32 pin = s_actPin[act];
    in_ch_t *ch = in_find(pin);

    line_release(pin);
    if (ch) {
        ch->blankTick = clock_time();
        if (ch->blankTick == 0) ch->blankTick = 1;  /* 0 은 "누르는 중" 표시용 */
    }
}

static s32 heater_out_cb(void *arg)
{
    heater_act_t next;
    (void)arg;

    if (s_phase == OUT_PHASE_PRESS) {
        press_end(s_curAct);
        heater_onActDone(s_curAct);
        s_phase = OUT_PHASE_GAP;
        return g_heaterCfg.gapMs;
    }

    /* GAP 끝 → 다음 명령 */
    if (q_pop(&next)) {
        press_start(next);
        return act_pulse_ms(next);
    }

    s_phase   = OUT_PHASE_IDLE;
    s_curAct  = HEATER_ACT_COUNT;
    s_outTmr  = NULL;
    return -1;
}

bool heater_request(heater_act_t act)
{
    heater_act_t first;

    if (act >= HEATER_ACT_COUNT) return FALSE;
    if (!q_push(act)) {
        DBG_LOG("[HTR] queue full, drop %s\r\n", s_actName[act]);
        return FALSE;
    }

    if (s_phase == OUT_PHASE_IDLE && q_pop(&first)) {
        press_start(first);
        s_outTmr = TL_ZB_TIMER_SCHEDULE(heater_out_cb, NULL, act_pulse_ms(first));
    }
    return TRUE;
}

bool heater_is_busy(void)
{
    return (s_phase != OUT_PHASE_IDLE) || (s_qCount != 0);
}

u8 heater_pending(void)
{
    return s_qCount + ((s_phase == OUT_PHASE_PRESS) ? 1 : 0);
}

heater_act_t heater_last_act(void)
{
    return (g_heaterLastAction < HEATER_ACT_COUNT) ? (heater_act_t)g_heaterLastAction
                                                    : HEATER_ACT_COUNT;
}

const char *heater_act_name(heater_act_t act)
{
    return s_actName[(act < HEATER_ACT_COUNT) ? act : HEATER_ACT_COUNT];
}

/*====================================================================
 * 히터 동작 — Zigbee / IR / 버튼 공용
 *   요청이 받아들여지면 그 시점에 상태 갱신 (출처 = 리모컨)
 *==================================================================*/
bool heater_power_set(bool on)
{
#if HEATER_POWER_SINGLE_BUTTON
    /* 전원 버튼 하나로 토글 — 같은 상태 요청에 누르면 반대로 바뀌므로 무시 */
    if (g_heaterOnOff[HEATER_IDX_POWER] == on) {
        DBG_LOG("[HTR] power already %s — skip\r\n", on ? "ON" : "OFF");
        return TRUE;
    }
#endif
    if (!heater_request(on ? HEATER_ACT_ON : HEATER_ACT_OFF)) return FALSE;
    heater_state_apply(on ? HEATER_ACT_ON : HEATER_ACT_OFF, HEATER_SRC_REMOTE);
    return TRUE;
}

bool heater_power_toggle(void)
{
    return heater_power_set(!g_heaterOnOff[HEATER_IDX_POWER]);
}

bool heater_step(bool up)
{
    heater_act_t act = up ? HEATER_ACT_UP : HEATER_ACT_DOWN;

#if HEATER_UPDN_REQUIRE_ON
    if (!g_heaterOnOff[HEATER_IDX_POWER]) {
        DBG_LOG("[HTR] %s ignored — heater off\r\n", s_actName[act]);
        return FALSE;
    }
#endif
    /* 범위 끝(0 또는 9)이어도 누름은 보냄 — 실제 히터와 어긋난 경우 다시 맞추는 효과 */
    if (!heater_request(act)) return FALSE;
    heater_state_apply(act, HEATER_SRC_REMOTE);

    /* EP2/EP3 순간 버튼 — 펄스가 끝날 때 heater_onActDone() 에서 Off */
    g_heaterOnOff[up ? HEATER_IDX_UP : HEATER_IDX_DOWN] = TRUE;
    return TRUE;
}

/*====================================================================
 * LED_POWER 상태 관리 — 단일 타이머, active HIGH
 *==================================================================*/
static led_pwr_state_t   s_led_pwr_state = LED_PWR_STATE_BOOT;
static ev_timer_event_t *s_led_tmr       = NULL;
static bool              s_led_level     = FALSE;

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
    drv_gpio_write(LED_POWER, s_led_level ? 1 : 0);

    if (s_blink_toggles) {
        if (--s_blink_toggles == 0) {
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
    drv_gpio_write(LED_POWER, 1);
    s_led_tmr = TL_ZB_TIMER_SCHEDULE(led_blink_cb, NULL, s_blink_on_ms);
}

static void led_blink_stop(bool on)
{
    led_timer_cancel();
    drv_gpio_write(LED_POWER, on ? 1 : 0);
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
    if (s_led_pwr_state == state && state != LED_PWR_STATE_JOINED) return;
    s_led_pwr_state = state;
    led_state_apply(state);
}

void led_power_init(void)
{
    drv_gpio_func_set(LED_POWER);
    drv_gpio_output_en(LED_POWER, 1);
    drv_gpio_input_en(LED_POWER, 0);
    drv_gpio_write(LED_POWER, 0);
}

void light_blink_start(u8 times, u16 ledOnTime, u16 ledOffTime)
{
    led_blink_run(ledOnTime, ledOffTime, (u16)times * 2);
}

void light_blink_stop(void)
{
    led_timer_cancel();
    led_state_reapply();
}

/*====================================================================
 * SDK 호환 스텁 (sampleLight 계열 공용 소스가 참조할 수 있음)
 *==================================================================*/
void sampleLight_onOffInit(void)     { }
void sampleLight_onOffUpdate(u8 cmd) { (void)cmd; }
void light_refresh(u8 sta)           { (void)sta; }
