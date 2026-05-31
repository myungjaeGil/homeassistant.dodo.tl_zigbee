/*********************************************************************
 * @file    ir_recv.c
 * @brief   NEC IR 수신 — GPIO_PA0 소프트웨어 디코딩
 *
 * 동작 방식:
 *   - PA0를 입력 + GPIO 엣지 인터럽트로 설정
 *   - 인터럽트 핸들러에서 RISING/FALLING 간격을 clock_time()으로 측정
 *   - 32비트 수집 완료 후 addr/cmd 검증 → ir_recv_on_cmd() 콜백
 *   - ir_recv_poll()은 타임아웃 감지용 (메인 루프)
 *
 * TLSR8258 GPIO 인터럽트:
 *   gpio_set_interrupt(pin, pol)  — pol: 0=falling, 1=rising
 *   gpio_en_interrupt(pin, en)    — 인터럽트 활성화
 *   irq_handler.c 에 gpio_irq_handler() 에서 호출
 *********************************************************************/

#include "tl_common.h"
#include "ir_recv.h"
#include "pwm5ch_ep.h"      /* g_pwmChAttrs */
#include "pwm5ch_ctrl.h"    /* led_power_set_state */

/*====================================================================
 * NEC 타이밍 상수 (단위: us)
 *==================================================================*/
#define NEC_LEADER_LOW_MIN   8000
#define NEC_LEADER_LOW_MAX  10000
#define NEC_LEADER_HIGH_MIN  4000
#define NEC_LEADER_HIGH_MAX  5000
#define NEC_REPEAT_HIGH_MIN  2000
#define NEC_REPEAT_HIGH_MAX  2500
#define NEC_BIT_LOW_MIN       400
#define NEC_BIT_LOW_MAX       750
#define NEC_BIT_ONE_MIN      1500
#define NEC_BIT_ONE_MAX      1900
#define NEC_BIT_ZERO_MIN      400
#define NEC_BIT_ZERO_MAX      750
#define NEC_TIMEOUT_US       15000   /* 프레임 타임아웃 */

/*====================================================================
 * 내부 상태머신
 *==================================================================*/
typedef enum {
    IR_STATE_IDLE = 0,
    IR_STATE_LEADER_LOW,     /* 9ms LOW 수신 중 */
    IR_STATE_LEADER_HIGH,    /* 4.5ms HIGH 수신 중 */
    IR_STATE_DATA,           /* 32비트 데이터 수신 중 */
    IR_STATE_DATA_HIGH,      /* 비트 HIGH 간격 측정 중 */
} ir_state_t;

static volatile ir_state_t s_state     = IR_STATE_IDLE;
static volatile u32        s_last_time = 0;   /* 마지막 엣지 clock_time() */
static volatile u32        s_bits      = 0;   /* 수집 중인 비트 */
static volatile u8         s_bit_cnt   = 0;   /* 수집된 비트 수 */
static volatile u32        s_low_time  = 0;   /* 직전 LOW 구간 시작 */

/* 수신 완료 버퍼 */
static volatile u8  s_cmd       = IR_CMD_NONE;
static volatile u8  s_addr      = 0;
static volatile bool s_repeat   = FALSE;
static volatile bool s_ready    = FALSE;

/*====================================================================
 * 시간 변환 헬퍼
 *==================================================================*/
static inline u32 ticks_to_us(u32 ticks)
{
    /* CLOCK_SYS_CLOCK_HZ = 48MHz → 1tick = 1/48 us
     * ticks / 48 = us */
    return ticks / (CLOCK_SYS_CLOCK_HZ / 1000000);
}

static inline bool in_range(u32 us, u32 min, u32 max)
{
    return (us >= min && us <= max);
}

/*====================================================================
 * GPIO 엣지 인터럽트 핸들러
 * irq_handler.c 의 gpio_irq_handler() 에서 호출:
 *   if (reg_irq_src & FLD_IRQ_GPIO_EN) { ir_recv_gpio_irq(); }
 *==================================================================*/
void ir_recv_gpio_irq(void)
{
    u32 now  = clock_time();
    u8  level = gpio_read(IR_RECV_PIN);  /* 현재 핀 레벨 */
    u32 dt_ticks;
    u32 dt_us;

    dt_ticks = (now >= s_last_time) ? (now - s_last_time)
                                    : (0xFFFFFFFF - s_last_time + now + 1);
    dt_us = ticks_to_us(dt_ticks);
    s_last_time = now;

    switch (s_state) {

    /* ── IDLE: FALLING 엣지 → Leader LOW 시작 ── */
    case IR_STATE_IDLE:
        if (!level) {  /* FALLING */
            s_state    = IR_STATE_LEADER_LOW;
            s_last_time = now;
            s_bits     = 0;
            s_bit_cnt  = 0;
        }
        break;

    /* ── Leader LOW → RISING 엣지: 구간 검증 ── */
    case IR_STATE_LEADER_LOW:
        if (level) {  /* RISING */
            if (in_range(dt_us, NEC_LEADER_LOW_MIN, NEC_LEADER_LOW_MAX)) {
                s_state = IR_STATE_LEADER_HIGH;
            } else {
                s_state = IR_STATE_IDLE;
            }
        }
        break;

    /* ── Leader HIGH → FALLING 엣지: 4.5ms or repeat 2.25ms ── */
    case IR_STATE_LEADER_HIGH:
        if (!level) {  /* FALLING */
            if (in_range(dt_us, NEC_LEADER_HIGH_MIN, NEC_LEADER_HIGH_MAX)) {
                /* 정상 프레임 시작 */
                s_state   = IR_STATE_DATA;
                s_low_time = now;
            } else if (in_range(dt_us, NEC_REPEAT_HIGH_MIN, NEC_REPEAT_HIGH_MAX)) {
                /* Repeat 코드 */
                s_repeat = TRUE;
                s_ready  = TRUE;
                s_state  = IR_STATE_IDLE;
            } else {
                s_state = IR_STATE_IDLE;
            }
        }
        break;

    /* ── Data: RISING 엣지 → LOW 구간 검증 ── */
    case IR_STATE_DATA:
        if (level) {  /* RISING — LOW 구간 종료 */
            if (in_range(dt_us, NEC_BIT_LOW_MIN, NEC_BIT_LOW_MAX)) {
                s_state = IR_STATE_DATA_HIGH;
            } else {
                s_state = IR_STATE_IDLE;
            }
        }
        break;

    /* ── Data HIGH: FALLING 엣지 → 비트 결정 ── */
    case IR_STATE_DATA_HIGH:
        if (!level) {  /* FALLING — HIGH 구간 종료 */
            if (in_range(dt_us, NEC_BIT_ONE_MIN, NEC_BIT_ONE_MAX)) {
                /* 비트 '1' */
                s_bits |= (1u << s_bit_cnt);
            } else if (in_range(dt_us, NEC_BIT_ZERO_MIN, NEC_BIT_ZERO_MAX)) {
                /* 비트 '0' */
            } else {
                /* 타이밍 오류 */
                s_state = IR_STATE_IDLE;
                break;
            }
            s_bit_cnt++;

            if (s_bit_cnt >= 32) {
                /* 32비트 수집 완료 — NEC 검증 */
                u8 addr      = (s_bits >>  0) & 0xFF;
                u8 addr_inv  = (s_bits >>  8) & 0xFF;
                u8 cmd       = (s_bits >> 16) & 0xFF;
                u8 cmd_inv   = (s_bits >> 24) & 0xFF;

                if (((addr & addr_inv) == 0) ||  /* 보수 체크 실패 허용 (확장 NEC) */
                    ((u8)(~cmd) == cmd_inv)) {
                    s_addr   = addr;
                    s_cmd    = cmd;
                    s_repeat = FALSE;
                    s_ready  = TRUE;
                }
                s_state = IR_STATE_IDLE;
            } else {
                s_state = IR_STATE_DATA;
            }
        }
        break;

    default:
        s_state = IR_STATE_IDLE;
        break;
    }
}

/*====================================================================
 * ir_recv_init — PA0 입력 + 양방향 인터럽트 설정
 *==================================================================*/
void ir_recv_init(void)
{
    /* PA0: GPIO 입력, 내부 풀업 */
    gpio_set_func(IR_RECV_PIN, AS_GPIO);
    gpio_set_output_en(IR_RECV_PIN, 0);
    gpio_set_input_en(IR_RECV_PIN, 1);
    gpio_setup_up_down_resistor(IR_RECV_PIN, PM_PIN_PULLUP_10K);

    /* GPIO 인터럽트 — 양방향 (RISING + FALLING 모두 감지) */
    gpio_set_interrupt_pol(IR_RECV_PIN, POL_FALLING);  /* 초기: FALLING */
    gpio_en_interrupt(IR_RECV_PIN, 1);

    s_state   = IR_STATE_IDLE;
    s_ready   = FALSE;
    s_cmd     = IR_CMD_NONE;
    s_last_time = clock_time();

    printf("[IR] init: PA0 NEC receiver ready\r\n");
}

/*====================================================================
 * ir_recv_poll — 타임아웃 감지 (app_task에서 호출)
 *==================================================================*/
void ir_recv_poll(void)
{
    if (s_state == IR_STATE_IDLE) return;

    u32 now = clock_time();
    u32 dt  = now - s_last_time;
    if (ticks_to_us(dt) > NEC_TIMEOUT_US) {
        /* 프레임 중간에 타임아웃 → 리셋 */
        s_state = IR_STATE_IDLE;
    }

    /* 수신 완료 처리 */
    if (s_ready) {
        s_ready = FALSE;
        ir_recv_on_cmd(s_addr, s_cmd, s_repeat);
    }
}

/*====================================================================
 * ir_recv_get_cmd / ir_recv_get_addr
 *==================================================================*/
u8 ir_recv_get_cmd(void)
{
    if (!s_ready) return IR_CMD_NONE;
    s_ready = FALSE;
    return s_cmd;
}

u8 ir_recv_get_addr(void)
{
    return s_addr;
}
