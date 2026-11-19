#ifndef _XMAS_MAIN_H_
#define _XMAS_MAIN_H_

/*********************************************************************
 * @file    xmas_main.h
 * @brief   ZT3L 크리스마스 트리 3CH 전등 — 메인 헤더
 *********************************************************************/

#include "xmas_ep.h"

/*--------------------------------------------------------------------
 * 앱 컨텍스트
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
extern app_ctx_t gPwmCtx;
extern bdb_commissionSetting_t g_bdbCommissionSetting;
extern bdb_appCb_t g_zbDemoBdbCb;

/*--------------------------------------------------------------------
 * 함수 선언
 *------------------------------------------------------------------*/
void pwm_zclProcessIncomingMsg(zclIncoming_t *pInHdlrMsg);

status_t pwm_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t pwm_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t pwm_onOffCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t pwm_levelCtrlCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);

void pwm_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf);
void pwm_leaveIndHandler(nlme_leave_ind_t *pLeaveInd);
bool pwm_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate);
void pwm_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd);

/* NV 저장 (xmas_epCfg.c 에서 정의) */
void pwm_attrs_save(u8 ep_idx);

#endif /* _XMAS_MAIN_H_ */
