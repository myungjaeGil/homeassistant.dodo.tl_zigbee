/*********************************************************************
 * @file    ir_recv.c
 * @brief   NEC IR 수신 — GPIO_PA0 (Pin12), IRQ 방식
 *
 * 실측 확인 사항:
 *   - gpio_read() : idle=HIGH → 1, 신호=LOW → 0  (정상)
 *   - lv=0 : 핀LOW(신호시작) = FALLING 진입
 *   - lv=1 : 핀HIGH(신호종료) = RISING 진입
 *   - ticks/16 = us (16MHz 기준 clock_time)
 *   - LEADER_LOW 실측 9051us, LEADER_HIGH 실측 4510us (NEC 표준 일치)
 *
 * IRQ 설정:
 *   gpio_set_interrupt() : 전역 마스크 + POL 설정
 *   gpio_en_interrupt()  : 핀별 wakeup_en 설정 (별도 필요)
 *   초기 POL_FALLING 으로 시작 (첫 신호 = 핀LOW = FALLING)
 *
 * irq_handler.c:
 *   FLD_IRQ_GPIO_EN 블록에서 ir_recv_gpio_irq() 호출
 *********************************************************************/
#include "tl_common.h"
#include "ir_recv.h"
#include "pwm5ch_ep.h"
#include "pwm5ch_ctrl.h"

/*====================================================================
 * NEC 타이밍 상수 (us, ticks/16 기준 — 실측 일치 확인)
 *==================================================================*/
#define NEC_LEADER_LOW_MIN    7500    /* 9ms */
#define NEC_LEADER_LOW_MAX   10500
#define NEC_LEADER_HIGH_MIN   3800    /* 4.5ms */
#define NEC_LEADER_HIGH_MAX   5200
#define NEC_REPEAT_HIGH_MIN   1800    /* 2.25ms */
#define NEC_REPEAT_HIGH_MAX   2700
#define NEC_BIT_LOW_MIN        300    /* 562us */
#define NEC_BIT_LOW_MAX        900
#define NEC_BIT_ONE_MIN       1300    /* 1687us */
#define NEC_BIT_ONE_MAX       2000
#define NEC_BIT_ZERO_MIN       300    /* 562us */
#define NEC_BIT_ZERO_MAX       900
#define NEC_TIMEOUT_US        30000

/*====================================================================
 * 내부 상태머신
 *==================================================================*/
typedef enum {
    IR_STATE_IDLE = 0,
    IR_STATE_LEADER_LOW,
    IR_STATE_LEADER_HIGH,
    IR_STATE_DATA,
    IR_STATE_DATA_HIGH,
} ir_state_t;

static volatile ir_state_t s_state     = IR_STATE_IDLE;
static volatile u32        s_last_time = 0;
static volatile u32        s_bits      = 0;
static volatile u8         s_bit_cnt   = 0;

static volatile u8   s_cmd    = IR_CMD_NONE;
static volatile u8   s_addr   = 0;
static volatile bool s_repeat = FALSE;
static volatile bool s_ready  = FALSE;

/*====================================================================
 * 헬퍼
 *==================================================================*/
static inline u32 ticks_to_us(u32 ticks)
{
    /* clock_time() 실측: 16MHz 기준 */
    return ticks / 16u;
}

static inline bool in_range(u32 us, u32 min, u32 max)
{
    return (us >= min && us <= max);
}

static inline void ir_reset_to_idle(void)
{
    s_state = IR_STATE_IDLE;
    /* IDLE 복귀 시 항상 POL_FALLING 으로 재설정
     * 다음 신호 시작(핀LOW=FALLING)을 놓치지 않도록 */
    gpio_set_interrupt(IR_RECV_PIN, POL_FALLING);
    gpio_en_interrupt(IR_RECV_PIN, 1);
}

/*====================================================================
 * ir_recv_gpio_irq
 * 실측 기준:
 *   lv=0 → 핀LOW(신호) = FALLING → LEADER_LOW 시작, 비트 LOW 시작
 *   lv=1 → 핀HIGH(idle) = RISING  → LEADER_LOW 종료, 비트 판정
 *==================================================================*/
_attribute_ram_code_
void ir_recv_gpio_irq(void)
{
    u32 now   = clock_time();
    u8  level = gpio_read(IR_RECV_PIN) ? 1 : 0;

    /* IDLE 상태에서 RISING(lv=1, 핀HIGH) 은 노이즈 — 무시 */
    if (s_state == IR_STATE_IDLE && level) return;

    /* 다음 엣지 방향 즉시 전환 — pol 만 변경 (irq_mask 는 유지됨) */
    gpio_set_interrupt_pol(IR_RECV_PIN, level ? POL_FALLING : POL_RISING);

    u32 dt_ticks = (now >= s_last_time) ? (now - s_last_time)
                                        : (0xFFFFFFFFu - s_last_time + now + 1);
    u32 dt_us = ticks_to_us(dt_ticks);
    s_last_time = now;

    switch (s_state) {

    case IR_STATE_IDLE:
        if (!level) {   /* lv=0 = 핀LOW = FALLING = 신호 시작 */
            s_state   = IR_STATE_LEADER_LOW;
            s_bits    = 0;
            s_bit_cnt = 0;
        }
        break;

    case IR_STATE_LEADER_LOW:
        if (level) {    /* lv=1 = 핀HIGH = RISING = LOW 구간 종료 */
            if (in_range(dt_us, NEC_LEADER_LOW_MIN, NEC_LEADER_LOW_MAX)) {
                s_state = IR_STATE_LEADER_HIGH;
            } else {
                ir_reset_to_idle();
            }
        }
        break;

    case IR_STATE_LEADER_HIGH:
        if (!level) {   /* lv=0 = 핀LOW = FALLING = HIGH 구간 종료 */
            if (in_range(dt_us, NEC_LEADER_HIGH_MIN, NEC_LEADER_HIGH_MAX)) {
                s_state = IR_STATE_DATA;
            } else if (in_range(dt_us, NEC_REPEAT_HIGH_MIN, NEC_REPEAT_HIGH_MAX)) {
                /* repeat 프레임 — s_cmd 가 유효(NEC OK 수신 후)할 때만 콜백 */
                if (s_cmd != IR_CMD_NONE) {
                    s_repeat = TRUE;
                    s_ready  = TRUE;
                }
                ir_reset_to_idle();
            } else {
                ir_reset_to_idle();
            }
        }
        break;

    case IR_STATE_DATA:
        if (level) {    /* lv=1 = 핀HIGH = RISING = bit LOW 종료 */
            if (in_range(dt_us, NEC_BIT_LOW_MIN, NEC_BIT_LOW_MAX)) {
                s_state = IR_STATE_DATA_HIGH;
            } else {
                ir_reset_to_idle();
            }
        }
        break;

    case IR_STATE_DATA_HIGH:
        if (!level) {   /* lv=0 = 핀LOW = FALLING = bit HIGH 종료 → 비트 결정 */
            if (in_range(dt_us, NEC_BIT_ONE_MIN, NEC_BIT_ONE_MAX)) {
                s_bits |= (1u << s_bit_cnt);
            } else if (in_range(dt_us, NEC_BIT_ZERO_MIN, NEC_BIT_ZERO_MAX)) {
                /* bit 0 */
            } else {
                ir_reset_to_idle();
                break;
            }
            s_bit_cnt++;

            if (s_bit_cnt >= 32) {
                u8 addr     = (s_bits >>  0) & 0xFF;
                u8 addr_inv = (s_bits >>  8) & 0xFF;
                u8 cmd      = (s_bits >> 16) & 0xFF;
                u8 cmd_inv  = (s_bits >> 24) & 0xFF;

                if (((addr & addr_inv) == 0) || ((u8)(~cmd) == cmd_inv)) {
                    s_addr   = addr;
                    s_cmd    = cmd;
                    s_repeat = FALSE;
                    s_ready  = TRUE;
                    DBG_LOG("[IR] NEC OK addr=0x%02x cmd=0x%02x\r\n",
                           (int)addr, (int)cmd);
                } else {
                    DBG_LOG("[IR] NEC FAIL addr=0x%02x inv=0x%02x cmd=0x%02x inv=0x%02x\r\n",
                           (int)addr, (int)addr_inv, (int)cmd, (int)cmd_inv);
                }
                ir_reset_to_idle();
            } else {
                s_state = IR_STATE_DATA;
            }
        }
        break;

    default:
        ir_reset_to_idle();
        break;
    }
}

/*====================================================================
 * ir_recv_init — bdb_init() 이후 호출
 *==================================================================*/
void ir_recv_init(void)
{
    gpio_set_func(IR_RECV_PIN, AS_GPIO);
    gpio_set_output_en(IR_RECV_PIN, 0);
    gpio_set_input_en(IR_RECV_PIN, 1);
    gpio_setup_up_down_resistor(IR_RECV_PIN, PM_PIN_PULLUP_10K);

    s_state     = IR_STATE_IDLE;
    s_ready     = FALSE;
    s_cmd       = IR_CMD_NONE;
    s_last_time = clock_time();

    /* POL_FALLING: 첫 신호 = 핀LOW(FALLING) 감지 */
    gpio_set_interrupt(IR_RECV_PIN, POL_FALLING);
    gpio_en_interrupt(IR_RECV_PIN, 1);

    DBG_LOG("[IR] init: PA0 NEC receiver ready\r\n");
}

/*====================================================================
 * ir_recv_poll — 타임아웃 감지 + 수신 완료 콜백 (app_task 에서 호출)
 *==================================================================*/
void ir_recv_poll(void)
{
    if (s_state != IR_STATE_IDLE) {
        u32 now = clock_time();
        u32 dt  = (now >= s_last_time) ? (now - s_last_time)
                                       : (0xFFFFFFFFu - s_last_time + now + 1);
        if (ticks_to_us(dt) > NEC_TIMEOUT_US) {
            ir_reset_to_idle();
        }
    }

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
