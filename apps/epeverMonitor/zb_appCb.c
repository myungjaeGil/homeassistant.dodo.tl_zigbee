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
 * OTA 콜백
 *   [수정] 시그니처를 SDK 의 ota_procMsgCb_t (u8 evt, u8 status) 에 맞추고
 *   epever_otaCb 로 등록 (기존: 빈 sampleLight_otaCb, ota_init 미호출 → OTA 미동작)
 *------------------------------------------------------------------*/
#ifndef OTA_PERIODIC_QUERY_INTERVAL
#define OTA_PERIODIC_QUERY_INTERVAL     (15 * 60)   /* 초 — OTA 서버 조회 주기 */
#endif

void epever_otaProcessMsgHandler(u8 evt, u8 status)
{
    switch (evt) {
    case OTA_EVT_START:
        DBG_LOG("[OTA] Start\r\n");
        break;
    case OTA_EVT_COMPLETE:
        DBG_LOG("[OTA] Complete status=%d\r\n", (int)status);
        break;
    default:
        DBG_LOG("[OTA] evt=%d status=%d\r\n", (int)evt, (int)status);
        break;
    }
}

#ifdef ZCL_OTA
ota_callBack_t epever_otaCb = {
    epever_otaProcessMsgHandler,
};
#endif

/*--------------------------------------------------------------------
 * Network Steering 재시도 타이머
 *  - 타이머는 항상 1개만 존재 (s_steerTimerEvt)
 *  - 1회 실행 후 종료(-1). 다음 재시도는 CommissioningCb 실패 시 다시 예약
 *    (기존: 매 실패마다 10초 주기 타이머가 새로 추가되어 누적
 *     → 타이머 풀 고갈 → sys exception → 리셋)
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
 * Z2M 연결 감시 (heartbeat)
 *
 *  가입 상태에서 30초마다 코디네이터(0x0000)로 Basic ZCLVersion 리포트를
 *  "Default Response 요청" 상태로 전송한다.
 *   - Default Response 는 코디네이터 동글이 아니라 Z2M(zigbee-herdsman)이
 *     생성해서 보내므로, 응답이 오면 Z2M 까지 살아있다는 뜻
 *   - APS ACK 는 동글 펌웨어가 자동 응답하므로 Z2M 생존 판단에 쓰지 않음
 *  5초 안에 응답이 없으면 miss++, 3회 연속 miss → Z2M 끊김 표시
 *  코디네이터로부터 어떤 ZCL 메시지든 수신되면 즉시 연결됨으로 복귀
 *------------------------------------------------------------------*/
#define LINK_HB_INTERVAL_MS     30000
#define LINK_HB_FIRST_MS        5000
#define LINK_RSP_TIMEOUT_MS     5000
#define LINK_MISS_MAX           3

static ev_timer_event_t *s_linkHbEvt = NULL;
static ev_timer_event_t *s_linkToEvt = NULL;
static u8   s_linkMiss    = 0;
static bool s_linkWaiting = FALSE;

void epever_link_alive(const char *why)
{
    if (s_linkToEvt) {
        TL_ZB_TIMER_CANCEL(&s_linkToEvt);
    }
    if (s_linkMiss >= LINK_MISS_MAX) {
        DBG_LOG("[LINK] Z2M reconnected (%s)\r\n", why);
    }
    s_linkWaiting = FALSE;
    s_linkMiss    = 0;
    led_power_set_link(TRUE);
}

static s32 link_timeout_cb(void *arg)
{
    (void)arg;
    s_linkToEvt = NULL;
    if (s_linkWaiting) {
        s_linkWaiting = FALSE;
        if (s_linkMiss < 0xFF) s_linkMiss++;
        DBG_LOG("[LINK] no response from Z2M (miss %d/%d)\r\n",
               (int)s_linkMiss, (int)LINK_MISS_MAX);
        if (s_linkMiss == LINK_MISS_MAX) {
            DBG_LOG("[LINK] Z2M disconnected\r\n");
            led_power_set_link(FALSE);
        }
    }
    return -1;
}

static s32 link_hb_cb(void *arg)
{
    (void)arg;
    if (!zb_isDeviceJoinedNwk()) {
        return LINK_HB_INTERVAL_MS;
    }

    epInfo_t dstEp;
    TL_SETSTRUCTCONTENT(dstEp, 0);
    dstEp.dstAddrMode       = APS_SHORT_DSTADDR_WITHEP;
    dstEp.dstAddr.shortAddr = 0x0000;
    dstEp.dstEp             = 1;
    dstEp.profileId         = HA_PROFILE_ID;
    dstEp.txOptions         = APS_TX_OPT_ACK_TX;

    status_t st = zcl_report(EPEVER_ENDPOINT_SOLAR, &dstEp,
                             FALSE,                        /* Default Response 요청 */
                             ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                             MANUFACTURER_CODE_NONE,
                             ZCL_CLUSTER_GEN_BASIC,
                             ZCL_ATTRID_BASIC_ZCL_VER,
                             ZCL_DATA_TYPE_UINT8,
                             (u8 *)&g_zcl_basicAttrs.zclVersion);
    // DBG_LOG("[LINK] heartbeat sent (st=%d)\r\n", (int)st);
    (void)st;

    s_linkWaiting = TRUE;
    if (s_linkToEvt) {
        TL_ZB_TIMER_CANCEL(&s_linkToEvt);
    }
    s_linkToEvt = TL_ZB_TIMER_SCHEDULE(link_timeout_cb, NULL, LINK_RSP_TIMEOUT_MS);
    return LINK_HB_INTERVAL_MS;
}

static void epever_link_start(void)
{
    s_linkMiss    = 0;
    s_linkWaiting = FALSE;
    led_power_set_link(TRUE);
    if (!s_linkHbEvt) {
        s_linkHbEvt = TL_ZB_TIMER_SCHEDULE(link_hb_cb, NULL, LINK_HB_FIRST_MS);
    }
}

static void epever_link_stop(void)
{
    if (s_linkHbEvt) {
        TL_ZB_TIMER_CANCEL(&s_linkHbEvt);
    }
    if (s_linkToEvt) {
        TL_ZB_TIMER_CANCEL(&s_linkToEvt);
    }
    s_linkMiss    = 0;
    s_linkWaiting = FALSE;
    led_power_set_link(TRUE);   /* 미가입 표시는 NOT_JOINED 가 담당 */
}

/*--------------------------------------------------------------------
 * bdbInitCb
 *------------------------------------------------------------------*/
static void zbdemo_bdbInitCb(u8 status, u8 joinedNetwork)
{
    DBG_LOG("[ZB] bdbInitCb: status=%d joinedNetwork=%d\r\n",
           (int)status, (int)joinedNetwork);
    if (joinedNetwork) {
        DBG_LOG("[ZB] Already joined. ShortAddr=0x%x\r\n",
               (unsigned int)zb_getLocalShortAddr());
        led_power_set_state(LED_PWR_STATE_JOINED);
        epever_link_start();
#ifdef ZCL_OTA
        ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
#endif
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
    DBG_LOG("[ZB] CommissioningCb: status=%d\r\n", (int)status);
    if (status == BDB_COMMISSION_STA_SUCCESS) {
        DBG_LOG("[ZB] Joined! ShortAddr=0x%x\r\n",
               (unsigned int)zb_getLocalShortAddr());
        if (s_steerTimerEvt) {
            TL_ZB_TIMER_CANCEL(&s_steerTimerEvt);
        }
        led_power_set_state(LED_PWR_STATE_JOINED);
        epever_link_start();
#ifdef ZCL_OTA
        ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
#endif
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
        epever_link_stop();
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        steer_retry_schedule(3000);
    } else {
        DBG_LOG("[ZB] LeaveInd for other node — still joined\r\n");
    }
    return -1;
}

void epever_leaveIndHandler(nlme_leave_ind_t *pLeaveInd)
{
    (void)pLeaveInd;
    DBG_LOG("[ZB] LeaveInd\r\n");
    TL_ZB_TIMER_SCHEDULE(leave_check_cb, NULL, 500);
}

void epever_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf)
{
    DBG_LOG("[ZB] LeaveCnf: status=%d\r\n", (int)pLeaveCnf->status);

    /* 버튼 공장초기화(zb_factoryReset) 후 SDK 가 재부팅하지 않으므로
     * 여기서 미가입 상태로 전환하고 3초 뒤 Network Steering 재시작.
     * (기존: 가입 상태에서 초기화하면 LED 가 RESET 점멸로 멈추고 재가입 시도 없음) */
    if (pLeaveCnf->status == 0) {   /* SUCCESS */
        epever_link_stop();
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        steer_retry_schedule(3000);
    }
}

bool epever_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate)
{
    (void)pNwkUpdate;
    return FALSE;
}

void epever_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd)
{
    if (pNwkStatusInd) {
        /* 일시적인 경로/주소 알림 — 가입 상태는 그대로이므로 LED 는 바꾸지 않음
         * (기존: NOT_JOINED 로 바꾼 뒤 복구하지 않아 가입 중에도 계속 점멸) */
        DBG_LOG("[ZB] NwkStatusInd: status=0x%x\r\n",
               (unsigned int)pNwkStatusInd->status);
    }
}

/*--------------------------------------------------------------------
 * Factory Reset LED
 *------------------------------------------------------------------*/
void zbdemo_factoryResetCb(void)
{
    led_power_set_state(LED_PWR_STATE_RESET);
}
