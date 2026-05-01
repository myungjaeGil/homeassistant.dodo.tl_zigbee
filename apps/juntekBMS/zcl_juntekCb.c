/********************************************************************************************************
 * @file    zcl_juntekCb.c
 *
 * @brief   This is the source file for zcl_juntekCb
 *
 * @author  Zigbee Group
 * @date    2021
 *
 * @par     Modified for ZT3L: EP-aware save, colorCtrl conditional
 *
 *******************************************************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "ota.h"
#include "juntekMonitor.h"
#include "juntek_ep.h"
#include "juntekCtrl.h"

#ifdef ZCL_READ
static void juntek_zclReadRspCmd(zclReadRspCmd_t *pReadRspCmd);
#endif
#ifdef ZCL_WRITE
static void juntek_zclWriteReqCmd(u8 ep, u16 clusterId, zclWriteCmd_t *pWriteReqCmd);
static void juntek_zclWriteRspCmd(zclWriteRspCmd_t *pWriteRspCmd);
#endif
#ifdef ZCL_REPORT
static void juntek_zclCfgReportCmd(zclCfgReportCmd_t *pCfgReportCmd);
static void juntek_zclCfgReportRspCmd(zclCfgReportRspCmd_t *pCfgReportRspCmd);
static void juntek_zclReportCmd(zclReportCmd_t *pReportCmd);
#endif
static void juntek_zclDfltRspCmd(zclDefaultRspCmd_t *pDftRspCmd);

#ifdef ZCL_IDENTIFY
static ev_timer_event_t *identifyTimerEvt = NULL;
#endif

void juntek_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 identifyTime);

/*********************************************************************
 * @fn      juntek_zclProcessIncomingMsg
 *********************************************************************/
void juntek_zclProcessIncomingMsg(zclIncoming_t *pInHdlrMsg)
{
    switch (pInHdlrMsg->hdr.cmd) {
#ifdef ZCL_READ
    case ZCL_CMD_READ_RSP:
        juntek_zclReadRspCmd(pInHdlrMsg->attrCmd);
        break;
#endif
#ifdef ZCL_WRITE
    case ZCL_CMD_WRITE:
    case ZCL_CMD_WRITE_NO_RSP:
        juntek_zclWriteReqCmd(pInHdlrMsg->msg->indInfo.dst_ep,
                                   pInHdlrMsg->msg->indInfo.cluster_id,
                                   pInHdlrMsg->attrCmd);
        break;
    case ZCL_CMD_WRITE_RSP:
        juntek_zclWriteRspCmd(pInHdlrMsg->attrCmd);
        break;
#endif
#ifdef ZCL_REPORT
    case ZCL_CMD_CONFIG_REPORT:
        juntek_zclCfgReportCmd(pInHdlrMsg->attrCmd);
        break;
    case ZCL_CMD_CONFIG_REPORT_RSP:
        juntek_zclCfgReportRspCmd(pInHdlrMsg->attrCmd);
        break;
    case ZCL_CMD_REPORT:
        juntek_zclReportCmd(pInHdlrMsg->attrCmd);
        break;
#endif
    case ZCL_CMD_DEFAULT_RSP:
        juntek_zclDfltRspCmd(pInHdlrMsg->attrCmd);
        break;
    default:
        break;
    }
}

#ifdef ZCL_READ
static void juntek_zclReadRspCmd(zclReadRspCmd_t *pReadRspCmd)
{
}
#endif

#ifdef ZCL_WRITE
/*********************************************************************
 * @fn      juntek_zclWriteReqCmd
 * @note    ep 인자 추가 — EP별 NV 저장
 *********************************************************************/
static void juntek_zclWriteReqCmd(u8 ep, u16 clusterId, zclWriteCmd_t *pWriteReqCmd)
{
//    u8 numAttr = pWriteReqCmd->numAttr;
//    zclWriteRec_t *attr = pWriteReqCmd->attrList;
//
//    if (clusterId == ZCL_CLUSTER_GEN_ON_OFF) {
//        for (u8 i = 0; i < numAttr; i++) {
//            if (attr[i].attrID == ZCL_ATTRID_START_UP_ONOFF) {
//                zcl_onOffAttr_save(ep);
//            }
//        }
//    } else if (clusterId == ZCL_CLUSTER_GEN_LEVEL_CONTROL) {
//        for (u8 i = 0; i < numAttr; i++) {
//            if (attr[i].attrID == ZCL_ATTRID_LEVEL_START_UP_CURRENT_LEVEL) {
//                zcl_levelAttr_save(ep);
//            }
//        }
//    } else if (clusterId == ZCL_CLUSTER_LIGHTING_COLOR_CONTROL) {
//#ifdef ZCL_LIGHT_COLOR_CONTROL
//        for (u8 i = 0; i < numAttr; i++) {
//            if (attr[i].attrID == ZCL_ATTRID_START_UP_COLOR_TEMPERATURE_MIREDS) {
//                zcl_colorCtrlAttr_save();
//            }
//        }
//#endif
//    } else if (clusterId == ZCL_CLUSTER_GEN_IDENTIFY) {
//        for (u8 i = 0; i < numAttr; i++) {
//            if (attr[i].attrID == ZCL_ATTRID_IDENTIFY_TIME) {
//                juntek_zclIdentifyCmdHandler(ep, 0xFFFE,
//                                                  g_zcl_identifyAttrs.identifyTime);
//            }
//        }
//    }
}

static void juntek_zclWriteRspCmd(zclWriteRspCmd_t *pWriteRspCmd)
{
}
#endif

static void juntek_zclDfltRspCmd(zclDefaultRspCmd_t *pDftRspCmd)
{
}

#ifdef ZCL_REPORT
static void juntek_zclCfgReportCmd(zclCfgReportCmd_t *pCfgReportCmd)
{
}
static void juntek_zclCfgReportRspCmd(zclCfgReportRspCmd_t *pCfgReportRspCmd)
{
}
static void juntek_zclReportCmd(zclReportCmd_t *pReportCmd)
{
}
#endif

#ifdef ZCL_BASIC
status_t juntek_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    if (cmdId == ZCL_CMD_BASIC_RESET_FAC_DEFAULT) {
        /* Reset all attributes to factory defaults */
    }
    return ZCL_STA_UNSUP_CLUSTER_COMMAND;
}
#endif

#ifdef ZCL_IDENTIFY
s32 juntek_zclIdentifyTimerCb(void *arg)
{
    if (g_zcl_identifyAttrs.identifyTime <= 0) {
        light_blink_stop();
        identifyTimerEvt = NULL;
        return -1;
    }
    g_zcl_identifyAttrs.identifyTime--;
    return 0;
}

void juntek_zclIdentifyTimerStop(void)
{
    if (identifyTimerEvt) {
        TL_ZB_TIMER_CANCEL(&identifyTimerEvt);
    }
}

void juntek_zclIdentifyCmdHandler(u8 endpoint, u16 srcAddr, u16 identifyTime)
{
    g_zcl_identifyAttrs.identifyTime = identifyTime;

    if (identifyTime == 0) {
        juntek_zclIdentifyTimerStop();
        light_blink_stop();
    } else {
        if (!identifyTimerEvt) {
            light_blink_start(identifyTime, 500, 500);
            identifyTimerEvt = TL_ZB_TIMER_SCHEDULE(juntek_zclIdentifyTimerCb,
                                                     NULL, 1000);
        }
    }
}

static void juntek_zcltriggerCmdHandler(zcl_triggerEffect_t *pTriggerEffect)
{
    switch (pTriggerEffect->effectId) {
    case IDENTIFY_EFFECT_BLINK:           light_blink_start(1,  500, 500);  break;
    case IDENTIFY_EFFECT_BREATHE:         light_blink_start(15, 300, 700);  break;
    case IDENTIFY_EFFECT_OKAY:            light_blink_start(2,  250, 250);  break;
    case IDENTIFY_EFFECT_CHANNEL_CHANGE:  light_blink_start(1,  500, 7500); break;
    case IDENTIFY_EFFECT_FINISH_EFFECT:   light_blink_start(1,  300, 700);  break;
    case IDENTIFY_EFFECT_STOP_EFFECT:     light_blink_stop();               break;
    default: break;
    }
}

status_t juntek_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    if (pAddrInfo->dirCluster == ZCL_FRAME_CLIENT_SERVER_DIR) {
        switch (cmdId) {
        case ZCL_CMD_IDENTIFY:
            juntek_zclIdentifyCmdHandler(pAddrInfo->dstEp, pAddrInfo->srcAddr,
                                              ((zcl_identifyCmd_t*)cmdPayload)->identifyTime);
            break;
        case ZCL_CMD_TRIGGER_EFFECT:
            juntek_zcltriggerCmdHandler((zcl_triggerEffect_t*)cmdPayload);
            break;
        default:
            break;
        }
    }
    return ZCL_STA_SUCCESS;
}
#endif
