/*********************************************************************
 * @file    epeverMonitor.c
 * @brief   ZT3L EPever Tracer AN MPPT 모니터 메인
 *
 * UART TX → Modbus RTU 요청 (Function 04, 0x3100~0x311A)
 * UART RX → Modbus RTU 응답 파싱 → epever_data_t
 * → ZCL 속성 업데이트 → Zigbee 리포팅
 *
 * EP 구성:
 *   EP1: Electrical Measurement (Solar V/I/W)
 *   EP2: Electrical Measurement (Battery V/I/W)
 *   EP3: Electrical Measurement (Load V/I/W)
 *   EP4: Temperature Measurement (Battery Temp)
 *   EP5: Analog Input (Battery SOC %)
 *
 * Modbus RTU 설정:
 *   Slave ID : 0x01
 *   Function : 0x04 (Read Input Registers)
 *   Start    : 0x3100
 *   Count    : 27 (0x3100~0x311A)
 *   Baudrate : 115200, 8N1
 *********************************************************************/

#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "gp.h"
#include "epeverMonitor.h"
#include "epeverCtrl.h"
#include "epever_ep.h"
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
app_ctx_t gEpeverCtx;

#ifdef ZCL_OTA
extern ota_callBack_t epever_otaCb;
ota_preamble_t epever_otaInfo = {
    .fileVer          = FILE_VERSION,
    .imageType        = IMAGE_TYPE,
    .manufacturerCode = MANUFACTURER_CODE_TELINK,
};
#endif

const zdo_appIndCb_t appCbLst = {
    bdb_zdoStartDevCnf,
    NULL,
    NULL,
    epever_leaveIndHandler,
    epever_leaveCnfHandler,
    epever_nwkUpdateIndicateHandler,
    NULL,
    NULL,
    NULL,
    NULL,
    epever_nwkStatusIndHandler,
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
 * UART RX 버퍼
 */
#ifndef UART_BUF_SIZE
#define UART_BUF_SIZE   128
#endif

volatile u8  uart_rx_buf[UART_BUF_SIZE] __attribute__((aligned(4))) = {0};
volatile u8  uart_rx_flag = 0;

extern drv_uart_t myUartDriver;

/**********************************************************************
 * Modbus RTU 설정
 */
#define EP_MODBUS_SLAVE_ID      0x01
#define EP_MODBUS_FUNC_READ     0x04
#define EP_READ_START_ADDR      0x3100
#define EP_READ_COUNT           27          /* 0x3100 ~ 0x311A */
#define EP_POLL_INTERVAL_MS     10000       /* 10초 주기       */
#define EP_RSP_TIMEOUT_MS       500         /* 응답 타임아웃   */

/**********************************************************************
 * CRC16 (Modbus)
 */
static u16 modbus_crc16(const u8 *buf, u16 len)
{
    u16 crc = 0xFFFF;
    u16 i;
    u8  j;
    for (i = 0; i < len; i++) {
        crc ^= buf[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x0001) crc = (crc >> 1) ^ 0xA001;
            else               crc >>= 1;
        }
    }
    return crc;
}

/**********************************************************************
 * Modbus RTU 요청 프레임 빌드
 * [ID][FC][AddrH][AddrL][CntH][CntL][CRCL][CRCH]
 */
static void modbus_build_request(u8 *buf, u8 slave_id, u8 func,
                                  u16 addr, u16 count)
{
    buf[0] = slave_id;
    buf[1] = func;
    buf[2] = (addr >> 8) & 0xFF;
    buf[3] =  addr       & 0xFF;
    buf[4] = (count >> 8) & 0xFF;
    buf[5] =  count       & 0xFF;
    u16 crc = modbus_crc16(buf, 6);
    buf[6] = crc & 0xFF;
    buf[7] = (crc >> 8) & 0xFF;
}

/**********************************************************************
 * Modbus RTU 응답 파서
 *
 * 응답: [ID][FC][ByteCnt][D0H][D0L]...[CRCL][CRCH]
 */
static bool epever_parse_response(const u8 *buf, u16 buf_len,
                                   epever_data_t *out)
{
    out->valid = false;

    /* 최소 길이: 3(헤더) + EP_READ_COUNT×2 + 2(CRC) */
    u16 expected = 3 + EP_READ_COUNT * 2 + 2;
    if (buf_len < expected) return false;

    if (buf[0] != EP_MODBUS_SLAVE_ID) return false;
    if (buf[1] != EP_MODBUS_FUNC_READ) return false;
    if (buf[2] != EP_READ_COUNT * 2)   return false;

    /* CRC 검증 */
    u16 crc_calc = modbus_crc16(buf, expected - 2);
    u16 crc_recv = ((u16)buf[expected - 1] << 8) | buf[expected - 2];
    if (crc_calc != crc_recv) return false;

    /* 레지스터 추출 — buf[3] 부터 2바이트씩 (Big-endian) */
    const u8 *d = buf + 3;

    #define REG(offset)  ((u16)(((d)[(offset)*2] << 8) | (d)[(offset)*2+1]))
    #define OFFSET(addr) ((addr) - EP_READ_START_ADDR)

    out->solar_volt = REG(OFFSET(0x3100)) / 100.0f;
    out->solar_curr = REG(OFFSET(0x3101)) / 100.0f;

    /* 32bit 전력: H×65536 + L, ÷100 = W */
    u32 sol_pow = ((u32)REG(OFFSET(0x3103)) << 16) | REG(OFFSET(0x3102));
    out->solar_pow  = sol_pow / 100.0f;

    out->bat_volt   = REG(OFFSET(0x3104)) / 100.0f;
    out->bat_curr   = REG(OFFSET(0x3105)) / 100.0f;

    u32 chg_pow = ((u32)REG(OFFSET(0x3107)) << 16) | REG(OFFSET(0x3106));
    out->chg_pow    = chg_pow / 100.0f;

    out->load_volt  = REG(OFFSET(0x310C)) / 100.0f;
    out->load_curr  = REG(OFFSET(0x310D)) / 100.0f;

    u32 load_pow = ((u32)REG(OFFSET(0x310F)) << 16) | REG(OFFSET(0x310E));
    out->load_pow   = load_pow / 100.0f;

    out->bat_temp   = REG(OFFSET(0x3110)) / 100.0f;
    out->dev_temp   = REG(OFFSET(0x3111)) / 100.0f;
    out->bat_soc    = (u8)REG(OFFSET(0x311A));

    #undef REG
    #undef OFFSET

    out->valid = true;
    return true;
}

/* forward declaration */
static s32 epever_rated_start_cb(void *arg);

/**********************************************************************
 * Rated Datum 레지스터 설정
 * 0x3000~0x300F: 장비 정격 정보 (부팅 시 1회만 읽기)
 */
#define EP_RATED_START_ADDR     0x3000
#define EP_RATED_COUNT          15      /* 0x3000~0x300E */

/**********************************************************************
 * 상태 머신
 * RATED_REQ  : 부팅 시 Rated Datum 요청 송신
 * RATED_WAIT : Rated Datum 응답 대기
 * IDLE       : 대기
 * WAIT_RSP   : 실시간 데이터 응답 대기
 */
typedef enum {
    EP_STATE_RATED_REQ = 0, /* 부팅 시 Rated Datum 요청 */
    EP_STATE_RATED_WAIT,    /* Rated Datum 응답 대기    */
    EP_STATE_IDLE,          /* 대기                     */
    EP_STATE_WAIT_RSP,      /* 실시간 데이터 응답 대기  */
} ep_modbus_state_t;

static ep_modbus_state_t s_ep_state      = EP_STATE_RATED_REQ;
static u32               s_req_time      = 0;
static bool              s_rated_done    = false; /* Rated Datum 읽기 완료 여부 */

/**********************************************************************
 * Rated Datum 파서
 * 0x3000: Array Rated Voltage  ÷100 = V
 * 0x3001: Array Rated Current  ÷100 = A
 * 0x3002~3003: Array Rated Power (32bit) ÷100 = W
 * 0x3004: Battery Rated Voltage ÷100 = V
 * 0x3005: Battery Rated Current ÷100 = A
 * 0x3006~3007: Battery Rated Power (32bit) ÷100 = W
 * 0x300E: Rated Load Current   ÷100 = A
 */
static void epever_parse_rated(const u8 *buf, u16 buf_len)
{
    /* 최소 길이: 3(헤더) + EP_RATED_COUNT×2 + 2(CRC) */
    u16 expected = 3 + EP_RATED_COUNT * 2 + 2;
    if (buf_len < expected) return;

    if (buf[0] != EP_MODBUS_SLAVE_ID) return;
    if (buf[1] != EP_MODBUS_FUNC_READ) return;
    if (buf[2] != EP_RATED_COUNT * 2)  return;

    u16 crc_calc = modbus_crc16(buf, expected - 2);
    u16 crc_recv = ((u16)buf[expected - 1] << 8) | buf[expected - 2];
    if (crc_calc != crc_recv) return;

    const u8 *d = buf + 3;
    #define REG(n) ((u16)((d[(n)*2] << 8) | d[(n)*2+1]))

    float array_rated_volt = REG(0) / 100.0f;   /* 0x3000 */
    float array_rated_curr = REG(1) / 100.0f;   /* 0x3001 */
    u32   array_rated_pow  = ((u32)REG(3) << 16) | REG(2); /* 0x3002~3003 */
    float bat_rated_volt   = REG(4) / 100.0f;   /* 0x3004 */
    float bat_rated_curr   = REG(5) / 100.0f;   /* 0x3005 */
    float load_rated_curr  = REG(14) / 100.0f;  /* 0x300E */

    #undef REG

    printf("EPever Rated: PV=%.1fV/%.1fA/%.1fW Bat=%.1fV/%.1fA Load=%.1fA\r\n",
           array_rated_volt, array_rated_curr, array_rated_pow / 100.0f,
           bat_rated_volt, bat_rated_curr, load_rated_curr);

    /* ZCL Basic 클러스터 modelId에 정격 정보 기록
     * 형식: "PV:XXV/XXA BAT:XXV/XXA LD:XXA"
     * ZCL_BASIC_MAX_LENGTH = 32 (첫 바이트는 길이) */
    char info[31];
    /* TC32은 %f 미지원 — 정수로 변환해서 출력 */
    int pv_v  = (int)(array_rated_volt + 0.5f);
    int pv_a  = (int)(array_rated_curr + 0.5f);
    int pv_w  = (int)(array_rated_pow / 100);
    int bat_v = (int)(bat_rated_volt + 0.5f);
    int bat_a = (int)(bat_rated_curr + 0.5f);
    int ld_a  = (int)(load_rated_curr + 0.5f);

    /* snprintf 대신 수동 문자열 조합 (TC32 호환) */
    u8 pos = 0;
    info[pos++] = 'P'; info[pos++] = 'V'; info[pos++] = ':';
    /* pv_w 값으로 컨트롤러 정격 표현 (예: 40W→"40W") */
    if (pv_w >= 100) { info[pos++] = '0' + pv_w / 100; }
    if (pv_w >= 10)  { info[pos++] = '0' + (pv_w % 100) / 10; }
    info[pos++] = '0' + pv_w % 10;
    info[pos++] = 'W'; info[pos++] = ' ';
    info[pos++] = 'B'; info[pos++] = 'A'; info[pos++] = 'T'; info[pos++] = ':';
    if (bat_v >= 10) { info[pos++] = '0' + bat_v / 10; }
    info[pos++] = '0' + bat_v % 10;
    info[pos++] = 'V'; info[pos++] = '/';
    if (bat_a >= 10) { info[pos++] = '0' + bat_a / 10; }
    info[pos++] = '0' + bat_a % 10;
    info[pos++] = 'A'; info[pos++] = ' ';
    info[pos++] = 'L'; info[pos++] = 'D'; info[pos++] = ':';
    if (ld_a >= 10) { info[pos++] = '0' + ld_a / 10; }
    info[pos++] = '0' + ld_a % 10;
    info[pos++] = 'A';
    info[pos]   = '\0';

    /* ZCL Pascal 문자열 형식으로 저장 (첫 바이트 = 길이) */
    g_zcl_basicAttrs.modelId[0] = pos;
    memcpy(&g_zcl_basicAttrs.modelId[1], info, pos);

    /* swBuildId에 PV 전압/전류 정보 추가 */
    char pv_info[31];
    u8   pi = 0;
    pv_info[pi++] = 'P'; pv_info[pi++] = 'V'; pv_info[pi++] = ':';
    if (pv_v >= 10) { pv_info[pi++] = '0' + pv_v / 10; }
    pv_info[pi++] = '0' + pv_v % 10;
    pv_info[pi++] = 'V'; pv_info[pi++] = '/';
    if (pv_a >= 10) { pv_info[pi++] = '0' + pv_a / 10; }
    pv_info[pi++] = '0' + pv_a % 10;
    pv_info[pi++] = 'A'; pv_info[pi++] = '/';
    if (pv_w >= 100) { pv_info[pi++] = '0' + pv_w / 100; }
    if (pv_w >= 10)  { pv_info[pi++] = '0' + (pv_w % 100) / 10; }
    pv_info[pi++] = '0' + pv_w % 10;
    pv_info[pi++] = 'W';
    pv_info[pi]   = '\0';

    g_zcl_basicAttrs.swBuildId[0] = pi;
    memcpy(&g_zcl_basicAttrs.swBuildId[1], pv_info, pi);

    s_rated_done = true;
}

/**********************************************************************
 * epever_rated_request_send — Rated Datum 요청 송신
 */
static void epever_rated_request_send(void)
{
    /* uart_dma_send() 는 앞 4바이트가 length 헤더 */
    u8 buf[12];
    buf[0] = 8; buf[1] = 0; buf[2] = 0; buf[3] = 0;  /* len=8 */
    modbus_build_request(buf + 4, EP_MODBUS_SLAVE_ID, EP_MODBUS_FUNC_READ,
                          EP_RATED_START_ADDR, EP_RATED_COUNT);

    uart_dma_send(buf);

    s_ep_state = EP_STATE_RATED_WAIT;
    s_req_time = clock_time();
    {
        u8 *req = buf + 4;
        u16 addr = (u16)((req[2] << 8) | req[3]);
        u16 cnt  = (u16)((req[4] << 8) | req[5]);
        printf("[485 TX] Rated: ID=%d FC=%d Addr=%d Cnt=%d CRC=%d %d\r\n",
               (int)req[0], (int)req[1],
               (int)addr, (int)cnt,
               (int)req[6], (int)req[7]);
    }
}

/**********************************************************************
 * epever_request_send — 실시간 데이터 Modbus 요청 송신
 */
static void epever_request_send(void)
{
    u8 buf[12];
    buf[0] = 8; buf[1] = 0; buf[2] = 0; buf[3] = 0;  /* len=8 */
    modbus_build_request(buf + 4, EP_MODBUS_SLAVE_ID, EP_MODBUS_FUNC_READ,
                          EP_READ_START_ADDR, EP_READ_COUNT);

    uart_dma_send(buf);

    s_ep_state = EP_STATE_WAIT_RSP;
    s_req_time = clock_time();
    {
        u8 *req = buf + 4;
        u16 addr = (u16)((req[2] << 8) | req[3]);
        u16 cnt  = (u16)((req[4] << 8) | req[5]);
        printf("[485 TX] Poll: ID=%d FC=%d Addr=%d Cnt=%d CRC=%d %d\r\n",
               (int)req[0], (int)req[1],
               (int)addr, (int)cnt,
               (int)req[6], (int)req[7]);
    }
}

/**********************************************************************
 * 폴링 타이머 콜백 — EP_POLL_INTERVAL_MS 마다 호출
 */
static s32 epever_poll_timer_cb(void *arg)
{
    (void)arg;

    if (s_ep_state != EP_STATE_IDLE) {
        printf("[485] prev rsp not done (state=%d), force idle & retry\r\n",
               (int)s_ep_state);
        s_ep_state = EP_STATE_IDLE;
        uart_rx_flag = 0;
        uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
    }

    epever_request_send();
    return EP_POLL_INTERVAL_MS;
}

/**********************************************************************
 * uart_rx_cb — IRQ 컨텍스트, 반드시 RAM에 위치해야 함
 */
_attribute_ram_code_ void uart_rx_cb(void)
{
    uart_rx_flag = 1;
}

/**********************************************************************
 * uart_rx_poll — app_task()에서 매 루프 호출
 */
void uart_rx_poll(void)
{
    if (!uart_rx_flag) return;

    u16  len  = ((u16)uart_rx_buf[1] << 8) | uart_rx_buf[0];
    u8  *data = (u8 *)(uart_rx_buf + 4);

    /* Rated Datum 응답 처리 */
    if (s_ep_state == EP_STATE_RATED_WAIT) {
        u32 elapsed = (u32)((clock_time() - s_req_time)
                            / CLOCK_16M_SYS_TIMER_CLK_1MS);
        if (elapsed > EP_RSP_TIMEOUT_MS) {
            printf("[485 RX] Rated rsp timeout (%d ms), retry\r\n",
                   (int)elapsed);
            s_ep_state = EP_STATE_RATED_REQ;
            uart_rx_flag = 0;
            uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
            return;
        }

        printf("[485 RX] Rated rsp: len=%d elapsed=%d ms\r\n",
               (int)len, (int)elapsed);
        uart_rx_flag = 0;
        if (len > 0 && len <= (UART_BUF_SIZE - 4)) {
            epever_parse_rated(data, len);
        }
        s_ep_state = EP_STATE_IDLE;
        uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);

        /* Rated Datum 완료 → 폴링 타이머 시작 */
        if (s_rated_done) {
            printf("[485] Rated done -> start poll timer\r\n");
            TL_ZB_TIMER_SCHEDULE(epever_poll_timer_cb, NULL, 1000);
        }
        return;
    }

    /* 실시간 데이터 응답 처리 */
    if (s_ep_state != EP_STATE_WAIT_RSP) {
        uart_rx_flag = 0;
        uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
        return;
    }

    u32 elapsed = (u32)((clock_time() - s_req_time)
                        / CLOCK_16M_SYS_TIMER_CLK_1MS);
    if (elapsed > EP_RSP_TIMEOUT_MS) {
        printf("[485 RX] Poll rsp timeout (%d ms)\r\n", (int)elapsed);
        s_ep_state = EP_STATE_IDLE;
        uart_rx_flag = 0;
        uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
        return;
    }

    uart_rx_flag = 0;

    if (len == 0 || len > (UART_BUF_SIZE - 4)) {
        printf("[485 RX] Bad len=%d, discard\r\n", (int)len);
        s_ep_state = EP_STATE_IDLE;
        uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
        return;
    }

    printf("[485 RX] Poll rsp: len=%d elapsed=%d ms\r\n",
           (int)len, (int)elapsed);

    epever_data_t d = {0};
    if (epever_parse_response(data, len, &d)) {
        epever_attrs_update(&d);
        /* TC32: %f 미지원 → 정수 변환 출력 */
        printf("[485 RX] OK  Sol=%d.%02dV/%d.%02dA/%dW"
               "  Bat=%d.%02dV/%d.%02dA/%dW  SOC=%d%%"
               "  Ld=%d.%02dV/%d.%02dA/%dW  BatT=%d.%02dC\r\n",
               (int)d.solar_volt,  (int)((d.solar_volt  - (int)d.solar_volt)  * 100),
               (int)d.solar_curr,  (int)((d.solar_curr  - (int)d.solar_curr)  * 100),
               (int)d.solar_pow,
               (int)d.bat_volt,    (int)((d.bat_volt    - (int)d.bat_volt)    * 100),
               (int)d.bat_curr,    (int)((d.bat_curr    - (int)d.bat_curr)    * 100),
               (int)d.chg_pow,
               (int)d.bat_soc,
               (int)d.load_volt,   (int)((d.load_volt   - (int)d.load_volt)   * 100),
               (int)d.load_curr,   (int)((d.load_curr   - (int)d.load_curr)   * 100),
               (int)d.load_pow,
               (int)d.bat_temp,    (int)((d.bat_temp    - (int)d.bat_temp)    * 100));
    } else {
        printf("[485 RX] Parse FAIL len=%d  hdr=%d %d %d\r\n",
               (int)len,
               len > 0 ? (int)data[0] : 0,
               len > 1 ? (int)data[1] : 0,
               len > 2 ? (int)data[2] : 0);
    }

    s_ep_state = EP_STATE_IDLE;
    uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
}

/**********************************************************************
 * uart_init — UART TX/RX + DMA 초기화
 */
static void uart_init_epever(void)
{
    /* SDK drv_uart.c 의 8258 표준 설정과 동일하게 맞춤
     * TX/RX 모두 DMA 방식 */
    uart_gpio_set(UART_TX_PIN, UART_RX_PIN);
    uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
    uart_init_baudrate(115200, CLOCK_SYS_CLOCK_HZ, PARITY_NONE, STOP_BIT_ONE);

    uart_dma_enable(1, 1);  /* TX DMA=1, RX DMA=1 */
    irq_set_mask(FLD_IRQ_DMA_EN);
    dma_chn_irq_enable(FLD_DMA_CHN_UART_RX | FLD_DMA_CHN_UART_TX, 1);
    uart_irq_enable(0, 0);  /* UART IRQ 불필요 — DMA IRQ로 처리 */

    myUartDriver.recvCb = uart_rx_cb;
}

/**********************************************************************
 * epever_report_tick — 주기적 리포팅 (BDB IDLE 상태에서 호출)
 * SDK의 report_handler()와 이름 충돌 방지를 위해 별도 이름 사용
 */
static void epever_report_tick(void)
{
    /* bdb_defaultReportingCfg 로 설정된 자동 리포팅은 SDK 내부에서 처리됨 */
    (void)0;
}

/**********************************************************************
 * stack_init
 */
static void stack_init(void)
{
    zb_init();
    zb_zdoCbRegister((zdo_appIndCb_t *)&appCbLst);
}

/**********************************************************************
 * user_app_init — EP1~EP5 등록
 */
static void user_app_init(void)
{
    u8 i;
    u8 ep_list[] = {
        EPEVER_ENDPOINT_SOLAR,
        EPEVER_ENDPOINT_BATTERY,
        EPEVER_ENDPOINT_LOAD,
        EPEVER_ENDPOINT_TEMP,
        EPEVER_ENDPOINT_SOC,
    };
    const af_simple_descriptor_t *ep_desc[] = {
        &epever_ep1_simpleDesc,
        &epever_ep2_simpleDesc,
        &epever_ep3_simpleDesc,
        &epever_ep4_simpleDesc,
        &epever_ep5_simpleDesc,
    };

    af_nodeDescManuCodeUpdate(MANUFACTURER_CODE_TELINK);
    zcl_init(epever_zclProcessIncomingMsg);

    for (i = 0; i < EPEVER_EP_COUNT; i++) {
        af_endpointRegister(ep_list[i],
                            (af_simple_descriptor_t *)ep_desc[i],
                            zcl_rx_handler, NULL);
    }

    epever_attrs_init();

    for (i = 0; i < EPEVER_EP_COUNT; i++) {
        zcl_register(ep_list[i], g_epClusterNum[i],
                     (zcl_specClusterInfo_t *)g_epClusterList[i]);
    }

#if ZCL_GP_SUPPORT
    gp_init(EPEVER_ENDPOINT_SOLAR);
#endif
}

/**********************************************************************
 * epever_modbus_timeout_poll
 * app_task()에서 매 루프 호출 — RX 없이도 타임아웃/재시도 처리
 */
static void epever_modbus_timeout_poll(void)
{
    /* RX 수신 중이면 uart_rx_poll()에서 처리 */
    if (uart_rx_flag) return;
    /* 요청 송신 전이면 무시 */
    if (s_req_time == 0) return;

    u32 elapsed = (u32)((clock_time() - s_req_time)
                        / CLOCK_16M_SYS_TIMER_CLK_1MS);

    if (s_ep_state == EP_STATE_RATED_WAIT) {
        if (elapsed > EP_RSP_TIMEOUT_MS) {
            printf("[485] Rated timeout (%d ms) -> retry\r\n", (int)elapsed);
            s_ep_state = EP_STATE_RATED_REQ;
            uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
            /* 1초 후 재시도 */
            TL_ZB_TIMER_SCHEDULE(epever_rated_start_cb, NULL, 1000);
        }
    } else if (s_ep_state == EP_STATE_WAIT_RSP) {
        if (elapsed > EP_RSP_TIMEOUT_MS) {
            printf("[485] Poll timeout (%d ms)\r\n", (int)elapsed);
            s_ep_state = EP_STATE_IDLE;
            uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
        }
    }
}

/**********************************************************************
 * app_task
 */
void app_task(void)
{
    uart_rx_poll();
    epever_modbus_timeout_poll();
    app_key_handler();
    localPermitJoinState();

    if (BDB_STATE_GET() == BDB_STATE_IDLE) {
        epever_report_tick();
    }
}

/**********************************************************************
 * sysException
 */
static void epeverSysException(void)
{
    SYSTEM_RESET();
}

/**********************************************************************
 * user_init
 */
void user_init(bool isRetention)
{
    (void)isRetention;

    sys_exceptHandlerRegister(epeverSysException);

    /* ----------------------------------------------------------------
     * [BOOT] 부팅 로그 — debug TX (PD7) 는 drv_hw_init()이 처리하므로
     * printf는 바로 사용 가능
     * ---------------------------------------------------------------- */
    printf("\r\n");
    printf("========================================\r\n");
    printf(" EPever MPPT Monitor ZT3L\r\n");
    printf(" App v%d.%d  Stack v%d.%d\r\n",
           (int)((APP_RELEASE >> 4) & 0xF), (int)(APP_RELEASE & 0xF),
           (int)((STACK_RELEASE >> 4) & 0xF), (int)(STACK_RELEASE & 0xF));
    printf(" Clock: %dMHz\r\n",
           (int)(CLOCK_SYS_CLOCK_HZ / 1000000));
    printf(" RS485 115200 8N1 ID=%d\r\n",
           (int)EP_MODBUS_SLAVE_ID);
    printf(" Poll=%dms Ret=%d\r\n",
           (int)EP_POLL_INTERVAL_MS, (int)isRetention);
    printf("========================================\r\n");

    /* LED + 버튼 초기화 */
    epever_hw_init();

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

    if (bdb_preInstallCodeLoad(&gEpeverCtx.tcLinkKey.keyType,
                               gEpeverCtx.tcLinkKey.key) == RET_OK) {
        g_bdbCommissionSetting.linkKey.tcLinkKey.keyType =
            gEpeverCtx.tcLinkKey.keyType;
        g_bdbCommissionSetting.linkKey.tcLinkKey.key =
            gEpeverCtx.tcLinkKey.key;
    }

    bdb_init((af_simple_descriptor_t *)&epever_ep1_simpleDesc,
             &g_bdbCommissionSetting, &g_zbDemoBdbCb, 1);

    /* RS485 UART 초기화 — bdb_init/zb_init 이후에 해야 GPIO 덮어쓰기 방지 */
    uart_init_epever();

    printf("[BOOT] ZB stack init done. joined=%d\r\n",
           (int)zb_isDeviceJoinedNwk());

    /* EP1~EP5 리포팅 설정 */
    {
        u8 reportableChange2[2] = {0x00, 0x01};
        u8 reportableChangeF[4] = {0x00, 0x00, 0x80, 0x3F}; /* float 1.0f */

        /* EP1: Solar V/I/W */
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_SOLAR, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE, 10, 600, reportableChange2);
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_SOLAR, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT, 10, 600, reportableChange2);
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_SOLAR, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER, 10, 600, reportableChange2);

        /* EP2: Battery V/I/W */
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_BATTERY, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE, 10, 600, reportableChange2);
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_BATTERY, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT, 10, 600, reportableChange2);
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_BATTERY, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER, 10, 600, reportableChange2);

        /* EP3: Load V/I/W */
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_LOAD, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE, 10, 600, reportableChange2);
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_LOAD, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT, 10, 600, reportableChange2);
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_LOAD, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER, 10, 600, reportableChange2);

        /* EP4: Battery Temperature */
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_TEMP, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_TEMPERATURE_MEASUREMENT,
            ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MEAS_VAL, 10, 600, reportableChange2);

        /* EP5: Battery SOC */
        bdb_defaultReportingCfg(EPEVER_ENDPOINT_SOC, HA_PROFILE_ID,
            ZCL_CLUSTER_GEN_ANALOG_INPUT_BASIC,
            ZCL_ATTRID_ANALOG_INPUT_PRESENT_VALUE, 30, 600, reportableChangeF);
    }

    /* 부팅 시 Rated Datum 먼저 읽기 — 3초 후 요청 송신
     * Rated Datum 응답 수신 완료 후 실시간 폴링 타이머 자동 시작 */
    printf("[BOOT] Rated datum request scheduled (3s)\r\n");
    TL_ZB_TIMER_SCHEDULE(epever_rated_start_cb, NULL, 3000);
}

/**********************************************************************
 * epever_rated_start_cb — 부팅 3초 후 Rated Datum 요청 시작
 */
static s32 epever_rated_start_cb(void *arg)
{
    (void)arg;
    epever_rated_request_send();
    return -1; /* 1회 실행 후 종료 */
}
