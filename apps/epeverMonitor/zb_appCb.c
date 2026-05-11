/*********************************************************************
 * @file    zb_appCb.c
 * @brief   Zigbee 스택 이벤트 콜백 — EPever MPPT 버전
 *********************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "epeverMonitor.h"
#include "epeverCtrl.h"
#include "epever_ep.h"
#include "factory_reset.h"

/*--------------------------------------------------------------------
 * OTA 콜백 — 더미
 *------------------------------------------------------------------*/
ota_callBack_t sampleLight_otaCb;

/*--------------------------------------------------------------------
 * Network Steering 재시도 타이머
 *------------------------------------------------------------------*/
static s32 steer_retry_cb(void *arg)
{
    (void)arg;
    if (zb_isDeviceJoinedNwk()) {
        return -1;
    }
    bdb_networkSteerStart();
    return 10000;
}

/*--------------------------------------------------------------------
 * bdbInitCb
 *------------------------------------------------------------------*/
static void zbdemo_bdbInitCb(u8 status, u8 joinedNetwork)
{
    if (joinedNetwork) {
        led_power_set_state(LED_PWR_STATE_JOINED);
    } else {
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        bdb_networkSteerStart();
    }
}

/*--------------------------------------------------------------------
 * bdbcommissioningCb
 *------------------------------------------------------------------*/
static void zbdemo_bdbCommissioningCb(u8 status, void *arg)
{
    (void)arg;
    if (status == BDB_COMMISSION_STA_SUCCESS) {
        led_power_set_state(LED_PWR_STATE_JOINED);
    } else if (status == BDB_COMMISSION_STA_IN_PROGRESS) {
        /* 진행 중 */
    } else {
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        TL_ZB_TIMER_SCHEDULE(steer_retry_cb, NULL, 10000);
    }
}

/*--------------------------------------------------------------------
 * BDB 콜백 구조체
 *------------------------------------------------------------------*/
bdb_appCb_t g_zbDemoBdbCb = {
    zbdemo_bdbInitCb,
    zbdemo_bdbCommissioningCb,
    NULL,
    NULL,
};

/*--------------------------------------------------------------------
 * ZDO 네트워크 이벤트 콜백
 *------------------------------------------------------------------*/
void epever_leaveIndHandler(nlme_leave_ind_t *pLeaveInd)
{
    (void)pLeaveInd;
    led_power_set_state(LED_PWR_STATE_NOT_JOINED);
    TL_ZB_TIMER_SCHEDULE(steer_retry_cb, NULL, 3000);
}

void epever_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf)
{
    (void)pLeaveCnf;
}

bool epever_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate)
{
    (void)pNwkUpdate;
    return FALSE;
}

void epever_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd)
{
    if (pNwkStatusInd) {
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
    }
}

/*--------------------------------------------------------------------
 * Factory Reset LED
 *------------------------------------------------------------------*/
void zbdemo_factoryResetCb(void)
{
    led_power_set_state(LED_PWR_STATE_RESET);
}
