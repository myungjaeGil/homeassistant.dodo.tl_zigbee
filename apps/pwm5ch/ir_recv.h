#pragma once
/*********************************************************************
 * @file    ir_recv.h
 * @brief   NEC IR 수신 — GPIO_PD4 (Pin11)
 *           FALLING=GPIO_EN, RISING=RISC0 양방향 IRQ
 *
 * NEC 프로토콜:
 *   Leader  : 9ms LOW + 4.5ms HIGH
 *   Bit '0' : 562us LOW + 562us  HIGH
 *   Bit '1' : 562us LOW + 1687us HIGH
 *   Repeat  : 9ms LOW + 2.25ms HIGH + 562us LOW
 *   Frame   : 32비트 (addr 8 + ~addr 8 + cmd 8 + ~cmd 8)
 *
 * 사용법:
 *   1. ir_recv_init()     — 초기화 (gpio_init 이후 호출)
 *   2. ir_recv_poll()     — app_task() 에서 매 루프 호출
 *   3. ir_recv_get_cmd()  — 수신된 커맨드 확인 (0xFF = 없음)
 *********************************************************************/

#include "tl_common.h"

/* IR 수신 핀 — board_8258_zt3l.h 에서 정의 (GPIO_PD4, Pin11) */
#ifndef IR_RECV_PIN
#define IR_RECV_PIN     GPIO_PD4
#endif

/* 수신 없음 반환값 */
#define IR_CMD_NONE     0xFF

/*--------------------------------------------------------------------
 * 초기화 / 폴링 / 커맨드 읽기
 *------------------------------------------------------------------*/
void ir_recv_init(void);
void ir_recv_poll(void);

/* 마지막으로 수신된 커맨드 반환 (IR_CMD_NONE = 없음)
 * 읽은 후 내부 버퍼 클리어 */
u8   ir_recv_get_cmd(void);
u8   ir_recv_get_addr(void);

/* IRQ 핸들러에서 호출 (FALLING + RISING 양방향) */
void ir_recv_gpio_irq(void);

/* IR 수신 이벤트 콜백 — 사용자 구현 */
void ir_recv_on_cmd(u8 addr, u8 cmd, bool repeat);
