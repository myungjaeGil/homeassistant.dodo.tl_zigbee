/*********************************************************************
 * @file    heater_main.c
 * @brief   ZT3L 무시동히터 리모컨 — 메인
 *
 *  EP1 : 히터 전원 (ON / OFF 버튼 펄스)
 *  EP2 : UP 버튼   (순간 동작)
 *  EP3 : DOWN 버튼 (순간 동작)
 *
 *  - 히터 컨트롤러의 버튼 라인을 GPIO 펄스로 눌러줌
 *  - 같은 라인을 평소 입력으로 감시 — 원래 리모컨/본체 버튼 조작도 상태에 반영
 *  - 레벨 0~9, 마지막 입력(동작·출처) 노출
 *  - 부팅 시 출력은 모두 비활성, 저장된 상태로 버튼을 자동으로 누르지 않음
 *  - BUTTON1 짧게 = 히터 토글, 5초 = Factory Reset
 *  - IR 리모컨 (NEC) ON / OFF / UP / DOWN
 *********************************************************************/

#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "gp.h"
#include "heater_main.h"
#include "app_ui.h"
#include "factory_reset.h"
#include "ir_recv.h"
#include "oled_ui.h"
#if ZBHCI_EN
#include "zbhci.h"
#endif

/*====================================================================
 * 전역 변수
 *==================================================================*/
app_ctx_t gHeaterCtx;

#ifdef ZCL_OTA
#ifndef OTA_PERIODIC_QUERY_INTERVAL
#define OTA_PERIODIC_QUERY_INTERVAL     (15 * 60)   /* 초 — OTA 서버 조회 주기 */
#endif
void heater_otaProcessMsgHandler(u8 evt, u8 status);
ota_callBack_t heater_otaCb = {
    heater_otaProcessMsgHandler,
};
ota_preamble_t heater_otaInfo = {
    .fileVer          = FILE_VERSION,
    .imageType        = IMAGE_TYPE,
    .manufacturerCode = MANUFACTURER_CODE_TELINK,
};
#endif

/*====================================================================
 * ZDO 콜백 등록 테이블
 *==================================================================*/
const zdo_appIndCb_t appCbLst = {
    bdb_zdoStartDevCnf,
    NULL,
    NULL,
    heater_leaveIndHandler,
    heater_leaveCnfHandler,
    heater_nwkUpdateIndicateHandler,
    NULL,
    NULL,
    NULL,
    NULL,
    heater_nwkStatusIndHandler,
};

/*====================================================================
 * BDB 커미셔닝 설정
 *==================================================================*/
bdb_commissionSetting_t g_bdbCommissionSetting = {
    .linkKey.tcLinkKey.keyType = SS_GLOBAL_LINK_KEY,
    .linkKey.tcLinkKey.key     = (u8 *)tcLinkKeyCentralDefault,

    .linkKey.distributeLinkKey.keyType = MASTER_KEY,
    .linkKey.distributeLinkKey.key     = (u8 *)linkKeyDistributedMaster,

    .linkKey.touchLinkKey.keyType = MASTER_KEY,
    .linkKey.touchLinkKey.key     = (u8 *)touchLinkKeyMaster,

#if TOUCHLINK_SUPPORT
    .touchlinkEnable = 1,
#else
    .touchlinkEnable = 0,
#endif
    .touchlinkChannel      = DEFAULT_CHANNEL,
    .touchlinkLqiThreshold = 0xA0,
};

/*====================================================================
 * stack_init / user_app_init
 *==================================================================*/
static void stack_init(void)
{
    zb_init();
    zb_zdoCbRegister((zdo_appIndCb_t *)&appCbLst);
}

static void user_app_init(void)
{
    u8 i;
    const u8 ep_list[HEATER_EP_COUNT] = {
        HEATER_EP_POWER,
        HEATER_EP_UP,
        HEATER_EP_DOWN,
    };
    const af_simple_descriptor_t *ep_desc[HEATER_EP_COUNT] = {
        &heater_ep1_simpleDesc,
        &heater_ep2_simpleDesc,
        &heater_ep3_simpleDesc,
    };

    af_nodeDescManuCodeUpdate(MANUFACTURER_CODE_TELINK);
    zcl_init(heater_zclProcessIncomingMsg);

    for (i = 0; i < HEATER_EP_COUNT; i++) {
        af_endpointRegister(ep_list[i],
                            (af_simple_descriptor_t *)ep_desc[i],
                            zcl_rx_handler, NULL);
    }

    /* NV 복원 — 표시용 전원 상태 + 펄스 설정 (버튼은 누르지 않음) */
    heater_attrs_init();

    for (i = 0; i < HEATER_EP_COUNT; i++) {
        zcl_register(ep_list[i], g_epClusterNum[i],
                     (zcl_specClusterInfo_t *)g_epClusterList[i]);
    }

#if ZCL_GP_SUPPORT
    gp_init(HEATER_EP_POWER);
#endif

#ifdef ZCL_OTA
    ota_init(OTA_TYPE_CLIENT, (af_simple_descriptor_t *)&heater_ep1_simpleDesc,
             &heater_otaInfo, &heater_otaCb);
#endif
}

/*====================================================================
 * 버튼 동작 완료 — UP/DOWN EP 는 펄스가 끝나면 Off 로 되돌림 (순간 동작)
 *==================================================================*/
void heater_onActDone(heater_act_t act)
{
    if (act == HEATER_ACT_UP)   g_heaterOnOff[HEATER_IDX_UP]   = FALSE;
    if (act == HEATER_ACT_DOWN) g_heaterOnOff[HEATER_IDX_DOWN] = FALSE;
}

/*====================================================================
 * OTA 콜백
 *==================================================================*/
#ifdef ZCL_OTA
void heater_otaProcessMsgHandler(u8 evt, u8 status)
{
    switch (evt) {
    case OTA_EVT_START:
        DBG_LOG("[OTA] Start\r\n");
        break;
    case OTA_EVT_COMPLETE:
        DBG_LOG("[OTA] Complete status=%d\r\n", (int)status);
        break;
    default:
        break;
    }
}
#endif

/*====================================================================
 * Network steering 재시도 — 타이머 1개만 유지
 *==================================================================*/
#define STEER_RETRY_MS      10000

static ev_timer_event_t *s_steerRetryTmr = NULL;

static s32 steer_retry_cb(void *arg)
{
    (void)arg;
    if (zb_isDeviceJoinedNwk()) {
        s_steerRetryTmr = NULL;
        return -1;
    }
    DBG_LOG("[ZB] steer_retry\r\n");
    bdb_networkSteerStart();
    return STEER_RETRY_MS;
}

static void steer_retry_schedule(u32 delay_ms)
{
    if (s_steerRetryTmr) return;
    s_steerRetryTmr = TL_ZB_TIMER_SCHEDULE(steer_retry_cb, NULL, delay_ms);
}

static void steer_retry_cancel(void)
{
    if (s_steerRetryTmr) {
        TL_ZB_TIMER_CANCEL(&s_steerRetryTmr);
        s_steerRetryTmr = NULL;
    }
}

static void app_update_join_indication(void)
{
    bool joined = zb_isDeviceJoinedNwk();
    led_power_set_state(joined ? LED_PWR_STATE_JOINED : LED_PWR_STATE_NOT_JOINED);
    oled_ui_notify_zigbee(joined ? 1 : 0);
}

/*====================================================================
 * ZDO 이벤트 핸들러
 *==================================================================*/
static s32 leave_check_cb(void *arg)
{
    (void)arg;
    /* 자식 노드가 나갈 때도 LeaveInd 가 오므로 자신이 빠졌는지 확인 */
    if (!zb_isDeviceJoinedNwk()) {
        app_update_join_indication();
        steer_retry_schedule(3000);
    }
    return -1;
}

void heater_leaveIndHandler(nlme_leave_ind_t *pLeaveInd)
{
    (void)pLeaveInd;
    DBG_LOG("[ZB] LeaveInd\r\n");
    TL_ZB_TIMER_SCHEDULE(leave_check_cb, NULL, 500);
}

void heater_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf) { (void)pLeaveCnf; }

bool heater_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate)
{
    (void)pNwkUpdate;
    return FALSE;
}

void heater_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd)
{
    /* route failure 등은 라우터에서 흔함 — 조인 상태와 무관, 로그만 */
    if (pNwkStatusInd) {
        DBG_LOG("[ZB] NwkStatus: %d\r\n", (int)pNwkStatusInd->status);
    }
}

/*====================================================================
 * BDB 콜백
 *==================================================================*/
static void zbdemo_bdbInitCb(u8 status, u8 joinedNetwork)
{
    DBG_LOG("[ZB] bdbInitCb: status=%d joined=%d\r\n",
            (int)status, (int)joinedNetwork);

#if IR_RECV_ENABLE
    ir_recv_init();     /* bdb 초기화 후 GPIO IRQ 재설정 */
#endif
    oled_ui_notify_zigbee(joinedNetwork);

    if (joinedNetwork) {
        led_power_set_state(LED_PWR_STATE_JOINED);
#ifdef ZCL_OTA
        ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
#endif
    } else {
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        bdb_networkSteerStart();
    }
}

static void zbdemo_bdbCommissioningCb(u8 status, void *arg)
{
    (void)arg;
    DBG_LOG("[ZB] CommissioningCb: status=%d\r\n", (int)status);
    if (status == BDB_COMMISSION_STA_SUCCESS) {
        DBG_LOG("[ZB] Joined! ShortAddr=0x%04x\r\n", (int)zb_getLocalShortAddr());
        steer_retry_cancel();
        app_update_join_indication();
#ifdef ZCL_OTA
        ota_queryStart(OTA_PERIODIC_QUERY_INTERVAL);
#endif
    } else if (status == BDB_COMMISSION_STA_IN_PROGRESS) {
        DBG_LOG("[ZB] Steering...\r\n");
    } else {
        DBG_LOG("[ZB] Steering failed -> retry 10s\r\n");
        app_update_join_indication();
        steer_retry_schedule(STEER_RETRY_MS);
    }
}

bdb_appCb_t g_zbDemoBdbCb = {
    zbdemo_bdbInitCb,
    zbdemo_bdbCommissioningCb,
    NULL,
    NULL,
};

/*====================================================================
 * app_task — 메인 루프에서 매번 호출
 *==================================================================*/
void app_task(void)
{
    static u32 s_cfgChkTick = 0;

    heater_in_poll();       /* 외부 버튼 입력 감시 (히터 라인) */
    app_key_handler();
    localPermitJoinState();
#if IR_RECV_ENABLE
    ir_recv_poll();
#endif
    oled_ui_update();       /* 변경 시에만 + 200ms 간격 제한 */

    /* 펄스 설정 속성 변경 감지 (Write 처리 경로와 별개로 500ms 마다) */
    if (clock_time_exceed(s_cfgChkTick, 500 * 1000)) {
        s_cfgChkTick = clock_time();
        heater_cfg_check();
    }

    /* 설정된 attribute reporting 전송 */
    if (BDB_STATE_GET() == BDB_STATE_IDLE) {
        report_handler();
    }
}

/*====================================================================
 * sysException
 *==================================================================*/
static void heaterSysException(void)
{
    SYSTEM_RESET();
}

/*====================================================================
 * user_init
 *==================================================================*/
void user_init(bool isRetention)
{
    (void)isRetention;

    /* 버튼 라인을 가장 먼저 입력 대기로 — 부팅 중 오동작 방지 */
    heater_out_init();

    sys_exceptHandlerRegister(heaterSysException);

    DBG_LOG("\r\n");
    DBG_LOG("========================================\r\n");
    DBG_LOG(" ZT3L Diesel Heater Remote\r\n");
    DBG_LOG(" App v%d.%d  Stack v%d.%d\r\n",
            (int)((APP_RELEASE >> 4) & 0xF), (int)(APP_RELEASE & 0xF),
            (int)((STACK_RELEASE >> 4) & 0xF), (int)(STACK_RELEASE & 0xF));
    DBG_LOG(" EP1: POWER  EP2: UP  EP3: DOWN  (%s, active %s)\r\n",
            HEATER_POWER_SINGLE_BUTTON ? "single power btn" : "ON/OFF btns",
            HEATER_ACTIVE_LEVEL ? "HIGH" : "LOW");
    DBG_LOG("========================================\r\n");

    /* PD7 — DEBUG soft-TX idle HIGH */
    gpio_set_func(GPIO_PD7, AS_GPIO);
    gpio_set_output_en(GPIO_PD7, 1);
    gpio_set_input_en(GPIO_PD7, 0);
    gpio_write(GPIO_PD7, 1);

    led_power_init();
    led_power_set_state(LED_PWR_STATE_NOT_JOINED);

    /* BUTTON1 (PD4, Pin11) */
    drv_gpio_func_set(BUTTON1);
    drv_gpio_input_en(BUTTON1, 1);
    drv_gpio_output_en(BUTTON1, 0);
    drv_gpio_up_down_resistor(BUTTON1, PM_PIN_PULLUP_10K);

    stack_init();
    user_app_init();
    oled_ui_init();

#if ZBHCI_EN
    zbhciInit();
    ev_on_poll(EV_POLL_HCI, zbhciTask);
#endif

    ev_on_poll(EV_POLL_IDLE, app_task);

    if (bdb_preInstallCodeLoad(&gHeaterCtx.tcLinkKey.keyType,
                               gHeaterCtx.tcLinkKey.key) == RET_OK) {
        g_bdbCommissionSetting.linkKey.tcLinkKey.keyType = gHeaterCtx.tcLinkKey.keyType;
        g_bdbCommissionSetting.linkKey.tcLinkKey.key     = gHeaterCtx.tcLinkKey.key;
    }

    bdb_init((af_simple_descriptor_t *)&heater_ep1_simpleDesc,
             &g_bdbCommissionSetting, &g_zbDemoBdbCb, 1);

#if IR_RECV_ENABLE
    /* bdb_init 이후 호출해야 GPIO IRQ 설정이 유지됨 */
    ir_recv_init();
#endif

    DBG_LOG("[BOOT] ZB stack init done. joined=%d\r\n",
            (int)zb_isDeviceJoinedNwk());
}
