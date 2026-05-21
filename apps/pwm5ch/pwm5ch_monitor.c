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
#if ZBHCI_EN
#include "zbhci.h"
#endif

/*====================================================================
 * 전역 변수
 *==================================================================*/
app_ctx_t gPwmCtx;

#ifdef ZCL_OTA
extern ota_callBack_t pwm5ch_otaCb;
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

    for (i = 0; i < PWM_EP_COUNT; i++) {
        zcl_register(ep_list[i], g_epClusterNum[i],
                     (zcl_specClusterInfo_t *)g_epClusterList[i]);
    }

#if ZCL_GP_SUPPORT
    gp_init(PWM_EP_CH0);
#endif

    /* NV 복원 상태를 PWM 하드웨어에 즉시 반영 */
    for (i = 0; i < PWM_EP_COUNT; i++) {
        pwm_hw_apply(i);
    }
}

/*====================================================================
 * OTA 콜백 (간이)
 *==================================================================*/
void pwm5ch_otaProcessMsgHandler(u8 evt, u8 *arg)
{
    (void)arg;
    switch (evt) {
    case OTA_EVT_START:
        printf("[OTA] Start\r\n");
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        break;
    case OTA_EVT_COMPLETE:
        printf("[OTA] Complete\r\n");
        break;
    default:
        break;
    }
}

ota_callBack_t pwm5ch_otaCb;

/*====================================================================
 * ZDO 이벤트 핸들러
 *==================================================================*/
static s32 steer_retry_cb(void *arg)
{
    (void)arg;
    if (zb_isDeviceJoinedNwk()) return -1;
    printf("[ZB] steer_retry\r\n");
    bdb_networkSteerStart();
    return 10000;
}

void pwm_leaveIndHandler(nlme_leave_ind_t *pLeaveInd)
{
    (void)pLeaveInd;
    printf("[ZB] LeaveInd\r\n");
    led_power_set_state(LED_PWR_STATE_NOT_JOINED);
    TL_ZB_TIMER_SCHEDULE(steer_retry_cb, NULL, 3000);
}

void pwm_leaveCnfHandler(nlme_leave_cnf_t *pLeaveCnf) { (void)pLeaveCnf; }

bool pwm_nwkUpdateIndicateHandler(nwkCmd_nwkUpdate_t *pNwkUpdate)
{
    (void)pNwkUpdate;
    return FALSE;
}

void pwm_nwkStatusIndHandler(zdo_nwk_status_ind_t *pNwkStatusInd)
{
    if (pNwkStatusInd) {
        printf("[ZB] NwkStatus: %d\r\n", (int)pNwkStatusInd->status);
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
    }
}

/*====================================================================
 * BDB 콜백
 *==================================================================*/
static void zbdemo_bdbInitCb(u8 status, u8 joinedNetwork)
{
    printf("[ZB] bdbInitCb: status=%d joined=%d\r\n",
           (int)status, (int)joinedNetwork);
    if (joinedNetwork) {
        led_power_set_state(LED_PWR_STATE_JOINED);
    } else {
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        bdb_networkSteerStart();
    }
}

static void zbdemo_bdbCommissioningCb(u8 status, void *arg)
{
    (void)arg;
    printf("[ZB] CommissioningCb: status=%d\r\n", (int)status);
    if (status == BDB_COMMISSION_STA_SUCCESS) {
        printf("[ZB] Joined! ShortAddr=%d\r\n", (int)zb_getLocalShortAddr());
        led_power_set_state(LED_PWR_STATE_JOINED);
    } else if (status == BDB_COMMISSION_STA_IN_PROGRESS) {
        printf("[ZB] Steering...\r\n");
    } else {
        printf("[ZB] Steering failed -> retry 10s\r\n");
        led_power_set_state(LED_PWR_STATE_NOT_JOINED);
        TL_ZB_TIMER_SCHEDULE(steer_retry_cb, NULL, 10000);
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

    printf("\r\n");
    printf("========================================\r\n");
    printf(" ZT3L PWM 5CH Controller\r\n");
    printf(" App v%d.%d  Stack v%d.%d\r\n",
           (int)((APP_RELEASE >> 4) & 0xF), (int)(APP_RELEASE & 0xF),
           (int)((STACK_RELEASE >> 4) & 0xF), (int)(STACK_RELEASE & 0xF));
    printf(" Clock: %dMHz  PWM: %dkHz\r\n",
           (int)(CLOCK_SYS_CLOCK_HZ / 1000000),
           (int)(CLOCK_SYS_CLOCK_HZ / PWM_MAX_TICK / 1000));
    printf(" EP1~EP5: 개별 채널  EP6: 마스터 디밍\r\n");
    printf("========================================\r\n");

    /* HW 초기화 (PWM 5ch + LED + GPIO) */
    pwm_hw_init();

    /* BUTTON1 (PD4) */
    drv_gpio_func_set(BUTTON1);
    drv_gpio_input_en(BUTTON1, 1);
    drv_gpio_output_en(BUTTON1, 0);
    drv_gpio_up_down_resistor(BUTTON1, PM_PIN_PULLUP_10K);

    /* Zigbee 스택 */
    stack_init();

    /* 앱 초기화 (EP 등록 + NV 복원 + PWM 적용) */
    user_app_init();

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

    printf("[BOOT] ZB stack init done. joined=%d\r\n",
           (int)zb_isDeviceJoinedNwk());
}
