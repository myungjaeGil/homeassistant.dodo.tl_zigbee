/*********************************************************************
 * @file    zcl_epeverCb.c
 * @brief   ZT3L EPever MPPT 모니터 — ZCL 콜백
 *********************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "ota.h"
#include "epeverMonitor.h"
#include "epever_ep.h"
#include "epeverCtrl.h"

#ifdef ZCL_READ
static void epever_zclReadRspCmd(zclReadRspCmd_t *pReadRspCmd);
#endif
#ifdef ZCL_WRITE
static void epever_zclWriteReqCmd(u8 ep, u16 clusterId, zclWriteCmd_t *pWriteReqCmd);
static void epever_zclWriteRspCmd(zclWriteRspCmd_t *pWriteRspCmd);
#endif
#ifdef ZCL_REPORT
static void epever_zclCfgReportCmd(zclCfgReportCmd_t *pCfgReportCmd);
static void epever_zclCfgReportRspCmd(zclCfgReportRspCmd_t *pCfgReportRspCmd);
static void epever_zclReportCmd(zclReportCmd_t *pReportCmd);
#endif
static void epever_zclDfltRspCmd(zclDefaultRspCmd_t *pDftRspCmd);

#ifdef ZCL_IDENTIFY
static ev_timer_event_t *identifyTimerEvt = NULL;
#endif

void epever_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 identifyTime);

/*********************************************************************
 * @fn      epever_zclProcessIncomingMsg
 *********************************************************************/
void epever_zclProcessIncomingMsg(zclIncoming_t *pInHdlrMsg)
{
    switch (pInHdlrMsg->hdr.cmd) {
#ifdef ZCL_READ
    case ZCL_CMD_READ_RSP:
        epever_zclReadRspCmd(pInHdlrMsg->attrCmd);
        break;
#endif
#ifdef ZCL_WRITE
    case ZCL_CMD_WRITE:
    case ZCL_CMD_WRITE_NO_RSP:
        epever_zclWriteReqCmd(pInHdlrMsg->msg->indInfo.dst_ep,
                               pInHdlrMsg->msg->indInfo.cluster_id,
                               pInHdlrMsg->attrCmd);
        break;
    case ZCL_CMD_WRITE_RSP:
        epever_zclWriteRspCmd(pInHdlrMsg->attrCmd);
        break;
#endif
#ifdef ZCL_REPORT
    case ZCL_CMD_CONFIG_REPORT:
        epever_zclCfgReportCmd(pInHdlrMsg->attrCmd);
        break;
    case ZCL_CMD_CONFIG_REPORT_RSP:
        epever_zclCfgReportRspCmd(pInHdlrMsg->attrCmd);
        break;
    case ZCL_CMD_REPORT:
        epever_zclReportCmd(pInHdlrMsg->attrCmd);
        break;
#endif
    case ZCL_CMD_DEFAULT_RSP:
        epever_zclDfltRspCmd(pInHdlrMsg->attrCmd);
        break;
    default:
        break;
    }
}

#ifdef ZCL_READ
static void epever_zclReadRspCmd(zclReadRspCmd_t *pReadRspCmd)
{
    (void)pReadRspCmd;
}
#endif

#ifdef ZCL_WRITE
static void epever_zclWriteReqCmd(u8 ep, u16 clusterId, zclWriteCmd_t *pWriteReqCmd)
{
    (void)ep; (void)clusterId; (void)pWriteReqCmd;
}

static void epever_zclWriteRspCmd(zclWriteRspCmd_t *pWriteRspCmd)
{
    (void)pWriteRspCmd;
}
#endif

static void epever_zclDfltRspCmd(zclDefaultRspCmd_t *pDftRspCmd)
{
    (void)pDftRspCmd;
}

#ifdef ZCL_REPORT
static void epever_zclCfgReportCmd(zclCfgReportCmd_t *pCfgReportCmd)
{
    (void)pCfgReportCmd;
}
static void epever_zclCfgReportRspCmd(zclCfgReportRspCmd_t *pCfgReportRspCmd)
{
    (void)pCfgReportRspCmd;
}
static void epever_zclReportCmd(zclReportCmd_t *pReportCmd)
{
    (void)pReportCmd;
}
#endif

#ifdef ZCL_BASIC
status_t epever_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    (void)pAddrInfo; (void)cmdPayload;
    if (cmdId == ZCL_CMD_BASIC_RESET_FAC_DEFAULT) {
        /* Factory reset */
    }
    return ZCL_STA_UNSUP_CLUSTER_COMMAND;
}
#endif

#ifdef ZCL_IDENTIFY
s32 epever_zclIdentifyTimerCb(void *arg)
{
    (void)arg;
    if (g_zcl_identifyAttrs.identifyTime <= 0) {
        light_blink_stop();
        identifyTimerEvt = NULL;
        return -1;
    }
    g_zcl_identifyAttrs.identifyTime--;
    return 0;
}

void epever_zclIdentifyTimerStop(void)
{
    if (identifyTimerEvt) {
        TL_ZB_TIMER_CANCEL(&identifyTimerEvt);
    }
}

void epever_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 identifyTime)
{
    (void)endpoint; (void)srcAddr;
    g_zcl_identifyAttrs.identifyTime = identifyTime;

    if (identifyTime == 0) {
        epever_zclIdentifyTimerStop();
        light_blink_stop();
    } else {
        if (!identifyTimerEvt) {
            light_blink_start(identifyTime, 500, 500);
            identifyTimerEvt = TL_ZB_TIMER_SCHEDULE(epever_zclIdentifyTimerCb,
                                                     NULL, 1000);
        }
    }
}

static void epever_zcltriggerCmdHandler(zcl_triggerEffect_t *pTriggerEffect)
{
    switch (pTriggerEffect->effectId) {
    case IDENTIFY_EFFECT_BLINK:          light_blink_start(1,  500, 500);  break;
    case IDENTIFY_EFFECT_BREATHE:        light_blink_start(15, 300, 700);  break;
    case IDENTIFY_EFFECT_OKAY:           light_blink_start(2,  250, 250);  break;
    case IDENTIFY_EFFECT_CHANNEL_CHANGE: light_blink_start(1,  500, 7500); break;
    case IDENTIFY_EFFECT_FINISH_EFFECT:  light_blink_start(1,  300, 700);  break;
    case IDENTIFY_EFFECT_STOP_EFFECT:    light_blink_stop();                break;
    default: break;
    }
}

status_t epever_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    if (pAddrInfo->dirCluster == ZCL_FRAME_CLIENT_SERVER_DIR) {
        switch (cmdId) {
        case ZCL_CMD_IDENTIFY:
            epever_zclIdentifyCmdHandler(pAddrInfo->dstEp, pAddrInfo->srcAddr,
                                          ((zcl_identifyCmd_t*)cmdPayload)->identifyTime);
            break;
        case ZCL_CMD_TRIGGER_EFFECT:
            epever_zcltriggerCmdHandler((zcl_triggerEffect_t*)cmdPayload);
            break;
        default:
            break;
        }
    }
    return ZCL_STA_SUCCESS;
}
#endif
