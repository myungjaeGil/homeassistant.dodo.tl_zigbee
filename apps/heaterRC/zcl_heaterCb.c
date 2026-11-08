/*********************************************************************
 * @file    zcl_heaterCb.c
 * @brief   ZT3L 무시동히터 리모컨 — ZCL 콜백
 *
 *  EP1 OnOff : On → ON 버튼, Off → OFF 버튼, Toggle → 현재 상태 반대로
 *  EP2 OnOff : On / Toggle → UP 버튼 1회   (Off 는 무시)
 *  EP3 OnOff : On / Toggle → DOWN 버튼 1회 (Off 는 무시)
 *********************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "heater_ep.h"
#include "heater_ctrl.h"

/*====================================================================
 * ZCL 수신 메시지 디스패처
 *==================================================================*/
void heater_zclProcessIncomingMsg(zclIncoming_t *pInHdlrMsg)
{
    switch (pInHdlrMsg->hdr.cmd) {
#ifdef ZCL_READ
    case ZCL_CMD_READ_RSP:
        break;
#endif
#ifdef ZCL_WRITE
    case ZCL_CMD_WRITE:
    case ZCL_CMD_WRITE_NO_RSP:
        /* 펄스 설정 속성(0xF000~0xF002) 쓰기 → 즉시 보정·저장 */
        heater_cfg_check();
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
status_t heater_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    (void)pAddrInfo; (void)cmdId; (void)cmdPayload;
    return ZCL_STA_SUCCESS;
}
#endif

/*====================================================================
 * Identify 콜백 — 상태 LED 블링크
 *==================================================================*/
#ifdef ZCL_IDENTIFY
static ev_timer_event_t *s_identifyTimerEvt = NULL;

static s32 heater_identifyTimerCb(void *arg)
{
    (void)arg;
    if (g_zcl_identifyAttrs.identifyTime == 0) {
        light_blink_stop();
        s_identifyTimerEvt = NULL;
        return -1;
    }
    g_zcl_identifyAttrs.identifyTime--;
    return 1000;
}

static void heater_identifyCmdHandler(u16 identifyTime)
{
    g_zcl_identifyAttrs.identifyTime = identifyTime;
    if (identifyTime == 0) {
        if (s_identifyTimerEvt) {
            TL_ZB_TIMER_CANCEL(&s_identifyTimerEvt);
            s_identifyTimerEvt = NULL;
        }
        light_blink_stop();
    } else if (!s_identifyTimerEvt) {
        light_blink_start(0, 500, 500);     /* 무한 — identify 타이머가 종료 */
        s_identifyTimerEvt = TL_ZB_TIMER_SCHEDULE(heater_identifyTimerCb, NULL, 1000);
    }
}

static void heater_triggerEffectHandler(zcl_triggerEffect_t *pEffect)
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

status_t heater_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    if (pAddrInfo->dirCluster == ZCL_FRAME_CLIENT_SERVER_DIR) {
        switch (cmdId) {
        case ZCL_CMD_IDENTIFY:
            heater_identifyCmdHandler(((zcl_identifyCmd_t *)cmdPayload)->identifyTime);
            break;
        case ZCL_CMD_TRIGGER_EFFECT:
            heater_triggerEffectHandler((zcl_triggerEffect_t *)cmdPayload);
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
status_t heater_onOffCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    u8   ep = pAddrInfo->dstEp;
    bool ok = TRUE;
    (void)cmdPayload;

    DBG_LOG("[ZCL] OnOff EP%d cmd=%d\r\n", (int)ep, (int)cmdId);

    if (cmdId != ZCL_CMD_ONOFF_OFF && cmdId != ZCL_CMD_ONOFF_ON &&
        cmdId != ZCL_CMD_ONOFF_TOGGLE) {
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }

    switch (ep) {
    case HEATER_EP_POWER:
        if (cmdId == ZCL_CMD_ONOFF_ON)        ok = heater_power_set(TRUE);
        else if (cmdId == ZCL_CMD_ONOFF_OFF)  ok = heater_power_set(FALSE);
        else                                  ok = heater_power_toggle();
        break;

    case HEATER_EP_UP:
    case HEATER_EP_DOWN:
        /* 순간 동작 버튼 — On/Toggle 만 누름, Off 는 아무것도 하지 않음 */
        if (cmdId != ZCL_CMD_ONOFF_OFF) {
            ok = heater_step(ep == HEATER_EP_UP);
        }
        break;

    default:
        return ZCL_STA_FAILURE;
    }

    /* 대기열이 찼거나(연타) 히터가 꺼져 있어 UP/DOWN 을 무시한 경우 실패 응답 */
    return ok ? ZCL_STA_SUCCESS : ZCL_STA_FAILURE;
}
#endif /* ZCL_ON_OFF */
