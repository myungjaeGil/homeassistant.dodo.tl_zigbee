#ifndef _HEATER_MAIN_H_
#define _HEATER_MAIN_H_

/*********************************************************************
 * @file    heater_main.h
 * @brief   ZT3L 무시동히터 리모컨 — 메인 헤더
 *********************************************************************/

#include "heater_ep.h"
#include "heater_ctrl.h"

/*--------------------------------------------------------------------
 * 앱 컨텍스트
 *------------------------------------------------------------------*/
typedef struct {
    u8 keyType;
    u8 key[16];
} app_linkKey_info_t;

typedef struct {
    app_linkKey_info_t tcLinkKey;
} app_ctx_t;

extern app_ctx_t gHeaterCtx;
extern bdb_commissionSetting_t g_bdbCommissionSetting;
extern bdb_appCb_t g_zbDemoBdbCb;

/*--------------------------------------------------------------------
 * ZDO 핸들러
 *------------------------------------------------------------------*/
void heater_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf);
void heater_leaveIndHandler(nlme_leave_ind_t *pLeaveInd);
bool heater_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate);
void heater_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd);

/* 버튼 동작 완료 알림 (heater_ctrl.c → main) */
void heater_onActDone(heater_act_t act);

#endif /* _HEATER_MAIN_H_ */
