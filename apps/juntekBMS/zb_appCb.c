/*********************************************************************
 * @file    zb_appCb.c
 * @brief   Zigbee 스택 이벤트 콜백 — LED_POWER 상태 연동
 *
 * [수정] PWM5CH/EPever 와 공통 수정 적용
 *  - steering 재시도 타이머를 1개 핸들로 관리 (실패할 때마다 타이머가 늘어나던 문제)
 *  - LeaveInd 를 받으면 500ms 뒤 자기 자신이 실제로 빠졌는지 확인
 *    (라우터는 자식 노드가 나갈 때도 LeaveInd 를 받음)
 *  - LeaveCnf(버튼 공장초기화) 후 미가입 표시 + steering 재시작
 *  - 가입되면 OTA 서버 주기 조회 시작 (기존: ota_init 만 하고 조회를 시작하지 않음)
 *  - printf → DBG_LOG
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

#ifndef OTA_PERIODIC_QUERY_INTERVAL
#define OTA_PERIODIC_QUERY_INTERVAL     (15 * 60)   /* 초 — OTA 서버 조회 주기 */
#endif

/*--------------------------------------------------------------------
 * 가입 완료 시 공통 처리
 *------------------------------------------------------------------*/
/* [추가] 가입/재부팅 복귀 후 EP6/EP7 현재 상태 보고
 *  NV 에서 복원된 릴레이 상태를 HA 에 바로 알림 (기존: 최대 보고 주기 300초 동안
 *  HA 가 재부팅 전 상태를 표시). 라우팅이 안정되도록 2초 뒤 1회 전송 */
static ev_timer_event_t *s_relayReportEvt = NULL;

static s32 relay_report_cb(void *arg)
{
    (void)arg;
    s_relayReportEvt = NULL;
    juntek_relay_report_all();
    return -1;
}

static void juntek_onJoined(void)
{
    led_power_set_state(LED_PWR_STATE_JOINED);

    if (s_relayReportEvt) {
        TL_ZB_TIMER_CANCEL(&s_relayReportEvt);
    }
    s_relayReportEvt = TL_ZB_TIMER_SCHEDULE(relay_report_cb, NULL, 2000);
#if ZCL_OTA_SUPPORT
    ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
#endif
}

/*--------------------------------------------------------------------
 * Network Steering 재시도 타이머
 *  - 타이머는 항상 1개만 존재 (s_steerTimerEvt)
 *  - 1회 실행 후 종료(-1). 다음 재시도는 CommissioningCb 실패 시 다시 예약
 *    (기존: 콜백이 10초 주기로 계속 돌면서, 실패할 때마다 새 타이머를 또 추가
 *     → 실패가 쌓일수록 steering 이 겹쳐 시작되고 타이머 풀이 고갈됨)
 *------------------------------------------------------------------*/
static ev_timer_event_t *s_steerTimerEvt = NULL;

static s32 steer_retry_cb(void *arg)
{
    (void)arg;
    s_steerTimerEvt = NULL;     /* -1 반환 시 SDK가 이벤트를 해제함 */

    if (zb_isDeviceJoinedNwk()) {
        DBG_LOG("[ZB] steer_retry: already joined, stop\r\n");
        return -1;
    }
    DBG_LOG("[ZB] steer_retry: start NetworkSteer\r\n");
    bdb_networkSteerStart();
    return -1;
}

static void steer_retry_schedule(u32 delay_ms)
{
    if (s_steerTimerEvt) {
        TL_ZB_TIMER_CANCEL(&s_steerTimerEvt);
    }
    s_steerTimerEvt = TL_ZB_TIMER_SCHEDULE(steer_retry_cb, NULL, delay_ms);
}

/*--------------------------------------------------------------------
 * bdbInitCb
 *------------------------------------------------------------------*/
static void zbdemo_bdbInitCb(u8 status, u8 joinedNetwork)
{
    DBG_LOG("[ZB] bdbInitCb: status=%d joined=%d factoryNew=%d\r\n",
            (int)status, (int)joinedNetwork, (int)zb_isDeviceFactoryNew());

    if (joinedNetwork) {
        DBG_LOG("[ZB] Already joined. ShortAddr=0x%x\r\n",
                (unsigned int)zb_getLocalShortAddr());
        juntek_onJoined();
    } else {
        DBG_LOG("[ZB] Not joined -> start Network Steering\r\n");
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
    DBG_LOG("[ZB] CommissioningCb: status=%d joined=%d\r\n",
            (int)status, (int)zb_isDeviceJoinedNwk());

    if (status == BDB_COMMISSION_STA_SUCCESS) {
        DBG_LOG("[ZB] Joined! ShortAddr=0x%x\r\n",
                (unsigned int)zb_getLocalShortAddr());
        if (s_steerTimerEvt) {
            TL_ZB_TIMER_CANCEL(&s_steerTimerEvt);
        }
        juntek_onJoined();
    } else if (status == BDB_COMMISSION_STA_IN_PROGRESS) {
        DBG_LOG("[ZB] Steering in progress...\r\n");
    } else {
        DBG_LOG("[ZB] Steering failed (sta=%d%s) -> retry in 10s\r\n",
                (int)status, (status == 3) ? " NO_NETWORK" : "");
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        steer_retry_schedule(10000);
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
/* [수정] 라우터에서는 자식 노드가 나갈 때도 LeaveInd 가 옴 → 바로 미가입 처리하면
 * 정상 가입 중인데 LED 가 점멸하고 steering 을 다시 시작함.
 * 500ms 뒤 자신이 실제로 네트워크에서 빠졌는지 확인 (PWM5CH 와 동일 수정) */
static s32 leave_check_cb(void *arg)
{
    (void)arg;
    if (!zb_isDeviceJoinedNwk()) {
        DBG_LOG("[ZB] left network -> not joined, steer in 3s\r\n");
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        steer_retry_schedule(3000);
    } else {
        DBG_LOG("[ZB] LeaveInd for other node - still joined\r\n");
    }
    return -1;
}

void juntek_leaveIndHandler(nlme_leave_ind_t *pLeaveInd)
{
    (void)pLeaveInd;
    DBG_LOG("[ZB] LeaveInd\r\n");
    TL_ZB_TIMER_SCHEDULE(leave_check_cb, NULL, 500);
}

void juntek_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf)
{
    DBG_LOG("[ZB] LeaveCnf: status=%d\r\n", (int)pLeaveCnf->status);

    /* 버튼 공장초기화(zb_factoryReset) 로 스스로 나간 경우 —
     * 미가입 표시로 바꾸고 3초 뒤 Network Steering 재시작
     * (기존: 로그만 찍고 아무 동작 없음 → RESET 점멸로 멈춤) */
    if (pLeaveCnf->status == 0) {   /* SUCCESS */
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        steer_retry_schedule(3000);
    }
}

bool juntek_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate)
{
    (void)pNwkUpdate;
    return FALSE;
}

void juntek_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd)
{
    /* NWK Status 알림은 정상 운영 중에도(라우팅 정보 등) 자주 발생하므로
     * 실제로 네트워크에서 이탈한 상태인지 확인 후에만 LED를 NOT_JOINED 로 전환한다. */
    DBG_LOG("[ZB] NwkStatusInd: joined=%d\r\n", (int)zb_isDeviceJoinedNwk());
    if (pNwkStatusInd && !zb_isDeviceJoinedNwk()) {
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
