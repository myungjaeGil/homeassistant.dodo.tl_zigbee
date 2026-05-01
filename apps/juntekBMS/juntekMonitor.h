#ifndef _JUNTEK_MONITOR_H_
#define _JUNTEK_MONITOR_H_

/*********************************************************************
 * @file    juntekMonitor.h
 * @brief   ZT3L JUNTEK JUNTEK BMS 모니터 — 메인 헤더
 *********************************************************************/

#include "juntek_ep.h"

/*--------------------------------------------------------------------
 * 타입 정의
 *------------------------------------------------------------------*/
typedef struct {
    u8 keyType;
    u8 key[16];
} app_linkKey_info_t;

typedef struct {
    ev_timer_event_t *timerLedEvt;
    u32 keyPressedTime;
    u16 ledOnTime;
    u16 ledOffTime;
    u8  oriSta;
    u8  sta;
    u8  times;
    u8  state;
    u8  keyPressed;
    bool bdbFindBindFlg;
    bool attrsChanged;
    app_linkKey_info_t tcLinkKey;
} app_ctx_t;

///* ZCL Basic/Identify/Color 호환 typedef (SDK 내부 참조용) */
//typedef struct {
//    u8 zclVersion;
//    u8 appVersion;
//    u8 stackVersion;
//    u8 hwVersion;
//    u8 manuName[ZCL_BASIC_MAX_LENGTH];
//    u8 modelId[ZCL_BASIC_MAX_LENGTH];
//    u8 swBuildId[ZCL_BASIC_MAX_LENGTH];
//    u8 powerSource;
//    u8 deviceEnable;
//} zcl_basicAttr_t;
//
//typedef struct {
//    u16 identifyTime;
//} zcl_identifyAttr_t;

/*--------------------------------------------------------------------
 * 전역 변수
 *------------------------------------------------------------------*/
extern app_ctx_t gJuntekCtx;
extern bdb_commissionSetting_t g_bdbCommissionSetting;
extern bdb_appCb_t g_zbDemoBdbCb;
extern u8 g_epClusterNum[JUNTEK_EP_COUNT];

/*--------------------------------------------------------------------
 * 함수 선언
 *------------------------------------------------------------------*/
void juntek_zclProcessIncomingMsg(zclIncoming_t *pInHdlrMsg);

status_t juntek_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t juntek_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);

void bms_otaProcessMsgHandler(u8 evt, u8 status);
void juntek_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf);
void juntek_leaveIndHandler(nlme_leave_ind_t *pLeaveInd);
bool juntek_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate);
void juntek_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd);

#if AF_TEST_ENABLE
void afTest_rx_handler(void *arg);
void afTest_dataSendConfirm(void *arg);
#endif

#endif /* _JUNTEK_MONITOR_H_ */
