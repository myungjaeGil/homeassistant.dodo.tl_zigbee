#ifndef _EPEVER_MONITOR_H_
#define _EPEVER_MONITOR_H_

/*********************************************************************
 * @file    epeverMonitor.h
 * @brief   ZT3L EPever Tracer AN MPPT 모니터 — 메인 헤더
 *********************************************************************/

#include "epever_ep.h"

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

/*--------------------------------------------------------------------
 * 전역 변수
 *------------------------------------------------------------------*/
extern app_ctx_t gEpeverCtx;
extern bdb_commissionSetting_t g_bdbCommissionSetting;
extern bdb_appCb_t g_zbDemoBdbCb;
extern u8 g_epClusterNum[EPEVER_EP_COUNT];

/*--------------------------------------------------------------------
 * 함수 선언
 *------------------------------------------------------------------*/
void epever_zclProcessIncomingMsg(zclIncoming_t *pInHdlrMsg);

status_t epever_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t epever_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);

void epever_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf);
void epever_leaveIndHandler(nlme_leave_ind_t *pLeaveInd);
bool epever_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate);
void epever_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd);

#endif /* _EPEVER_MONITOR_H_ */
