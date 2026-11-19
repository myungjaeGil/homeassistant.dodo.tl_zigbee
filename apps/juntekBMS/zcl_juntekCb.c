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

/* 소프트 리셋 — Telink 8258 레지스터 직접 접근
 * cpu_reboot() / SOFT_REBOOT() 심볼이 SDK 라이브러리에 따라 다를 수 있으므로
 * 레지스터를 직접 조작하는 방식으로 구현 */
#define SOFT_REBOOT()  do { \
    REG_ADDR8(0x6f) = 0x20; \
} while(0)

/* soft_reset 타이머 콜백 — forward declaration */
static s32 soft_reset_timer_cb(void *arg);

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
    u8 numAttr = pWriteReqCmd->numAttr;
    zclWriteRec_t *attr = pWriteReqCmd->attrList;

    (void)ep;

    /* [수정] 필터 설정(0xFF00 클러스터) — 전역 Write 는 클러스터 콜백(filter_cfg_writeCb)을
     * 부르지 않으므로 기존에는 NV 에 저장되지 않아 재부팅하면 기본값으로 돌아갔음.
     * 스택이 속성을 쓴 뒤 이 경로가 호출되므로 여기서 값 변화를 확인해 저장 */
    if (clusterId == ZCL_CLUSTER_JUNTEK_FILTER) {
        juntek_filter_cfg_check();
        return;
    }

    if (clusterId == ZCL_CLUSTER_GEN_BASIC) {
        for (u8 i = 0; i < numAttr; i++) {
            if (attr[i].attrID == ZCL_ATTRID_SOFT_RESET) {
                /* 0xFF00 에 1 쓰면 소프트 리셋
                 * Write 응답 전송 후 200ms 뒤 reboot */
                if (attr[i].attrData && attr[i].attrData[0] == 0x01) {
                    DBG_LOG("ZCL: soft reset requested\r\n");
                    light_blink_start(3, 100, 100);
                    TL_ZB_TIMER_SCHEDULE(soft_reset_timer_cb, NULL, 700);
                }
            }
        }
    }
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

/*====================================================================
 * OTA 콜백 구현
 *  ZCL_OTA_SUPPORT 1 로 활성화됨. OTA 진행 중 LED 빠른 점멸.
 *
 * [수정] 완료(성공) 시 여기서 바로 SOFT_REBOOT 하던 것을 제거.
 *  이 콜백은 이미지 검증 직후, Upgrade End Request 를 보내기 전에 호출됨.
 *  SDK 는 서버의 Upgrade End Response 를 받은 뒤 ota_mcuReboot() 에서
 *  새 이미지 부팅 플래그를 쓰고 재부팅하는데, 여기서 먼저 재부팅하면
 *  플래그가 안 써져 이전 펌웨어로 다시 부팅됨(Z2M 에는 타임아웃/실패로 보임).
 *  재부팅은 SDK 에 맡긴다.
 *==================================================================*/
#if ZCL_OTA_SUPPORT
ota_callBack_t juntek_otaCb = {
    .processMsgCbFunc = bms_otaProcessMsgHandler,
};

void bms_otaProcessMsgHandler(u8 evt, u8 status)
{
    if (evt == OTA_EVT_START) {
        /* OTA 시작: LED 빠른 깜빡임
         * [수정] 0xFF 는 255회(약 51초) 뒤 멈춤 → 0 = 정지할 때까지 무한 */
        DBG_LOG("[OTA] Start\r\n");
        light_blink_start(0, 100, 100);
    } else if (evt == OTA_EVT_COMPLETE) {
        DBG_LOG("[OTA] Complete status=%d\r\n", (int)status);
        /* 성공이면 SDK 가 Upgrade End Response 후 재부팅함 */
        light_blink_stop();
    }
}
#endif

#ifdef ZCL_BASIC
/*--------------------------------------------------------------------
 * soft_reset 타이머 콜백 — Write 응답 전송 후 reboot
 *------------------------------------------------------------------*/
static s32 soft_reset_timer_cb(void *arg)
{
    (void)arg;
    SOFT_REBOOT();
    return -1;
}

status_t juntek_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    if (cmdId == ZCL_CMD_BASIC_RESET_FAC_DEFAULT) {
        /* Factory Reset — 기존 동작 없음 */
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
            /* [수정] 횟수 대신 0(무한) — 종료는 identify 타이머가 light_blink_stop() 으로 처리
             * (PWM5CH 와 동일: identifyTime 이 255 를 넘으면 u8 로 잘리던 문제도 해결) */
            light_blink_start(0, 500, 500);
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
