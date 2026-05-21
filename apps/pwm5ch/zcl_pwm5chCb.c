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
        }
        light_blink_stop();
    } else {
        if (!s_identifyTimerEvt) {
            light_blink_start(identifyTime, 500, 500);
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

    printf("[ZCL] OnOff EP%d cmd=%d\r\n", (int)ep, (int)cmdId);

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

/* Move 타이머 상태 — 채널당 1개 */
typedef struct {
    ev_timer_event_t *tmr;
    u8   ep_idx;
    bool moveUp;    /* TRUE=증가 */
    u8   rate;      /* steps/sec */
    s8   stepSize;  /* MoveWithOnOff: 자동 계산 */
    u16  stepMs;    /* 스텝 간격 ms */
    u16  totalMs;   /* 전체 남은 ms */
    bool withOnOff;
} move_ctx_t;

static move_ctx_t s_moveCtx[PWM_EP_COUNT];

/* 스텝 타이머 콜백 */
static s32 level_move_cb(void *arg)
{
    move_ctx_t *ctx = (move_ctx_t *)arg;
    u8   idx = ctx->ep_idx;
    u8   lv  = g_pwmChAttrs[idx].currentLevel;
    bool changed = FALSE;

    if (ctx->moveUp) {
        if (lv < 254) {
            lv++;
            changed = TRUE;
        }
    } else {
        if (lv > 1) {
            lv--;
            changed = TRUE;
        }
    }

    if (changed) {
        g_pwmChAttrs[idx].currentLevel = lv;
        if (ctx->withOnOff && lv > 0) {
            g_pwmChAttrs[idx].onOff = TRUE;
        }
        pwm_hw_apply(idx);
    }

    /* 경계에 도달 */
    if ((!ctx->moveUp && lv <= 1) || (ctx->moveUp && lv >= 254)) {
        if (ctx->withOnOff && !ctx->moveUp) {
            g_pwmChAttrs[idx].onOff = FALSE;
            pwm_hw_apply(idx);
        }
        pwm_attrs_save(idx);
        ctx->tmr = NULL;
        return -1;
    }

    return ctx->stepMs;
}

static void level_move_stop(u8 idx)
{
    if (s_moveCtx[idx].tmr) {
        TL_ZB_TIMER_CANCEL(&s_moveCtx[idx].tmr);
        s_moveCtx[idx].tmr = NULL;
        pwm_attrs_save(idx);
    }
}

static void level_move_start(u8 idx, bool moveUp, u8 rate, bool withOnOff)
{
    level_move_stop(idx);
    if (rate == 0) rate = 1;

    s_moveCtx[idx].ep_idx    = idx;
    s_moveCtx[idx].moveUp    = moveUp;
    s_moveCtx[idx].rate      = rate;
    s_moveCtx[idx].stepMs    = 1000 / rate;
    s_moveCtx[idx].withOnOff = withOnOff;

    if (withOnOff && moveUp) {
        g_pwmChAttrs[idx].onOff = TRUE;
        pwm_hw_apply(idx);
    }

    s_moveCtx[idx].tmr = TL_ZB_TIMER_SCHEDULE(level_move_cb,
                                               &s_moveCtx[idx],
                                               s_moveCtx[idx].stepMs);
}

/* MoveToLevel 타이머 */
typedef struct {
    ev_timer_event_t *tmr;
    u8   ep_idx;
    u8   targetLevel;
    bool withOnOff;
    u16  stepMs;
} move2lv_ctx_t;

static move2lv_ctx_t s_m2lvCtx[PWM_EP_COUNT];

static s32 level_move2lv_cb(void *arg)
{
    move2lv_ctx_t *ctx = (move2lv_ctx_t *)arg;
    u8 idx = ctx->ep_idx;
    u8 lv  = g_pwmChAttrs[idx].currentLevel;
    u8 tgt = ctx->targetLevel;

    if (lv < tgt)      lv++;
    else if (lv > tgt) lv--;

    g_pwmChAttrs[idx].currentLevel = lv;

    if (ctx->withOnOff) {
        g_pwmChAttrs[idx].onOff = (lv > 0);
    }
    pwm_hw_apply(idx);

    if (lv == tgt) {
        pwm_attrs_save(idx);
        ctx->tmr = NULL;
        return -1;
    }
    return ctx->stepMs;
}

static void level_move2lv(u8 idx, u8 target, u16 transTime_100ms, bool withOnOff)
{
    if (s_m2lvCtx[idx].tmr) {
        TL_ZB_TIMER_CANCEL(&s_m2lvCtx[idx].tmr);
        s_m2lvCtx[idx].tmr = NULL;
    }

    u8 cur = g_pwmChAttrs[idx].currentLevel;
    u8 diff = (target > cur) ? (target - cur) : (cur - target);
    if (diff == 0) {
        if (withOnOff) {
            g_pwmChAttrs[idx].onOff = (target > 0);
            pwm_hw_apply(idx);
            pwm_attrs_save(idx);
        }
        return;
    }

    /* transTime_100ms 단위를 ms로 변환 → 스텝 간격 */
    u32 total_ms = (u32)transTime_100ms * 100;
    u16 step_ms  = (total_ms == 0) ? 10 : (u16)(total_ms / diff);
    if (step_ms < 10) step_ms = 10;

    if (withOnOff && target > 0) {
        g_pwmChAttrs[idx].onOff = TRUE;
        pwm_hw_apply(idx);
    }

    s_m2lvCtx[idx].ep_idx      = idx;
    s_m2lvCtx[idx].targetLevel = target;
    s_m2lvCtx[idx].withOnOff   = withOnOff;
    s_m2lvCtx[idx].stepMs      = step_ms;
    s_m2lvCtx[idx].tmr = TL_ZB_TIMER_SCHEDULE(level_move2lv_cb,
                                               &s_m2lvCtx[idx], step_ms);
}

status_t pwm_levelCtrlCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    u8 ep  = pAddrInfo->dstEp;
    s8 idx = ep_to_idx(ep);
    if (idx < 0) return ZCL_STA_FAILURE;

    printf("[ZCL] LevelCtrl EP%d cmd=%d\r\n", (int)ep, (int)cmdId);

    switch (cmdId) {
    case ZCL_CMD_LEVEL_MOVE_TO_LEVEL:
    {
        moveToLvl_t *p = (moveToLvl_t *)cmdPayload;
        u8 lv = p->level;
        if (lv < 1)   lv = 1;
        if (lv > 254) lv = 254;
        level_move2lv((u8)idx, lv, p->transitionTime, FALSE);
        break;
    }
    case ZCL_CMD_LEVEL_MOVE_TO_LEVEL_WITH_ON_OFF:
    {
        moveToLvl_t *p = (moveToLvl_t *)cmdPayload;
        u8 lv = p->level;
        if (lv < 1)   lv = 1;
        if (lv > 254) lv = 254;
        level_move2lv((u8)idx, lv, p->transitionTime, TRUE);
        break;
    }
    case ZCL_CMD_LEVEL_MOVE:
    {
        move_t *p = (move_t *)cmdPayload;
        bool up = (p->moveMode == LEVEL_MOVE_UP);
        level_move_start((u8)idx, up, p->rate, FALSE);
        break;
    }
    case ZCL_CMD_LEVEL_MOVE_WITH_ON_OFF:
    {
        move_t *p = (move_t *)cmdPayload;
        bool up = (p->moveMode == LEVEL_MOVE_UP);
        level_move_start((u8)idx, up, p->rate, TRUE);
        break;
    }
    case ZCL_CMD_LEVEL_STEP:
    {
        step_t *p = (step_t *)cmdPayload;
        u8 lv = g_pwmChAttrs[idx].currentLevel;
        if (p->stepMode == LEVEL_STEP_UP) {
            lv = (lv + p->stepSize > 254) ? 254 : (lv + p->stepSize);
        } else {
            lv = (lv < p->stepSize + 1) ? 1 : (lv - p->stepSize);
        }
        level_move2lv((u8)idx, lv, p->transitionTime, FALSE);
        break;
    }
    case ZCL_CMD_LEVEL_STEP_WITH_ON_OFF:
    {
        step_t *p = (step_t *)cmdPayload;
        u8 lv = g_pwmChAttrs[idx].currentLevel;
        if (p->stepMode == LEVEL_STEP_UP) {
            lv = (lv + p->stepSize > 254) ? 254 : (lv + p->stepSize);
        } else {
            lv = (lv < p->stepSize + 1) ? 1 : (lv - p->stepSize);
        }
        level_move2lv((u8)idx, lv, p->transitionTime, TRUE);
        break;
    }
    case ZCL_CMD_LEVEL_STOP:
    case ZCL_CMD_LEVEL_STOP_WITH_ON_OFF:
        level_move_stop((u8)idx);
        break;

    default:
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }

    return ZCL_STA_SUCCESS;
}
#endif /* ZCL_LEVEL_CTRL */
