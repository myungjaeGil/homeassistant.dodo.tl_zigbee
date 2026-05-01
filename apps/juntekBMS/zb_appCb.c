/*********************************************************************
 * @file    zb_appCb.c
 * @brief   Zigbee 스택 이벤트 콜백 — LED_POWER 상태 연동
 *
 * [수정] steer_retry_cb: 1회 종료 → 조인 성공까지 무한 반복
 *********************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "juntekMonitor.h"
#include "juntekCtrl.h"
#include "juntek_ep.h"
#include "factory_reset.h"

/*--------------------------------------------------------------------
 * JOIN 후 전체 EP 상태 보고 타이머
 *------------------------------------------------------------------*/
static s32 report_all_cb(void *arg)
{
//    u8 i;
//    u8 ep_list[] = {JUNTEK_ENDPOINT_ELEC,
//    		        JUNTEK_ENDPOINT_TEMP,
//    		        JUNTEK_ENDPOINT_RELAY};
//
//    for (i = 0; i < JUNTEK_EP_COUNT; i++) {
//        u8 ep  = ep_list[i];
//        u8 idx = i;
//        if (idx == 0xFF) continue;
//
//        /* broadcast로 전송 — Z2M이 수신하도록 */
//        epInfo_t dstEp;
//        TL_SETSTRUCTCONTENT(dstEp, 0);
//        dstEp.dstAddrMode       = APS_SHORT_DSTADDR_WITHEP;
//        dstEp.dstAddr.shortAddr = 0xFFFF;  /* broadcast */
//        dstEp.dstEp             = 0xFF;    /* broadcast EP */
//        dstEp.profileId         = HA_PROFILE_ID;
//        dstEp.txOptions         = 0;
//        dstEp.radius            = 0;
//
////        /* OnOff 보고 */
////        zcl_sendReportCmd(ep,
////        		         &dstEp,
////        		          FALSE,
////        		          ZCL_FRAME_SERVER_CLIENT_DIR,
////                          ZCL_CLUSTER_GEN_ON_OFF, ZCL_ATTRID_ONOFF,
////                          ZCL_DATA_TYPE_BOOLEAN, &g_zcl_onOffAttrs[idx].onOff);
////
////        /* Level 보고 */
////        zcl_sendReportCmd(ep,
////        		         &dstEp,
////        		          FALSE,
////        		          ZCL_FRAME_SERVER_CLIENT_DIR,
////                          ZCL_CLUSTER_GEN_LEVEL_CONTROL, ZCL_ATTRID_LEVEL_CURRENT_LEVEL,
////                          ZCL_DATA_TYPE_UINT8, &g_zcl_levelAttrs[idx].curLevel);
//    }
//
//
//

    printf("report_all: done");
    return -1;
}

/*--------------------------------------------------------------------
 * OTA 콜백 — 더미 (OTA 미사용)
 *------------------------------------------------------------------*/
ota_callBack_t sampleLight_otaCb;

/*--------------------------------------------------------------------
 * Network Steering 재시도 타이머
 * [수정] 조인 성공 전까지 10초마다 반복 재시도
 *------------------------------------------------------------------*/
static s32 steer_retry_cb(void *arg)
{
    if (zb_isDeviceJoinedNwk()) {
        /* 이미 조인됨 — 타이머 종료 */
        printf("BDB: already joined, stop retry\r\n");
        return -1;
    }
    printf("BDB: retry steering\r\n");
    bdb_networkSteerStart();
    return 10000;   /* 10초마다 반복 */
}

/*--------------------------------------------------------------------
 * bdbInitCb: 초기화 완료 콜백
 * 시그니처: void (*bdb_initAppCb_t)(u8 status, u8 joinedNetwork)
 *------------------------------------------------------------------*/
static void zbdemo_bdbInitCb(u8 status, u8 joinedNetwork)
{
    printf("BDB init: status=%d joined=%d factoryNew=%d\r\n",
           status, joinedNetwork, zb_isDeviceFactoryNew());
    printf("BDB channel: primary=0x%08x\r\n", g_bdbAttrs.primaryChannelSet);

    if (joinedNetwork) {
        led_power_set_state(LED_PWR_STATE_JOINED);
        /* 재시작 후 이미 JOIN된 상태 — 3초 후 전체 EP 상태 보고 */
        TL_ZB_TIMER_SCHEDULE(report_all_cb, NULL, 3000);
    } else {
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        printf("BDB: start steering\r\n");
        bdb_networkSteerStart();
    }
}

/*--------------------------------------------------------------------
 * bdbcommissioningCb: 커미셔닝 결과 콜백
 * 시그니처: void (*bdb_commissioningAppCb_t)(u8 status, void *arg)
 *------------------------------------------------------------------*/
static void zbdemo_bdbCommissioningCb(u8 status, void *arg)
{
    printf("BDB commission: status=%d joined=%d\r\n", status, zb_isDeviceJoinedNwk());

    if (status == BDB_COMMISSION_STA_SUCCESS) {
        led_power_set_state(LED_PWR_STATE_JOINED);
        printf("BDB: Joined\r\n");
        /* JOIN 후 2초 딜레이 후 전체 EP 상태 보고
         * Z2M이 interview 완료 후 상태를 수신할 수 있도록 */
        TL_ZB_TIMER_SCHEDULE(report_all_cb, NULL, 2000);
    } else if (status == BDB_COMMISSION_STA_IN_PROGRESS) {
        /* 진행 중 */
    } else {
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        printf("BDB: fail=%d, retry 10s\r\n", status);
        /* [수정] 10초 후 재시도 — 성공까지 반복 (steer_retry_cb 내부에서 반복) */
        TL_ZB_TIMER_SCHEDULE(steer_retry_cb, NULL, 10000);
    }
}

/*--------------------------------------------------------------------
 * BDB 콜백 구조체
 *------------------------------------------------------------------*/
bdb_appCb_t g_zbDemoBdbCb = {
    zbdemo_bdbInitCb,           /* bdbInitCb */
    zbdemo_bdbCommissioningCb,  /* bdbcommissioningCb */
    NULL,                       /* bdbIdentifyCb */
    NULL,                       /* bdbFindBindSuccessCb */
};

/*--------------------------------------------------------------------
 * ZDO 네트워크 이벤트 콜백
 *------------------------------------------------------------------*/
void juntek_leaveIndHandler(nlme_leave_ind_t *pLeaveInd)
{
    led_power_set_state(LED_PWR_STATE_NOT_JOINED);
    printf("NWK: leave\r\n");

    /* [수정] leave 후 자동 재조인 시도 */
    TL_ZB_TIMER_SCHEDULE(steer_retry_cb, NULL, 3000);
}

void juntek_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf)
{
    printf("NWK: leave cnf\r\n");
}

bool juntek_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate)
{
    return FALSE;
}

void juntek_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd)
{
    if (pNwkStatusInd) {
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
    }
}

/*--------------------------------------------------------------------
 * Rejoin 백오프
 *------------------------------------------------------------------*/
void juntek_rejoinBackoff(void)
{
    if (!zb_isDeviceFactoryNew()) {
        zb_rejoinSecModeSet(0);
        zb_rejoinReq(zb_apsChannelMaskGet(), 0);
    }
}

/*--------------------------------------------------------------------
 * Factory Reset LED
 *------------------------------------------------------------------*/
void zbdemo_factoryResetCb(void)
{
    led_power_set_state(LED_PWR_STATE_RESET);
}
