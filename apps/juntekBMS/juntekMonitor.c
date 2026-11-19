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

#if ZCL_OTA_SUPPORT
extern ota_callBack_t juntek_otaCb;   /* zcl_juntekCb.c 에서 정의됨 */
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
/* [수정] 128 → 256 : :r50 한 줄(70~100B)이 두 줄 연속 들어와도 DMA 한 번에 수용
 *        (기존 124B 초과 시 len 검사에서 묶음 전체가 버려졌음). 16 의 배수 유지 */
#ifndef UART_BUF_SIZE
#define UART_BUF_SIZE   256     /* 4(DMA 길이 헤더) + 252(데이터) */
#endif

volatile u8  uart_rx_buf[UART_BUF_SIZE] __attribute__((aligned(4))) = {0};

/* [추가] 수신 링버퍼 — IRQ 에서 DMA 버퍼 내용을 즉시 복사
 *  기존: IRQ 는 플래그만 세우고 메인 루프에서 DMA 버퍼를 읽음 → 메인 루프가
 *        Zigbee 처리로 늦어지면 다음 패킷이 같은 DMA 버퍼를 덮어써 데이터 손상.
 *  변경: IRQ 에서 링버퍼로 복사 + DMA 재설정, 메인 루프는 링버퍼만 소비.
 *  head 는 IRQ 만, tail 은 메인 루프만 갱신 (단일 생산자/단일 소비자) */
#define RX_RING_SIZE    512     /* 2 의 거듭제곱 */
#define RX_RING_MASK    (RX_RING_SIZE - 1)

static volatile u8  s_rx_ring[RX_RING_SIZE];
static volatile u16 s_rx_head = 0;
static volatile u16 s_rx_tail = 0;
static volatile u8  s_rx_overflow = 0;

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
        //printf("BMS parse: token count %d < 13\r\n", tcnt);
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

    DBG_LOG("BMS V=%d.%02d A=%c%d.%02d Ah=%d.%03d T=%d C relay=%s elapsed=%dmin\r\n",
           (int)out->voltage, (int)(out->voltage * 100) % 100,
           out->current < 0 ? '-' : '+',
           (int)(out->current < 0 ? -out->current : out->current),
           (int)((out->current < 0 ? -out->current : out->current) * 100) % 100,
           (int)out->remain_ah, (int)(out->remain_ah * 1000) % 1000,
           (int)out->temperature,
           out->relay ? "CHG" : "DCH",
           (int)out->elapsed_min);
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

/* [추가] 연속 spike 허용 횟수
 *  기존: spike 로 거부되면 last 가 갱신되지 않아, 부하가 실제로 크게 바뀐 경우
 *  (예: 인버터 ON 으로 0A → -150A) 이후 모든 샘플이 영구히 거부되어 값이 멈췄음.
 *  spike 가 연속 N 회 들어오면 실제 변화로 보고 필터를 새 값으로 다시 시작 */
#define BMS_SPIKE_ACCEPT_N  3

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
        DBG_LOG("BMS filter REJECT: V out of range %d.%02d\r\n",
               (int)d->voltage, (int)(d->voltage * 100) % 100);
        return false;
    }
    if (d->current < c->curr_min || d->current > c->curr_max) {
        DBG_LOG("BMS filter REJECT: A out of range\r\n");
        return false;
    }
    if (d->temperature < c->temp_min || d->temperature > c->temp_max) {
        DBG_LOG("BMS filter REJECT: T out of range\r\n");
        return false;
    }
    if (d->remain_ah < 0.0f || d->remain_ah > c->ah_max) {
        DBG_LOG("BMS filter REJECT: Ah out of range\r\n");
        return false;
    }

    /* --- 2단계: Rate Limit --- */
    if (f_voltage.initialized) {
        static u8 s_spikeCnt = 0;
        bool spike = false;

        if (bms_fabsf(d->voltage - f_voltage.last) > c->volt_rate) {
            DBG_LOG("BMS filter REJECT: V spike\r\n");
            spike = true;
        } else if (bms_fabsf(d->current - f_current.last) > c->curr_rate) {
            DBG_LOG("BMS filter REJECT: A spike\r\n");
            spike = true;
        } else if (bms_fabsf(d->temperature - f_temperature.last) > c->temp_rate) {
            DBG_LOG("BMS filter REJECT: T spike\r\n");
            spike = true;
        }

        if (spike) {
            if (++s_spikeCnt < BMS_SPIKE_ACCEPT_N) {
                return false;
            }
            /* 연속으로 범위 안의 '튄 값'이 계속 들어옴 → 실제 변화로 판단, 필터 재시작 */
            DBG_LOG("BMS filter: %d consecutive spikes -> accept, reset filter\r\n",
                    BMS_SPIKE_ACCEPT_N);
            memset(&f_voltage,     0, sizeof(f_voltage));
            memset(&f_current,     0, sizeof(f_current));
            memset(&f_temperature, 0, sizeof(f_temperature));
            memset(&f_remain_ah,   0, sizeof(f_remain_ah));
        }
        s_spikeCnt = 0;
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
 * [추가] RS485 통신 상태 판정
 *
 *  - 정상(OK)        : 마지막 정상 :r50 프레임 이후 RS485_TIMEOUT_SEC 미만
 *  - 파싱 오류        : 정상 프레임은 끊겼는데 데이터(바이트)는 계속 들어오고,
 *                      :r50 줄 파싱 실패가 RS485_PARSE_ERR_N 회 이상 연속
 *                      (배선 노이즈, 보레이트 불일치, 줄 잘림 등)
 *  - 수신 없음(TIMEOUT): 그 외 — RS485 로 아무것도 안 들어옴 (단선, BMS 꺼짐 등)
 *
 *  '정상 프레임' = 파싱 성공 기준. 필터(범위/튐)에서 거부된 값도 통신 자체는 정상이므로 포함.
 *  경과 시간은 1초 타이머로 직접 셈 (clock_time() 차이는 16MHz 에서 약 268초마다 넘쳐 사용 불가)
 **********************************************************************/
#define RS485_TIMEOUT_SEC       5     /* BMS 는 약 0.5초마다 송신 → 5초면 10프레임 누락 */
#define RS485_PARSE_ERR_N       3     /* 연속 파싱 실패 기준 */
#define RS485_REPORT_OK_SEC     60    /* 정상일 때 주기 보고 */
#define RS485_REPORT_BAD_SEC    10    /* 비정상일 때 주기 보고 (경과 시간 갱신용) */

static u16 s_rsOkAgeSec   = 0;   /* 마지막 정상 프레임 이후 초 */
static u16 s_rsRxIdleSec  = 0;   /* 마지막 수신 바이트 이후 초 */
static u8  s_rsConsecErr  = 0;   /* 마지막 정상 프레임 이후 연속 파싱 실패 */

static void rs485_on_frame_ok(void)
{
    s_rsOkAgeSec  = 0;
    s_rsConsecErr = 0;
}

static void rs485_on_parse_error(void)
{
    if (g_juntek_rs485Attrs.errCnt < 0xFFFF) g_juntek_rs485Attrs.errCnt++;
    if (s_rsConsecErr < 0xFF) s_rsConsecErr++;
}

/* 줄 안에 "r50" 이 있으면 :r50 응답으로 봄 (다른 명령 줄은 오류로 세지 않음) */
static bool line_is_r50(const char *s)
{
    for (; s[0] && s[1] && s[2]; s++) {
        if (s[0] == 'r' && s[1] == '5' && s[2] == '0') return TRUE;
    }
    return FALSE;
}

/* app_task 에서 1초마다 호출 */
void juntek_rs485_tick(void)
{
    static u16 s_reportElapsed = 0;

    if (s_rsOkAgeSec  < 0xFFFF) s_rsOkAgeSec++;
    if (s_rsRxIdleSec < 0xFFFF) s_rsRxIdleSec++;

    u8 st;
    if (s_rsOkAgeSec < RS485_TIMEOUT_SEC) {
        st = RS485_STATUS_OK;
    } else if (s_rsRxIdleSec < RS485_TIMEOUT_SEC && s_rsConsecErr >= RS485_PARSE_ERR_N) {
        st = RS485_STATUS_PARSE_ERROR;
    } else {
        st = RS485_STATUS_TIMEOUT;
    }

    u8 prev = g_juntek_rs485Attrs.status;
    g_juntek_rs485Attrs.status = st;
    g_juntek_rs485Attrs.rxAge  = s_rsOkAgeSec;

    s_reportElapsed++;
    u16 period = (st == RS485_STATUS_OK) ? RS485_REPORT_OK_SEC : RS485_REPORT_BAD_SEC;

    if (st != prev) {
        DBG_LOG("RS485 status %d -> %d (age=%ds, err=%d)\r\n",
                prev, st, s_rsOkAgeSec, g_juntek_rs485Attrs.errCnt);
        s_reportElapsed = 0;
        juntek_rs485_report();
    } else if (s_reportElapsed >= period) {
        s_reportElapsed = 0;
        juntek_rs485_report();
    }
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
 * uart_rx_cb — IRQ 컨텍스트
 * [수정] DMA 버퍼 → 링버퍼 복사 후 DMA 재설정 (메인 루프 지연과 무관하게 데이터 보존)
 **********************************************************************/
_attribute_ram_code_ void uart_rx_cb(void)
{
    u32 len = (u32)uart_rx_buf[0]
            | ((u32)uart_rx_buf[1] << 8)
            | ((u32)uart_rx_buf[2] << 16)
            | ((u32)uart_rx_buf[3] << 24);

    if (len > (UART_BUF_SIZE - 4)) {
        len = UART_BUF_SIZE - 4;        /* 비정상 길이 — 버퍼 범위까지만 */
        s_rx_overflow = 1;
    }

    u16 head = s_rx_head;
    for (u32 i = 0; i < len; i++) {
        u16 next = (head + 1) & RX_RING_MASK;
        if (next == s_rx_tail) {        /* 링버퍼 가득 참 — 나머지 버림 */
            s_rx_overflow = 1;
            break;
        }
        s_rx_ring[head] = uart_rx_buf[4 + i];
        head = next;
    }
    s_rx_head = head;

    /* 길이 헤더 초기화 + DMA 재설정 (기존 uart_rx_poll() 의 재설정을 IRQ 로 이동) */
    uart_rx_buf[0] = uart_rx_buf[1] = uart_rx_buf[2] = uart_rx_buf[3] = 0;
    uart_recbuff_init((u8 *)uart_rx_buf, UART_BUF_SIZE);
}

/**********************************************************************
 * uart_rx_poll — app_task() 에서 매 루프 호출
 *
 * 링버퍼 → 라인 버퍼 조립 → \n 수신 시 파싱
 **********************************************************************/
void uart_rx_poll(void)
{
    if (s_rx_overflow) {
        s_rx_overflow = 0;
        s_line_pos = 0;                 /* 잘린 줄은 버리고 다음 줄부터 */
        rs485_on_parse_error();         /* [추가] 잘린 줄 = 프레임 손실 */
        DBG_LOG("UART RX overflow -> line reset\r\n");
    }

    while (s_rx_tail != s_rx_head) {
        char c = (char)s_rx_ring[s_rx_tail];
        s_rx_tail = (s_rx_tail + 1) & RX_RING_MASK;
        s_rsRxIdleSec = 0;              /* [추가] 바이트 수신 = 선로는 살아 있음 */

        if (c == '\n' || c == '\r') {
            if (s_line_pos > 0) {
                s_line_buf[s_line_pos] = '\0';

                /* :r50 파싱 */
                juntek_data_t d = {0};
                juntek_parse_r50(s_line_buf, &d);

                if (d.valid) {
                    rs485_on_frame_ok();        /* [추가] RS485 정상 */
                    juntek_r50_process(&d);
                } else if (line_is_r50(s_line_buf)) {
                    rs485_on_parse_error();     /* [추가] :r50 줄인데 파싱 실패 */
                }

                s_line_pos = 0;
            }
        } else {
            if (s_line_pos < LINE_BUF_SIZE - 1) {
                s_line_buf[s_line_pos++] = c;
            } else {
                /* 라인 버퍼 오버플로 — 리셋 */
                s_line_pos = 0;
                rs485_on_parse_error();         /* [추가] 줄바꿈 없이 너무 긴 데이터 = 깨진 데이터 */
            }
        }
    }
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

    DBG_LOG("uart_rx_init done\r\n");
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
 * user_app_init — EP1~EP7 등록
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
        JUNTEK_ENDPOINT_DRVCHG,
        JUNTEK_ENDPOINT_CHGCUR,
    };
    const af_simple_descriptor_t *ep_desc[] = {
        &juntek_ep1_simpleDesc,
        &juntek_ep2_simpleDesc,
        &juntek_ep3_simpleDesc,
        &juntek_ep4_simpleDesc,
        &juntek_ep5_simpleDesc,
        &juntek_ep6_simpleDesc,
        &juntek_ep7_simpleDesc,
    };

    af_nodeDescManuCodeUpdate(MANUFACTURER_CODE_TELINK);

    zcl_init(juntek_zclProcessIncomingMsg);

    /* EP1~EP7 등록 */
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
    static u32 s_cfgCheckTick = 0;
    static u32 s_rsTick = 0;            /* [추가] RS485 상태 1초 타이머 */

    uart_rx_poll();
    app_key_handler();
    localPermitJoinState();

    /* [추가] 필터 설정 변경 감시 (Write 경로를 거치지 않은 변경 대비, 500ms 주기) */
    if (clock_time_exceed(s_cfgCheckTick, 500 * 1000)) {
        s_cfgCheckTick = clock_time();
        juntek_filter_cfg_check();
    }

    /* [추가] RS485 통신 상태 판정 + 보고 (1초 주기) */
    if (clock_time_exceed(s_rsTick, 1000 * 1000)) {
        s_rsTick = clock_time();
        juntek_rs485_tick();
    }

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

    DBG_LOG("user_init start\r\n");

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

#if ZCL_OTA_SUPPORT
    ota_init(OTA_TYPE_CLIENT, (af_simple_descriptor_t *)&juntek_ep1_simpleDesc,
             &juntek_otaInfo, &juntek_otaCb);
#endif

    /* EP1~EP7 리포팅 설정 — bdb_init() 이후에 호출해야 NV에 정상 저장됨 */
    {
        /* [수정] {0x00,0x01} 은 little-endian 으로 256 → 전압 2.56V 이상 바뀌어야 리포트됐음 */
        u8 reportableChange2[2] = {0x01, 0x00};  /* INT16/UINT16 변화량 1 단위 (LE) */
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

        /* [추가] 부호 있는 DC 전류 (INT16, 0.01A) */
        bdb_defaultReportingCfg(JUNTEK_ENDPOINT_ELEC, HA_PROFILE_ID,
            ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
            ZCL_ATTRID_ELECTRICAL_MEAS_DC_CURRENT,
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

        /* [추가] EP6/EP7: 릴레이 OnOff (BOOLEAN) — Z2M configure 전에도 주기 보고 */
        bdb_defaultReportingCfg(JUNTEK_ENDPOINT_DRVCHG, HA_PROFILE_ID,
            ZCL_CLUSTER_GEN_ON_OFF, ZCL_ATTRID_ONOFF,
            1, 300, (u8*)reportableChange1);

        bdb_defaultReportingCfg(JUNTEK_ENDPOINT_CHGCUR, HA_PROFILE_ID,
            ZCL_CLUSTER_GEN_ON_OFF, ZCL_ATTRID_ONOFF,
            1, 300, (u8*)reportableChange1);
    }
}
