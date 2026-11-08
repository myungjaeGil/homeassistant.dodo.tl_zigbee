/*********************************************************************
 * @file    zcl_pwm5chCb.c
 * @brief   ZT3L PWM 5채널 — ZCL 콜백
 *          OnOff / LevelControl 명령 수신 → PWM HW 적용
 *********************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "ota.h"
#include "pwm5ch_monitor.h"
#include "pwm5ch_ep.h"
#include "pwm5ch_ctrl.h"

/*====================================================================
 * 내부 헬퍼 — EP 번호 → 채널 인덱스 (0~5)
 *==================================================================*/
static inline s8 ep_to_idx(u8 ep)
{
    if (ep >= PWM_EP_CH0 && ep <= PWM_EP_MASTER) {
        return (s8)(ep - PWM_EP_CH0);  /* EP1→0, EP6→5 */
    }
    return -1;
}

/*====================================================================
 * ZCL 수신 메시지 디스패처
 *==================================================================*/
void pwm_zclProcessIncomingMsg(zclIncoming_t *pInHdlrMsg)
{
    switch (pInHdlrMsg->hdr.cmd) {
#ifdef ZCL_READ
    case ZCL_CMD_READ_RSP:
        break;
#endif
#ifdef ZCL_WRITE
    case ZCL_CMD_WRITE:
    case ZCL_CMD_WRITE_NO_RSP:
        /* 디밍 노출 구간(0xF000/0xF001) 쓰기 → 즉시 보정·저장·적용 */
        pwm_dimRange_check();
        break;
    case ZCL_CMD_WRITE_RSP:
        break;
#endif
#ifdef ZCL_REPORT
    case ZCL_CMD_CONFIG_REPORT:
    case ZCL_CMD_CONFIG_REPORT_RSP:
    case ZCL_CMD_REPORT:
        break;
#endif
    case ZCL_CMD_DEFAULT_RSP:
        break;
    default:
        break;
    }
}

/*====================================================================
 * Basic 콜백
 *==================================================================*/
#ifdef ZCL_BASIC
status_t pwm_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    (void)pAddrInfo; (void)cmdPayload;
    if (cmdId == ZCL_CMD_BASIC_RESET_FAC_DEFAULT) {
        /* factory reset은 app_ui.c에서 처리 */
    }
    return ZCL_STA_SUCCESS;
}
#endif

/*====================================================================
 * Identify 콜백
 *==================================================================*/
#ifdef ZCL_IDENTIFY
static ev_timer_event_t *s_identifyTimerEvt = NULL;

static s32 pwm_identifyTimerCb(void *arg)
{
    (void)arg;
    if (g_zcl_identifyAttrs.identifyTime <= 0) {
        light_blink_stop();
        s_identifyTimerEvt = NULL;
        return -1;
    }
    g_zcl_identifyAttrs.identifyTime--;
    return 1000;
}

static void pwm_identifyCmdHandler(u8 ep, u16 srcAddr, u16 identifyTime)
{
    (void)ep; (void)srcAddr;
    g_zcl_identifyAttrs.identifyTime = identifyTime;
    if (identifyTime == 0) {
        if (s_identifyTimerEvt) {
            TL_ZB_TIMER_CANCEL(&s_identifyTimerEvt);
            s_identifyTimerEvt = NULL;
        }
        light_blink_stop();
    } else {
        if (!s_identifyTimerEvt) {
            light_blink_start(0, 500, 500);   /* 무한 — identify 타이머가 종료 */
            s_identifyTimerEvt = TL_ZB_TIMER_SCHEDULE(pwm_identifyTimerCb, NULL, 1000);
        }
    }
}

static void pwm_triggerEffectHandler(zcl_triggerEffect_t *pEffect)
{
    switch (pEffect->effectId) {
    case IDENTIFY_EFFECT_BLINK:          light_blink_start(1,  500, 500);  break;
    case IDENTIFY_EFFECT_BREATHE:        light_blink_start(15, 300, 700);  break;
    case IDENTIFY_EFFECT_OKAY:           light_blink_start(2,  250, 250);  break;
    case IDENTIFY_EFFECT_CHANNEL_CHANGE: light_blink_start(1,  500, 7500); break;
    case IDENTIFY_EFFECT_FINISH_EFFECT:  light_blink_start(1,  300, 700);  break;
    case IDENTIFY_EFFECT_STOP_EFFECT:    light_blink_stop();                break;
    default: break;
    }
}

status_t pwm_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    if (pAddrInfo->dirCluster == ZCL_FRAME_CLIENT_SERVER_DIR) {
        switch (cmdId) {
        case ZCL_CMD_IDENTIFY:
            pwm_identifyCmdHandler(pAddrInfo->dstEp, pAddrInfo->srcAddr,
                                   ((zcl_identifyCmd_t *)cmdPayload)->identifyTime);
            break;
        case ZCL_CMD_TRIGGER_EFFECT:
            pwm_triggerEffectHandler((zcl_triggerEffect_t *)cmdPayload);
            break;
        default:
            break;
        }
    }
    return ZCL_STA_SUCCESS;
}
#endif /* ZCL_IDENTIFY */

#ifdef ZCL_LEVEL_CTRL
static void level_cancel_all(u8 idx);   /* 아래 LevelControl 영역에 정의 */
#endif

/*====================================================================
 * OnOff 콜백
 *==================================================================*/
#ifdef ZCL_ON_OFF
status_t pwm_onOffCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    (void)cmdPayload;
    u8 ep  = pAddrInfo->dstEp;
    s8 idx = ep_to_idx(ep);
    if (idx < 0) return ZCL_STA_FAILURE;

    DBG_LOG("[ZCL] OnOff EP%d cmd=%d\r\n", (int)ep, (int)cmdId);

    switch (cmdId) {
    case ZCL_CMD_ONOFF_OFF:
    case ZCL_CMD_ONOFF_ON:
    case ZCL_CMD_ONOFF_TOGGLE:
#ifdef ZCL_LEVEL_CTRL
        /* 진행 중인 Move/MoveToLevel 이 다시 켜거나 레벨을 바꾸지 않도록 정지 */
        level_cancel_all((u8)idx);
#endif
        break;
    default:
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }

    switch (cmdId) {
    case ZCL_CMD_ONOFF_OFF:
        g_pwmChAttrs[idx].onOff = FALSE;
        break;

    case ZCL_CMD_ONOFF_ON:
        g_pwmChAttrs[idx].onOff = TRUE;
        /* OnLevel 적용 (0xFF=현재 레벨 유지) */
        if (g_pwmChAttrs[idx].onLevel != 0xFF) {
            g_pwmChAttrs[idx].currentLevel = g_pwmChAttrs[idx].onLevel;
        }
        break;

    case ZCL_CMD_ONOFF_TOGGLE:
        g_pwmChAttrs[idx].onOff = !g_pwmChAttrs[idx].onOff;
        if (g_pwmChAttrs[idx].onOff &&
            g_pwmChAttrs[idx].onLevel != 0xFF) {
            g_pwmChAttrs[idx].currentLevel = g_pwmChAttrs[idx].onLevel;
        }
        break;

    default:
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }

    /* PWM 출력 */
    pwm_hw_apply((u8)idx);

    /* NV 저장 */
    pwm_attrs_save((u8)idx);

    return ZCL_STA_SUCCESS;
}
#endif /* ZCL_ON_OFF */

/*====================================================================
 * LevelControl 콜백
 *==================================================================*/
#ifdef ZCL_LEVEL_CTRL

#define LEVEL_MIN           1
#define LEVEL_MAX           254
#define LEVEL_STEP_MIN_MS   10

/*--------------------------------------------------------------------
 * 채널당 레벨 전환 컨텍스트 1개 — Move / MoveToLevel / Step 공용
 *   한 채널에서 동시에 두 개의 전환이 돌지 않도록 단일화
 *------------------------------------------------------------------*/
typedef enum {
    LV_MODE_NONE = 0,
    LV_MODE_MOVE,           /* 경계까지 계속 이동 (Move) */
    LV_MODE_TO_LEVEL,       /* 목표 레벨까지 이동 (MoveToLevel / Step) */
} lv_mode_t;

typedef struct {
    ev_timer_event_t *tmr;
    u8   ep_idx;
    u8   mode;          /* lv_mode_t */
    bool moveUp;        /* MOVE: TRUE=증가 */
    u8   targetLevel;   /* TO_LEVEL: 목표 (LEVEL_MIN~LEVEL_MAX) */
    bool withOnOff;
    bool offAtEnd;      /* 종료 시 OFF (WithOnOff + 목표 0 / 최소 레벨 도달) */
    u16  stepMs;
} lv_ctx_t;

static lv_ctx_t s_lvCtx[PWM_EP_COUNT];

static void level_finish(lv_ctx_t *ctx)
{
    u8 idx = ctx->ep_idx;
    if (ctx->offAtEnd) {
        g_pwmChAttrs[idx].onOff = FALSE;
        pwm_hw_apply(idx);
    }
    g_pwmChAttrs[idx].remainTime = 0;
    ctx->mode = LV_MODE_NONE;
    pwm_attrs_save(idx);
}

/* 진행 중인 전환 정지 (OnOff 명령, Stop 명령, 새 Level 명령에서 호출) */
static void level_cancel_all(u8 idx)
{
    lv_ctx_t *ctx = &s_lvCtx[idx];
    if (ctx->tmr) {
        TL_ZB_TIMER_CANCEL(&ctx->tmr);
        ctx->tmr = NULL;
        ctx->mode = LV_MODE_NONE;
        g_pwmChAttrs[idx].remainTime = 0;
        pwm_attrs_save(idx);
    }
}

static s32 level_step_cb(void *arg)
{
    lv_ctx_t *ctx = (lv_ctx_t *)arg;
    u8   idx  = ctx->ep_idx;
    u8   lv   = g_pwmChAttrs[idx].currentLevel;
    bool done = FALSE;

    if (ctx->mode == LV_MODE_MOVE) {
        if (ctx->moveUp) { if (lv < LEVEL_MAX) lv++; done = (lv >= LEVEL_MAX); }
        else             { if (lv > LEVEL_MIN) lv--; done = (lv <= LEVEL_MIN); }
    } else {
        u8 tgt = ctx->targetLevel;
        if (lv < tgt)      lv++;
        else if (lv > tgt) lv--;
        done = (lv == tgt);
    }

    g_pwmChAttrs[idx].currentLevel = lv;
    pwm_hw_apply(idx);

    if (done) {
        ctx->tmr = NULL;
        level_finish(ctx);
        return -1;
    }
    return ctx->stepMs;
}

/* Move / MoveWithOnOff — rate: steps/sec (0xFF = 기본값) */
static void level_move_start(u8 idx, bool moveUp, u8 rate, bool withOnOff)
{
    lv_ctx_t *ctx = &s_lvCtx[idx];
    u32 step_ms;

    level_cancel_all(idx);
    if (rate == 0)    return;          /* ZCL: rate 0 이면 무시 */
    if (rate == 0xFF) rate = 50;       /* 기본 이동 속도 ≈ 5초/전구간 */

    step_ms = 1000 / rate;
    if (step_ms < LEVEL_STEP_MIN_MS) step_ms = LEVEL_STEP_MIN_MS;

    ctx->ep_idx    = idx;
    ctx->mode      = LV_MODE_MOVE;
    ctx->moveUp    = moveUp;
    ctx->withOnOff = withOnOff;
    ctx->offAtEnd  = (withOnOff && !moveUp);  /* WithOnOff 하강 → 최소 도달 시 OFF */
    ctx->stepMs    = (u16)step_ms;

    if (withOnOff && moveUp && !g_pwmChAttrs[idx].onOff) {
        g_pwmChAttrs[idx].onOff = TRUE;
        pwm_hw_apply(idx);
    }

    ctx->tmr = TL_ZB_TIMER_SCHEDULE(level_step_cb, ctx, ctx->stepMs);
}

/*--------------------------------------------------------------------
 * MoveToLevel / Step 공용
 *   target          : 0~254 (WithOnOff 에서 0 = 최소 레벨까지 내린 뒤 OFF)
 *   transTime_100ms : 0 또는 0xFFFF → 즉시 (PWM 페이드가 부드럽게 처리)
 *------------------------------------------------------------------*/
static void level_move2lv(u8 idx, u8 target, u16 transTime_100ms, bool withOnOff)
{
    lv_ctx_t *ctx = &s_lvCtx[idx];
    bool off_req  = (withOnOff && target == 0);
    u8   cur, diff;
    u32  total_ms, step_ms;

    level_cancel_all(idx);

    if (target < LEVEL_MIN) target = LEVEL_MIN;
    if (target > LEVEL_MAX) target = LEVEL_MAX;

    cur  = g_pwmChAttrs[idx].currentLevel;
    diff = (target > cur) ? (target - cur) : (cur - target);

    /* WithOnOff 로 켜는 경우: 먼저 ON */
    if (withOnOff && !off_req && !g_pwmChAttrs[idx].onOff) {
        g_pwmChAttrs[idx].onOff = TRUE;
    }

    /* 꺼진 상태에서 끄기 요청 → 레벨 그대로 두고 끝 */
    if (off_req && !g_pwmChAttrs[idx].onOff) {
        pwm_hw_apply(idx);
        pwm_attrs_save(idx);
        return;
    }

    total_ms = (transTime_100ms == 0xFFFF) ? 0 : (u32)transTime_100ms * 100;

    /* 즉시 적용 (전환 시간 0 이거나 변화 없음) */
    if (total_ms == 0 || diff == 0) {
        g_pwmChAttrs[idx].currentLevel = off_req ? cur : target;
        if (off_req) g_pwmChAttrs[idx].onOff = FALSE;
        g_pwmChAttrs[idx].remainTime = 0;
        pwm_hw_apply(idx);
        pwm_attrs_save(idx);
        return;
    }

    step_ms = total_ms / diff;
    if (step_ms < LEVEL_STEP_MIN_MS) step_ms = LEVEL_STEP_MIN_MS;
    if (step_ms > 0xFFFF)            step_ms = 0xFFFF;

    ctx->ep_idx      = idx;
    ctx->mode        = LV_MODE_TO_LEVEL;
    ctx->targetLevel = target;
    ctx->withOnOff   = withOnOff;
    ctx->offAtEnd    = off_req;
    ctx->stepMs      = (u16)step_ms;
    g_pwmChAttrs[idx].remainTime = transTime_100ms;

    pwm_hw_apply(idx);
    ctx->tmr = TL_ZB_TIMER_SCHEDULE(level_step_cb, ctx, ctx->stepMs);
}

/* Step 목표 계산 — WithOnOff 하강에서 최소 이하로 내려가면 0(=OFF) 반환 */
static u8 level_step_target(u8 cur, u8 stepMode, u8 stepSize, bool withOnOff)
{
    if (stepMode == LEVEL_STEP_UP) {
        return ((u16)cur + stepSize > LEVEL_MAX) ? LEVEL_MAX : (u8)(cur + stepSize);
    }
    if ((s16)cur - stepSize < LEVEL_MIN) {
        return withOnOff ? 0 : LEVEL_MIN;
    }
    return (u8)(cur - stepSize);
}

status_t pwm_levelCtrlCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    u8 ep  = pAddrInfo->dstEp;
    s8 idx = ep_to_idx(ep);
    if (idx < 0) return ZCL_STA_FAILURE;

    DBG_LOG("[ZCL] LevelCtrl EP%d cmd=%d\r\n", (int)ep, (int)cmdId);

    switch (cmdId) {
    case ZCL_CMD_LEVEL_MOVE_TO_LEVEL:
    {
        moveToLvl_t *p = (moveToLvl_t *)cmdPayload;
        level_move2lv((u8)idx, p->level, p->transitionTime, FALSE);
        break;
    }
    case ZCL_CMD_LEVEL_MOVE_TO_LEVEL_WITH_ON_OFF:
    {
        moveToLvl_t *p = (moveToLvl_t *)cmdPayload;
        level_move2lv((u8)idx, p->level, p->transitionTime, TRUE);  /* level 0 = OFF */
        break;
    }
    case ZCL_CMD_LEVEL_MOVE:
    {
        move_t *p = (move_t *)cmdPayload;
        level_move_start((u8)idx, (p->moveMode == LEVEL_MOVE_UP), p->rate, FALSE);
        break;
    }
    case ZCL_CMD_LEVEL_MOVE_WITH_ON_OFF:
    {
        move_t *p = (move_t *)cmdPayload;
        level_move_start((u8)idx, (p->moveMode == LEVEL_MOVE_UP), p->rate, TRUE);
        break;
    }
    case ZCL_CMD_LEVEL_STEP:
    {
        step_t *p = (step_t *)cmdPayload;
        u8 lv = level_step_target(g_pwmChAttrs[idx].currentLevel, p->stepMode, p->stepSize, FALSE);
        level_move2lv((u8)idx, lv, p->transitionTime, FALSE);
        break;
    }
    case ZCL_CMD_LEVEL_STEP_WITH_ON_OFF:
    {
        step_t *p = (step_t *)cmdPayload;
        u8 lv = level_step_target(g_pwmChAttrs[idx].currentLevel, p->stepMode, p->stepSize, TRUE);
        level_move2lv((u8)idx, lv, p->transitionTime, TRUE);
        break;
    }
    case ZCL_CMD_LEVEL_STOP:
    case ZCL_CMD_LEVEL_STOP_WITH_ON_OFF:
        level_cancel_all((u8)idx);
        break;

    default:
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }

    return ZCL_STA_SUCCESS;
}
#endif /* ZCL_LEVEL_CTRL */
