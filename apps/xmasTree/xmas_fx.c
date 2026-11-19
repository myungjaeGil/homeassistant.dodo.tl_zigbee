/*********************************************************************
 * @file    xmas_fx.c
 * @brief   크리스마스 트리 3CH 효과 엔진
 *
 *  - 타이머 1개 (XMAS_FX_TICK_MS = 20ms) — 효과가 동작할 때만 존재
 *  - 위상(phase) 16bit 누산기: 0~65535 가 효과 1주기. 속도(1~10)로 주기 결정
 *  - 결과는 채널별 게인(0~255) → pwm_hw_set_fx_gain() 이 레벨·감마·디밍구간 적용 후 즉시 출력
 *********************************************************************/
#include "tl_common.h"
#include "xmas_ep.h"
#include "xmas_fx.h"

#ifndef XMAS_FX_TICK_MS
#define XMAS_FX_TICK_MS     20
#endif

#define NCH                 PWM_PHY_CH_COUNT
#define PHASE_THIRD         21845u      /* 65536 / 3 */

/* 속도별 효과 1주기 (ms) — index 1~10 */
static const u16 s_cycle_ms[XMAS_SPEED_MAX + 1] = {
    0, 8000, 6000, 4500, 3500, 2600, 2000, 1500, 1100, 800, 550
};

static const char * const s_fx_names[XMAS_FX_COUNT] = {
    "STEADY", "WAVES", "SEQUENCE", "GLOW", "CHASE",
    "XFADE", "TWINKLE", "FLASH", "ALTERNATE", "AUTO"
};

static ev_timer_event_t *s_fx_tmr     = NULL;
static u16               s_phase      = 0;
static u8                s_lastEffect = 0xFF;  /* 마지막으로 적용한 설정 효과 */
static u8                s_anim       = 0;     /* 실제 동작 중인 애니메이션 (1~8) */
static u32               s_autoMs     = 0;

/* TWINKLE 상태 */
static u8  s_tw_gain[NCH];
static u8  s_tw_target[NCH];
static u16 s_tw_hold[NCH];
static u32 s_rand = 0x12345678;

static u32 fx_rand(void)
{
    /* xorshift32 */
    s_rand ^= s_rand << 13;
    s_rand ^= s_rand >> 17;
    s_rand ^= s_rand << 5;
    return s_rand;
}

static inline u8 tri8(u16 p)
{
    return (p < 32768u) ? (u8)(p >> 7) : (u8)((65535u - p) >> 7);
}

static u16 fx_cycle_ms(void)
{
    u8 sp = g_xmasFxCfg.speed;
    if (sp < XMAS_SPEED_MIN) sp = XMAS_SPEED_MIN;
    if (sp > XMAS_SPEED_MAX) sp = XMAS_SPEED_MAX;
    return s_cycle_ms[sp];
}

const char *xmas_fx_name(u8 effect)
{
    return (effect < XMAS_FX_COUNT) ? s_fx_names[effect] : "?";
}

bool xmas_fx_is_active(void)
{
    return (s_fx_tmr != NULL);
}

/*--------------------------------------------------------------------
 * 애니메이션별 게인 계산
 *------------------------------------------------------------------*/
static void fx_twinkle_reset(void)
{
    u8 i;
    for (i = 0; i < NCH; i++) {
        s_tw_gain[i]   = 0;
        s_tw_target[i] = 0;
        s_tw_hold[i]   = (u16)(fx_rand() % 10);
    }
}

static void fx_twinkle(u8 g[NCH])
{
    u16 cyc  = fx_cycle_ms();
    u16 step = (u16)(255u * XMAS_FX_TICK_MS * 4 / cyc);   /* 1/4 주기에 0→255 */
    u8  i;

    if (step < 1) step = 1;

    for (i = 0; i < NCH; i++) {
        u8 cur = s_tw_gain[i], tgt = s_tw_target[i];

        if (cur < tgt)      cur = (tgt - cur > step) ? cur + step : tgt;
        else if (cur > tgt) cur = (cur - tgt > step) ? cur - step : tgt;
        s_tw_gain[i] = cur;

        if (cur == tgt) {
            if (s_tw_hold[i]) {
                s_tw_hold[i]--;
            } else {
                u32 r = fx_rand();
                /* 반은 밝게, 반은 거의 꺼짐 — 유지 시간은 주기의 1/8 ~ 1/2 */
                s_tw_target[i] = (r & 1) ? (u8)(180 + (r >> 8) % 76) : (u8)((r >> 8) % 40);
                s_tw_hold[i]   = (u16)((cyc / XMAS_FX_TICK_MS) / 8 +
                                       (r >> 16) % ((cyc / XMAS_FX_TICK_MS) * 3 / 8 + 1));
            }
        }
        g[i] = s_tw_gain[i];
    }
}

static void fx_compute(u8 anim, u16 p, u8 g[NCH])
{
    u8 i;

    switch (anim) {
    case XMAS_FX_WAVES:
        for (i = 0; i < NCH; i++) g[i] = tri8((u16)(p + i * PHASE_THIRD));
        break;

    case XMAS_FX_SEQUENTIAL: {
        u8 step = (u8)(((u32)p * 4) >> 16);          /* 0~3 */
        for (i = 0; i < NCH; i++) g[i] = (step < 3 && i <= step) ? 255 : 0;
        break;
    }

    case XMAS_FX_GLOW:
        for (i = 0; i < NCH; i++) g[i] = tri8(p);
        break;

    case XMAS_FX_CHASE: {
        u8 step = (u8)(((u32)p * NCH) >> 16);        /* 0~2 */
        for (i = 0; i < NCH; i++) g[i] = (i == step) ? 255 : 0;
        break;
    }

    case XMAS_FX_CROSSFADE:
        /* 채널 i 는 위상 i/3 에서 최대, ±1/3 에서 0 — 이웃 두 채널 합이 항상 255 */
        for (i = 0; i < NCH; i++) {
            s16 d  = (s16)(u16)(p - i * PHASE_THIRD);
            u32 ad = (d < 0) ? (u32)(-(s32)d) : (u32)d;
            g[i] = (ad < PHASE_THIRD) ? (u8)(255 - ad * 255 / PHASE_THIRD) : 0;
        }
        break;

    case XMAS_FX_TWINKLE:
        fx_twinkle(g);
        break;

    case XMAS_FX_FLASH:
        for (i = 0; i < NCH; i++) g[i] = (p < 32768u) ? 255 : 0;
        break;

    case XMAS_FX_ALTERNATE:
        for (i = 0; i < NCH; i++) g[i] = (((i & 1) == 0) == (p < 32768u)) ? 255 : 0;
        break;

    default:
        for (i = 0; i < NCH; i++) g[i] = 255;
        break;
    }
}

/*--------------------------------------------------------------------
 * 애니메이션 시작 (위상 초기화)
 *------------------------------------------------------------------*/
static void fx_anim_begin(u8 anim)
{
    s_anim  = anim;
    s_phase = 0;
    if (anim == XMAS_FX_TWINKLE) fx_twinkle_reset();
    g_xmasFxRunning = anim;
    DBG_LOG("[FX] run %s\r\n", xmas_fx_name(anim));
}

static s32 fx_tick_cb(void *arg)
{
    u8  g[NCH];
    u32 inc;

    (void)arg;

    /* 자동 순환 */
    if (g_xmasFxCfg.effect == XMAS_FX_AUTO) {
        s_autoMs += XMAS_FX_TICK_MS;
        if (s_autoMs >= (u32)g_xmasFxCfg.autoSec * 1000u) {
            u8 next = (s_anim >= XMAS_FX_LAST_ANIM) ? XMAS_FX_FIRST_ANIM : (u8)(s_anim + 1);
            s_autoMs = 0;
            fx_anim_begin(next);
        }
    }

    fx_compute(s_anim, s_phase, g);
    pwm_hw_set_fx_gain(g);

    inc = (65536u * XMAS_FX_TICK_MS) / fx_cycle_ms();
    s_phase = (u16)(s_phase + inc);

    return XMAS_FX_TICK_MS;
}

/*--------------------------------------------------------------------
 * 엔진 시작/정지 판단
 *------------------------------------------------------------------*/
void xmas_fx_refresh(void)
{
    u8   eff  = g_xmasFxCfg.effect;
    bool want = g_pwmChAttrs[PWM_MASTER_IDX].onOff &&
                (eff != XMAS_FX_STEADY) && (eff < XMAS_FX_COUNT);

    if (eff != s_lastEffect) {
        s_lastEffect = eff;
        s_autoMs     = 0;
        if (eff == XMAS_FX_AUTO) {
            fx_anim_begin(XMAS_FX_FIRST_ANIM);
        } else if (eff != XMAS_FX_STEADY && eff < XMAS_FX_COUNT) {
            fx_anim_begin(eff);
        } else {
            s_anim = XMAS_FX_STEADY;
            g_xmasFxRunning = XMAS_FX_STEADY;
        }
    }

    if (want && !s_fx_tmr) {
        s_phase = 0;
        if (s_anim == XMAS_FX_TWINKLE) fx_twinkle_reset();
        fx_tick_cb(NULL);                                   /* 첫 프레임 즉시 */
        s_fx_tmr = TL_ZB_TIMER_SCHEDULE(fx_tick_cb, NULL, XMAS_FX_TICK_MS);
        DBG_LOG("[FX] engine start (%s)\r\n", xmas_fx_name(eff));
    } else if (!want && s_fx_tmr) {
        TL_ZB_TIMER_CANCEL(&s_fx_tmr);
        s_fx_tmr = NULL;
        pwm_hw_clear_fx_gain();
        DBG_LOG("[FX] engine stop\r\n");
    }
}

void xmas_fx_init(void)
{
    s_rand ^= clock_time();
    if (s_rand == 0) s_rand = 0x12345678;
    s_lastEffect = 0xFF;
    xmas_fx_refresh();
}

/*--------------------------------------------------------------------
 * 로컬 조작 — 속성을 바꾸고 xmas_fxCfg_check() 로 보정·저장·반영
 *------------------------------------------------------------------*/
void xmas_fx_select(u8 effect)
{
    if (effect >= XMAS_FX_COUNT) return;
    g_xmasFxCfg.effect = effect;
    xmas_fxCfg_check();
}

void xmas_fx_next(void)
{
    u8 e = g_xmasFxCfg.effect;
    xmas_fx_select((e + 1 >= XMAS_FX_COUNT) ? 0 : (u8)(e + 1));
}

void xmas_fx_prev(void)
{
    u8 e = g_xmasFxCfg.effect;
    xmas_fx_select((e == 0 || e >= XMAS_FX_COUNT) ? (u8)(XMAS_FX_COUNT - 1) : (u8)(e - 1));
}

void xmas_fx_speed_step(s8 delta)
{
    s16 sp = (s16)g_xmasFxCfg.speed + delta;
    if (sp < XMAS_SPEED_MIN) sp = XMAS_SPEED_MIN;
    if (sp > XMAS_SPEED_MAX) sp = XMAS_SPEED_MAX;
    g_xmasFxCfg.speed = (u8)sp;
    xmas_fxCfg_check();
}
