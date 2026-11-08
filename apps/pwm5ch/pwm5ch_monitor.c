/*********************************************************************
 * @file    pwm5ch_monitor.c
 * @brief   ZT3L PWM 5채널 컨트롤러 — 메인
 *
 *  EP1~EP5 : 개별 채널 OnOff + LevelControl (PWM CH0~CH4)
 *  EP6     : 마스터 디밍 OnOff + LevelControl (전채널 동시)
 *
 *  - ZCL OnOff / LevelControl → PWM 듀티 변경
 *  - NV 저장으로 전원 복구 시 상태 복원
 *  - 버튼 길게 누름(5초) → Factory Reset
 *********************************************************************/

#include "tl_common.h"
#include "oled_ui.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "gp.h"
#include "pwm5ch_monitor.h"
#include "pwm5ch_ctrl.h"
#include "pwm5ch_ep.h"
#include "app_ui.h"
#include "factory_reset.h"
#include "ir_recv.h"
#if ZBHCI_EN
#include "zbhci.h"
#endif

/*====================================================================
 * 전역 변수
 *==================================================================*/
app_ctx_t gPwmCtx;

#ifdef ZCL_OTA
#ifndef OTA_PERIODIC_QUERY_INTERVAL
#define OTA_PERIODIC_QUERY_INTERVAL     (15 * 60)   /* 초 — OTA 서버 조회 주기 */
#endif
void pwm5ch_otaProcessMsgHandler(u8 evt, u8 status);
ota_callBack_t pwm5ch_otaCb = {
    pwm5ch_otaProcessMsgHandler,
};
ota_preamble_t pwm5ch_otaInfo = {
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
    pwm_leaveIndHandler,
    pwm_leaveCnfHandler,
    pwm_nwkUpdateIndicateHandler,
    NULL,
    NULL,
    NULL,
    NULL,
    pwm_nwkStatusIndHandler,
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
 * stack_init
 *==================================================================*/
static void stack_init(void)
{
    zb_init();
    zb_zdoCbRegister((zdo_appIndCb_t *)&appCbLst);
}

/*====================================================================
 * user_app_init — EP1~EP6 등록 + NV 복원 후 PWM 출력
 *==================================================================*/
static void user_app_init(void)
{
    u8 i;
    u8 ep_list[] = {
        PWM_EP_CH0,
        PWM_EP_CH1,
        PWM_EP_CH2,
        PWM_EP_CH3,
        PWM_EP_CH4,
        PWM_EP_MASTER,
    };
    const af_simple_descriptor_t *ep_desc[] = {
        &pwm_ep1_simpleDesc,
        &pwm_ep2_simpleDesc,
        &pwm_ep3_simpleDesc,
        &pwm_ep4_simpleDesc,
        &pwm_ep5_simpleDesc,
        &pwm_ep6_simpleDesc,
    };

    af_nodeDescManuCodeUpdate(MANUFACTURER_CODE_TELINK);
    zcl_init(pwm_zclProcessIncomingMsg);

    for (i = 0; i < PWM_EP_COUNT; i++) {
        af_endpointRegister(ep_list[i],
                            (af_simple_descriptor_t *)ep_desc[i],
                            zcl_rx_handler, NULL);
    }

    /* NV에서 채널 상태 복원 */
    pwm_attrs_init();
    pwm_dimRange_init();    /* 채널별 디밍 노출 구간 복원 */

    for (i = 0; i < PWM_EP_COUNT; i++) {
        zcl_register(ep_list[i], g_epClusterNum[i],
                     (zcl_specClusterInfo_t *)g_epClusterList[i]);
    }

#if ZCL_GP_SUPPORT
    gp_init(PWM_EP_CH0);
#endif

#ifdef ZCL_OTA
    /* [수정] 기존에는 ota_init 호출이 없어 OTA 가 동작하지 않았음 */
    ota_init(OTA_TYPE_CLIENT, (af_simple_descriptor_t *)&pwm_ep1_simpleDesc,
             &pwm5ch_otaInfo, &pwm5ch_otaCb);
#endif

    /* NV 복원 상태를 PWM 하드웨어에 즉시 반영 */
    for (i = 0; i < PWM_EP_COUNT; i++) {
        pwm_hw_apply(i);
        DBG_LOG("[PWM] CH%d apply: onOff=%d lv=%d\r\n",
                (int)i,
                (int)g_pwmChAttrs[i].onOff,
                (int)g_pwmChAttrs[i].currentLevel);
    }
}

/*====================================================================
 * OTA 콜백
 *==================================================================*/
#ifdef ZCL_OTA
void pwm5ch_otaProcessMsgHandler(u8 evt, u8 status)
{
    switch (evt) {
    case OTA_EVT_START:
        DBG_LOG("[OTA] Start\r\n");
        break;
    case OTA_EVT_COMPLETE:
        DBG_LOG("[OTA] Complete status=%d\r\n", (int)status);
        if (status == ZCL_STA_SUCCESS) {
            pwm_attrs_flush();      /* 재부팅 전 예약된 NV 저장 마무리 */
        }
        break;
    default:
        break;
    }
}
#endif

/*====================================================================
 * Network steering 재시도 — 타이머 1개만 유지
 *   [수정] 기존에는 실패할 때마다 10초 주기 타이머가 새로 생겨
 *          코디네이터 부재 시 타이머가 계속 늘어났음
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
    if (s_steerRetryTmr) return;    /* 이미 예약됨 — 중복 생성 금지 */
    s_steerRetryTmr = TL_ZB_TIMER_SCHEDULE(steer_retry_cb, NULL, delay_ms);
}

static void steer_retry_cancel(void)
{
    if (s_steerRetryTmr) {
        TL_ZB_TIMER_CANCEL(&s_steerRetryTmr);
        s_steerRetryTmr = NULL;
    }
}

/* 조인 상태에 맞춰 LED / OLED 갱신 */
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
    /* 라우터에서는 자식 노드가 나갈 때도 LeaveInd 가 올 수 있으므로
     * 실제로 자신이 네트워크에서 빠졌는지 확인 후 처리 */
    if (!zb_isDeviceJoinedNwk()) {
        app_update_join_indication();
        steer_retry_schedule(3000);
    }
    return -1;
}

void pwm_leaveIndHandler(nlme_leave_ind_t *pLeaveInd)
{
    (void)pLeaveInd;
    DBG_LOG("[ZB] LeaveInd\r\n");
    TL_ZB_TIMER_SCHEDULE(leave_check_cb, NULL, 500);
}

void pwm_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf) { (void)pLeaveCnf; }

bool pwm_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate)
{
    (void)pNwkUpdate;
    return FALSE;
}

void pwm_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd)
{
    /* [수정] route failure 등 NWK status 는 라우터에서 흔함 —
     *        조인 상태와 무관하므로 LED 를 바꾸지 않고 로그만 남김 */
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

    /* bdb 초기화 완료 후 IR IRQ 재설정 */
    ir_recv_init();
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
    app_key_handler();
    localPermitJoinState();
    ir_recv_poll();
    oled_ui_update();       /* 내부에서 변경 시에만 + 200ms 간격 제한 */

    /* 디밍 노출 구간 속성 변경 감지 (Write 응답 경로와 별개로 500ms 마다 확인) */
    {
        static u32 s_dimChkTick = 0;
        if (clock_time_exceed(s_dimChkTick, 500 * 1000)) {
            s_dimChkTick = clock_time();
            pwm_dimRange_check();
        }
    }

    /* [수정] 설정된 attribute reporting 전송 — 없으면 IR 등 로컬 변경이 HA 에 반영 안 됨 */
    if (BDB_STATE_GET() == BDB_STATE_IDLE) {
        report_handler();
    }
}

/*====================================================================
 * sysException
 *==================================================================*/
static void pwmSysException(void)
{
    SYSTEM_RESET();
}

/*====================================================================
 * user_init
 *==================================================================*/
void user_init(bool isRetention)
{
    (void)isRetention;

    sys_exceptHandlerRegister(pwmSysException);

    DBG_LOG("\r\n");
    DBG_LOG("========================================\r\n");
    DBG_LOG(" ZT3L PWM 5CH Controller\r\n");
    DBG_LOG(" App v%d.%d  Stack v%d.%d\r\n",
           (int)((APP_RELEASE >> 4) & 0xF), (int)(APP_RELEASE & 0xF),
           (int)((STACK_RELEASE >> 4) & 0xF), (int)(STACK_RELEASE & 0xF));
    DBG_LOG(" Clock: %dMHz  PWM: %dHz\r\n",
           (int)(CLOCK_SYS_CLOCK_HZ / 1000000),
           (int)(CLOCK_SYS_CLOCK_HZ / PWM_MAX_TICK));
    DBG_LOG(" EP1~EP5: 개별 채널  EP6: 마스터 디밍\r\n");
    DBG_LOG("========================================\r\n");

    /* HW 초기화 (PWM 5ch + LED + GPIO) */
    pwm_hw_init();

    /* BUTTON1 (PD4, Pin11) */
    drv_gpio_func_set(BUTTON1);
    drv_gpio_input_en(BUTTON1, 1);
    drv_gpio_output_en(BUTTON1, 0);
    drv_gpio_up_down_resistor(BUTTON1, PM_PIN_PULLUP_10K);

    /* Zigbee 스택 */
    stack_init();

    /* 앱 초기화 (EP 등록 + NV 복원 + PWM 적용) */
    user_app_init();

    /* OLED 초기화 */
    oled_ui_init();

#if ZBHCI_EN
    zbhciInit();
    ev_on_poll(EV_POLL_HCI, zbhciTask);
#endif

    ev_on_poll(EV_POLL_IDLE, app_task);

    if (bdb_preInstallCodeLoad(&gPwmCtx.tcLinkKey.keyType,
                               gPwmCtx.tcLinkKey.key) == RET_OK) {
        g_bdbCommissionSetting.linkKey.tcLinkKey.keyType =
            gPwmCtx.tcLinkKey.keyType;
        g_bdbCommissionSetting.linkKey.tcLinkKey.key =
            gPwmCtx.tcLinkKey.key;
    }

    bdb_init((af_simple_descriptor_t *)&pwm_ep1_simpleDesc,
             &g_bdbCommissionSetting, &g_zbDemoBdbCb, 1);

    /* IR 수신 초기화 — bdb_init 이후 호출해야 GPIO IRQ 설정이 유지됨
     * Zigbee 스택 초기화가 reg_gpio_wakeup_irq 를 덮어쓰기 때문 */
    ir_recv_init();

    DBG_LOG("[BOOT] ZB stack init done. joined=%d\r\n",
           (int)zb_isDeviceJoinedNwk());
}
