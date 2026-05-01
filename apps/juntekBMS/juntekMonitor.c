/*********************************************************************
 * @file    juntekMonitor.c
 * @brief   ZT3L JUNTEK JUNTEK BMS 모니터 메인
 *
 * UART RX → JUNTEK :r50 프로토콜 파싱
 * → juntek_data_t 콜백 → ZCL 속성 업데이트 → Zigbee 리포팅
 *
 * EP 구성:
 *   EP1: Electrical Measurement (Voltage/Current/Power)
 *   EP2: Temperature Measurement
 *   EP3: Binary Input (Relay: 충전/방전)
 *
 * JUNTEK :r50 포맷:
 *   :r50=idx,?,volt,amp,ah,?,?,?,temp,?,?,relay,elapsed,...\r\n
 *   p[2]  voltage  ÷100 → V
 *   p[3]  current  ÷100 → A  (relay<1 이면 음수)
 *   p[4]  remain_ah ÷1000 → Ah
 *   p[8]  temp     -100 → °C
 *   p[11] relay    1=충전, 0=방전
 *   p[12] elapsed  분
 *********************************************************************/

#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "gp.h"
#include "juntekMonitor.h"
#include "juntekCtrl.h"
#include "juntek_ep.h"
#include "app_ui.h"
#include "factory_reset.h"
#if ZBHCI_EN
#include "zbhci.h"
#endif
#if ZCL_WWAH_SUPPORT
#include "wwah.h"
#endif

/**********************************************************************
 * GLOBAL VARIABLES
 */
app_ctx_t gJuntekCtx;

#ifdef ZCL_OTA
extern ota_callBack_t juntek_otaCb;
ota_preamble_t juntek_otaInfo = {
    .fileVer          = FILE_VERSION,
    .imageType        = IMAGE_TYPE,
    .manufacturerCode = MANUFACTURER_CODE_TELINK,
};
#endif

const zdo_appIndCb_t appCbLst = {
    bdb_zdoStartDevCnf,
    NULL,
    NULL,
    juntek_leaveIndHandler,
    juntek_leaveCnfHandler,
    juntek_nwkUpdateIndicateHandler,
    NULL,
    NULL,
    NULL,
    NULL,
    juntek_nwkStatusIndHandler,
};

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
    .touchlinkChannel    = DEFAULT_CHANNEL,
    .touchlinkLqiThreshold = 0xA0,
};

/**********************************************************************
 * UART RX
 */
#ifndef UART_BUF_SIZE
#define UART_BUF_SIZE   128     /* 4(DMA 헤더) + 124(데이터), 4의 배수 */
#endif

volatile u8  uart_rx_buf[UART_BUF_SIZE] __attribute__((aligned(4))) = {0};
volatile u8  uart_rx_flag = 0;

extern drv_uart_t myUartDriver;

/**********************************************************************
 * juntek_atoi — strtol 방식의 안전한 문자열→정수 변환
 *
 * TC32 toolchain 은 표준 stdlib.h 의 atoi/strtol 을 제공하지 않으므로
 * 직접 구현. strtol 과 동일한 동작:
 *  - 선행 공백 무시
 *  - 부호(+/-) 처리
 *  - 숫자 아닌 문자에서 중단
 *  - 오버플로 시 INT_MAX / INT_MIN 반환
 **********************************************************************/
static int juntek_atoi(const char *str)
{
    long  res  = 0;
    int   sign = 1;
    const char *p = str;

    while (*p == ' ' || *p == '\t') p++;

    if (*p == '-')      { sign = -1; p++; }
    else if (*p == '+') {            p++; }

    while (*p >= '0' && *p <= '9') {
        res = res * 10 + (*p - '0');
        /* 오버플로 방지 (long 기준 2,147,483,647) */
        if (sign == 1  && res >  2147483647L) return  2147483647;
        if (sign == -1 && res > 2147483648L)  return -2147483647 - 1;
        p++;
    }
    return (int)(sign * res);
}

/**********************************************************************
 * JUNTEK :r50 라인 조립 버퍼
 * DMA 수신은 패킷 단위가 아닐 수 있으므로 \n 까지 조립
 */
#define LINE_BUF_SIZE   200
static char  s_line_buf[LINE_BUF_SIZE];
static u16   s_line_pos = 0;

/**********************************************************************
 * JUNTEK :r50 파서
 *
 * 입력: ":r50=1,220,1331,0,200000,0,0,504102,125,0,0,1,90,100,...\r\n"
 * 출력: juntek_data_t (valid=true 이면 파싱 성공)
 *
 * TODO: 파싱 완료 후 처리 로직은 juntek_r50_process() 에서 구현
 **********************************************************************/
static void juntek_parse_r50(const char *line, juntek_data_t *out)
{
    out->valid = false;

    /* ':r50=' 시작 확인 */
    const char *p = line;
    while (*p && *p != ':') p++;
    if (!*p) return;

    /* '=' 위치 찾기 */
    const char *eq = p;
    while (*eq && *eq != '=') eq++;
    if (!*eq) return;

    /* 커맨드 확인: :r50 */
    /* colon~eq 구간에서 '/' 제거 후 비교 */
    char cmd[8] = {0};
    u8 ci = 0;
    const char *cp = p;
    while (cp < eq && ci < 7) {
        if (*cp != '/') cmd[ci++] = *cp;
        cp++;
    }
    if (ci < 3 || cmd[0] != ':' || cmd[1] != 'r' || cmd[2] != '5' || cmd[3] != '0') {
        return;
    }

    /* CSV 파싱 — eq+1 부터 */
    const char *s = eq + 1;
    char tokens[17][20];
    u8   tcnt = 0;
    u8   tlen = 0;

    while (*s && tcnt < 17) {
        if (*s == ',' || *s == '\r' || *s == '\n' || *s == '\0') {
            tokens[tcnt][tlen] = '\0';
            tcnt++;
            tlen = 0;
            if (*s == '\r' || *s == '\n' || *s == '\0') break;
        } else {
            if (tlen < 19) tokens[tcnt][tlen++] = *s;
        }
        s++;
    }
    /* 마지막 토큰 처리 */
    if (tlen > 0 && tcnt < 17) {
        tokens[tcnt][tlen] = '\0';
        tcnt++;
    }

    if (tcnt < 13) {
        printf("BMS parse: token count %d < 13\r\n", tcnt);
        return;
    }

    /* 값 변환 */
    out->voltage     = (float)juntek_atoi(tokens[2])  / 100.0f;
    out->current     = (float)juntek_atoi(tokens[3])  / 100.0f;
    out->remain_ah   = (float)juntek_atoi(tokens[4])  / 1000.0f;
    out->temperature = (float)juntek_atoi(tokens[8])  - 100.0f;
    out->relay       = juntek_atoi(tokens[11]);
    out->elapsed_min = juntek_atoi(tokens[12]);

    /* 방전 시 전류/전력 음수 */
    if (out->relay < 1) out->current = -out->current;
    out->power = out->voltage * out->current;

    out->valid = true;

    printf("----------------------------------------\r\n");
    printf("BMS Voltage  >> %d.%02d V\r\n",
           (int)out->voltage, (int)(out->voltage * 100) % 100);
    printf("BMS Current  >> %c%d.%02d A\r\n",
           out->current < 0 ? '-' : '+',
           (int)(out->current < 0 ? -out->current : out->current),
           (int)((out->current < 0 ? -out->current : out->current) * 100) % 100);
    printf("BMS Power    >> %c%d.%01d W\r\n",
           out->power < 0 ? '-' : '+',
           (int)(out->power < 0 ? -out->power : out->power),
           (int)((out->power < 0 ? -out->power : out->power) * 10) % 10);
    printf("BMS RemainAh >> %d.%03d Ah\r\n",
           (int)out->remain_ah,
           (int)(out->remain_ah * 1000) % 1000);
    printf("BMS Temp     >> %d.%01d C\r\n",
           (int)out->temperature,
           (int)(out->temperature < 0 ? -out->temperature : out->temperature) % 10);
    printf("BMS Elapsed  >> %d min\r\n", (int)out->elapsed_min);
    printf("BMS Relay    >> %s\r\n", out->relay ? "CHG" : "DCH");
    printf("----------------------------------------\r\n");
}

/**********************************************************************
 * BMS 데이터 필터링
 *
 * 사양: LiFePO4 4셀 직렬, 240Ah, 2kW 인버터
 *
 * 1단계: 범위 검사 (Range Check) — 물리적으로 불가능한 값 제거
 * 2단계: 이전값 대비 변화량 제한 (Rate Limit) — 순간 튐 제거
 * 3단계: 이동평균 (Moving Average, N=4) — 노이즈 평활화
 *
 * 범위 기준:
 *   전압  : 4셀 × (2.5V~3.65V) = 10.0V ~ 14.6V, 여유 ±1V → 9.0~15.6V
 *   전류  : 방전 최대 2kW÷10V=200A, 충전 최대 0.5C=120A → -210A ~ +130A
 *   온도  : 상온 운용 -20°C ~ 60°C
 *   잔량  : 0 ~ 240Ah (공칭 용량), 여유 10% → 0 ~ 265Ah
 *
 * Rate Limit 기준 (샘플 주기 ~0.5초):
 *   전압  : 0.5초에 ±0.5V 이상 변화 불가
 *   전류  : 2kW 인버터 돌입전류 고려 ±80A/샘플
 *   온도  : 0.5초에 ±2°C 이상 변화 불가
 **********************************************************************/

#define BMS_FILTER_N    4   /* 이동평균 샘플 수 */

/* 샘플 주기 ~0.5초 기준 rate limit은 g_juntek_filterCfg에서 참조 */

typedef struct {
    float buf[BMS_FILTER_N];
    u8    idx;
    u8    filled;
    float last;
    bool  initialized;
} bms_filter_t;

static bms_filter_t f_voltage;
static bms_filter_t f_current;
static bms_filter_t f_temperature;
static bms_filter_t f_remain_ah;

/* 이동평균 */
static float filter_avg(bms_filter_t *f, float v)
{
    f->buf[f->idx] = v;
    f->idx = (f->idx + 1) % BMS_FILTER_N;
    if (f->idx == 0) f->filled = 1;

    u8 n = f->filled ? BMS_FILTER_N : (f->idx ? f->idx : 1);
    float sum = 0;
    for (u8 i = 0; i < n; i++) sum += f->buf[i];
    return sum / n;
}

/* 절댓값 (float) */
static float bms_fabsf(float v) { return v < 0.0f ? -v : v; }

/* 범위 검사 + rate limit + 이동평균 적용 */
static bool bms_filter_apply(juntek_data_t *d)
{
    const juntek_filter_cfg_t *c = &g_juntek_filterCfg;

    /* --- 1단계: 범위 검사 --- */
    if (d->voltage < c->volt_min || d->voltage > c->volt_max) {
        printf("BMS filter: V out %d.%02d\r\n",
               (int)d->voltage, (int)(d->voltage * 100) % 100);
        return false;
    }
    if (d->current < c->curr_min || d->current > c->curr_max) {
        printf("BMS filter: A out\r\n");
        return false;
    }
    if (d->temperature < c->temp_min || d->temperature > c->temp_max) {
        printf("BMS filter: T out\r\n");
        return false;
    }
    if (d->remain_ah < 0.0f || d->remain_ah > c->ah_max) {
        printf("BMS filter: Ah out\r\n");
        return false;
    }

    /* --- 2단계: Rate Limit --- */
    if (f_voltage.initialized) {
        if (bms_fabsf(d->voltage - f_voltage.last) > c->volt_rate) {
            printf("BMS filter: V spike\r\n");
            return false;
        }
        if (bms_fabsf(d->current - f_current.last) > c->curr_rate) {
            printf("BMS filter: A spike\r\n");
            return false;
        }
        if (bms_fabsf(d->temperature - f_temperature.last) > c->temp_rate) {
            printf("BMS filter: T spike\r\n");
            return false;
        }
    }

    /* --- 3단계: 이동평균 --- */
    d->voltage     = filter_avg(&f_voltage,     d->voltage);
    d->current     = filter_avg(&f_current,     d->current);
    d->temperature = filter_avg(&f_temperature, d->temperature);
    d->remain_ah   = filter_avg(&f_remain_ah,   d->remain_ah);
    d->power       = d->voltage * d->current;

    f_voltage.last     = d->voltage;
    f_current.last     = d->current;
    f_temperature.last = d->temperature;
    f_remain_ah.last   = d->remain_ah;

    f_voltage.initialized     = true;
    f_current.initialized     = true;
    f_temperature.initialized = true;
    f_remain_ah.initialized   = true;

    return true;
}

/**********************************************************************
 * juntek_r50_process — 파싱 완료 후 처리 콜백
 **********************************************************************/
static void juntek_r50_process(const juntek_data_t *d)
{
    juntek_data_t filtered = *d;

    if (!bms_filter_apply(&filtered)) {
        return;   /* 비정상 데이터 — 무시 */
    }

    /* ZCL 속성 업데이트 → Zigbee 리포팅 */
    juntek_attrs_update(&filtered);
}

/**********************************************************************
 * uart_rx_cb — IRQ 컨텍스트 (최소 작업만)
 **********************************************************************/
_attribute_ram_code_ void uart_rx_cb(void)
{
    uart_rx_flag = 1;
}

/**********************************************************************
 * uart_rx_poll — app_task() 에서 매 루프 호출
 *
 * DMA 버퍼 → 라인 버퍼 조립 → \n 수신 시 파싱
 **********************************************************************/
void uart_rx_poll(void)
{
//    if (uart_rx_flag) {
//        printf("POLL: flag=%d\r\n", uart_rx_flag);
//    }

    if (!uart_rx_flag) return;
    uart_rx_flag = 0;

    u16  len  = (uart_rx_buf[1] << 8) | uart_rx_buf[0];
    u8  *data = (u8 *)(uart_rx_buf + 4);

//    printf("uart_rx_buf len=%d \r\n", len);

    if (len == 0 || len > (UART_BUF_SIZE - 4)) {
        uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
        return;
    }

    /* 바이트 단위로 라인 버퍼에 조립 */
    u16 i;
    for (i = 0; i < len; i++) {
        char c = (char)data[i];
//        printf("%c", c);

        if (c == '\n' || c == '\r') {
            if (s_line_pos > 0) {
                s_line_buf[s_line_pos] = '\0';

                printf("BMS RX: %s\r\n", s_line_buf);

                /* :r50 파싱 */
                juntek_data_t d = {0};
                juntek_parse_r50(s_line_buf, &d);

                if (d.valid) {
                    juntek_r50_process(&d);
                }

                s_line_pos = 0;
            }
        } else {
            if (s_line_pos < LINE_BUF_SIZE - 1) {
                s_line_buf[s_line_pos++] = c;
            } else {
                /* 버퍼 오버플로 — 리셋 */
                s_line_pos = 0;
            }
        }
    }
//    printf("\r\n");

    /* DMA 재시작 */
    uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
}

/**********************************************************************
 * uart_rx_init
 **********************************************************************/
void uart_rx_init(void)
{
    uart_gpio_set(UART_TX_PIN, UART_RX_PIN);
    uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
    uart_init_baudrate(115200, CLOCK_SYS_CLOCK_HZ, PARITY_NONE, STOP_BIT_ONE);

    uart_dma_enable(1, 0);
    irq_set_mask(FLD_IRQ_DMA_EN);
    dma_chn_irq_enable(FLD_DMA_CHN_UART_RX, 1);
    uart_irq_enable(1, 0);

    myUartDriver.recvCb = uart_rx_cb;

    printf("uart_rx_init done\r\n");
}

/**********************************************************************
 * stack_init
 **********************************************************************/
void stack_init(void)
{
    zb_init();
    zb_zdoCbRegister((zdo_appIndCb_t *)&appCbLst);
}

/**********************************************************************
 * user_app_init — EP1~EP3 등록
 **********************************************************************/
void user_app_init(void)
{
    u8 i;
    u8 ep_list[] = {
        JUNTEK_ENDPOINT_ELEC,
        JUNTEK_ENDPOINT_TEMP,
        JUNTEK_ENDPOINT_RELAY,
        JUNTEK_ENDPOINT_METERING,
        JUNTEK_ENDPOINT_ANALOG,
    };
    const af_simple_descriptor_t *ep_desc[] = {
        &juntek_ep1_simpleDesc,
        &juntek_ep2_simpleDesc,
        &juntek_ep3_simpleDesc,
        &juntek_ep4_simpleDesc,
        &juntek_ep5_simpleDesc,
    };

    af_nodeDescManuCodeUpdate(MANUFACTURER_CODE_TELINK);

    zcl_init(juntek_zclProcessIncomingMsg);

    /* EP1~EP5 등록 */
    for (i = 0; i < JUNTEK_EP_COUNT; i++) {
        af_endpointRegister(ep_list[i],
                            (af_simple_descriptor_t *)ep_desc[i],
                            zcl_rx_handler, NULL);
    }

    juntek_attrs_init();

    /* EP별 클러스터 등록 — g_epClusterNum으로 EP별 개수 전달 */
    for (i = 0; i < JUNTEK_EP_COUNT; i++) {
        zcl_register(ep_list[i], g_epClusterNum[i],
                     (zcl_specClusterInfo_t *)g_epClusterList[i]);
    }

#if ZCL_GP_SUPPORT
    gp_init(JUNTEK_ENDPOINT_ELEC);
#endif
}

/**********************************************************************
 * app_task
 **********************************************************************/
void app_task(void)
{
    uart_rx_poll();
    app_key_handler();
    localPermitJoinState();

    if (BDB_STATE_GET() == BDB_STATE_IDLE) {
        report_handler();
    }
}

/**********************************************************************
 * sysException
 **********************************************************************/
static void bmsSysException(void)
{
    SYSTEM_RESET();
}

/**********************************************************************
 * user_init
 **********************************************************************/
void user_init(bool isRetention)
{
    (void)isRetention;

    sys_exceptHandlerRegister(bmsSysException);

    /* UART RX 초기화 */
    uart_rx_init();

    printf("user_init start\r\n");

    /* LED + 버튼 초기화 */
    juntek_hw_init();

    /* Button1 (PD4) GPIO 입력 + 풀업 */
    drv_gpio_func_set(BUTTON1);
    drv_gpio_input_en(BUTTON1, 1);
    drv_gpio_output_en(BUTTON1, 0);
    drv_gpio_up_down_resistor(BUTTON1, PM_PIN_PULLUP_10K);

    /* Zigbee 스택 초기화 */
    stack_init();

    /* 애플리케이션 초기화 */
    user_app_init();

#if ZBHCI_EN
    zbhciInit();
    ev_on_poll(EV_POLL_HCI, zbhciTask);
#endif

    ev_on_poll(EV_POLL_IDLE, app_task);

    if (bdb_preInstallCodeLoad(&gJuntekCtx.tcLinkKey.keyType,
                               gJuntekCtx.tcLinkKey.key) == RET_OK) {
        g_bdbCommissionSetting.linkKey.tcLinkKey.keyType =
            gJuntekCtx.tcLinkKey.keyType;
        g_bdbCommissionSetting.linkKey.tcLinkKey.key =
            gJuntekCtx.tcLinkKey.key;
    }

    bdb_init((af_simple_descriptor_t *)&juntek_ep1_simpleDesc,
             &g_bdbCommissionSetting, &g_zbDemoBdbCb, 1);

    /* EP1~EP5 리포팅 설정 — bdb_init() 이후에 호출해야 NV에 정상 저장됨 */
    {
        u8 reportableChange2[2] = {0x00, 0x01};  /* INT16 변화량 1 단위 */
        u8 reportableChange1[1] = {0x01};         /* BOOLEAN 변화량 1 단위 */
        u8 reportableChange6[6] = {0x01, 0x00, 0x00, 0x00, 0x00, 0x00}; /* UINT48 변화량 1 단위 */
        u8 reportableChangeF[4] = {0x00, 0x00, 0x80, 0x3F}; /* float 1.0f (IEEE754 LE) */

        /* EP1: Voltage / Current / Power */
        bdb_defaultReportingCfg(JUNTEK_ENDPOINT_ELEC, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE,
            5, 300, (u8*)reportableChange2);

        bdb_defaultReportingCfg(JUNTEK_ENDPOINT_ELEC, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,
            5, 300, (u8*)reportableChange2);

        bdb_defaultReportingCfg(JUNTEK_ENDPOINT_ELEC, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER,
            5, 300, (u8*)reportableChange2);

        /* EP2: Temperature (INT16, 0.01°C 단위) */
        bdb_defaultReportingCfg(JUNTEK_ENDPOINT_TEMP, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_TEMPERATURE_MEASUREMENT,
            ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MEAS_VAL,
            5, 300, (u8*)reportableChange2);

        /* EP3: Relay State (BOOLEAN) */
        bdb_defaultReportingCfg(JUNTEK_ENDPOINT_RELAY, HA_PROFILE_ID,
            ZCL_CLUSTER_GEN_BINARY_INPUT_BASIC,
            ZCL_ATTRID_BINARY_INPUT_PRESENT_VALUE,
            1, 300, (u8*)reportableChange1);

        /* EP4: Remain Ah (UINT48, mAh 단위) */
        bdb_defaultReportingCfg(JUNTEK_ENDPOINT_METERING, HA_PROFILE_ID,
            ZCL_CLUSTER_SE_METERING,
            ZCL_ATTRID_METERING_CURRENT_SUMMATION_DELIVERD,
            10, 300, (u8*)reportableChange6);

        /* EP5: Elapsed Minutes (SINGLE float) */
        bdb_defaultReportingCfg(JUNTEK_ENDPOINT_ANALOG, HA_PROFILE_ID,
            ZCL_CLUSTER_GEN_ANALOG_INPUT_BASIC,
            ZCL_ATTRID_ANALOG_INPUT_PRESENT_VALUE,
            30, 600, (u8*)reportableChangeF);
    }
}
