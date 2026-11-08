#pragma once
/*********************************************************************
 * @file    heater_ctrl.h
 * @brief   ZT3L 무시동히터 리모컨 — 양방향 버튼 라인 / LED / 히터 동작 인터페이스
 *********************************************************************/

#include "tl_common.h"

/*--------------------------------------------------------------------
 * 히터 버튼 동작
 *------------------------------------------------------------------*/
typedef enum {
    HEATER_ACT_ON = 0,
    HEATER_ACT_OFF,
    HEATER_ACT_UP,
    HEATER_ACT_DOWN,
    HEATER_ACT_COUNT,
} heater_act_t;

/* 버튼 라인 초기화 — 부팅 직후 가장 먼저 호출 (모든 라인 입력 대기) */
void heater_out_init(void);

/* 외부 입력 감시 — 메인 루프에서 매번 호출 */
void heater_in_poll(void);

/* 버튼 펄스 1회 요청 — 대기열에 넣고 순서대로 실행. 대기열이 차면 FALSE */
bool heater_request(heater_act_t act);

/* 펄스 진행 중이거나 대기 명령이 있으면 TRUE */
bool heater_is_busy(void);

/* 대기열에 남은 명령 수 (진행 중 포함) */
u8   heater_pending(void);

/* 마지막 입력 — 직접 누름 또는 외부 감지 (없으면 HEATER_ACT_COUNT) */
heater_act_t heater_last_act(void);

const char *heater_act_name(heater_act_t act);

/*--------------------------------------------------------------------
 * 히터 동작 (상태 갱신 포함) — Zigbee / IR / 버튼 공용
 *------------------------------------------------------------------*/
bool heater_power_set(bool on);     /* 히터 켜기/끄기 */
bool heater_power_toggle(void);
bool heater_step(bool up);          /* UP / DOWN 1단계 */

/*--------------------------------------------------------------------
 * LED_POWER 상태
 *------------------------------------------------------------------*/
typedef enum {
    LED_PWR_STATE_BOOT        = 0,
    LED_PWR_STATE_JOINED      = 1,
    LED_PWR_STATE_NOT_JOINED  = 2,
    LED_PWR_STATE_RESET       = 3,
    LED_PWR_STATE_PERMIT      = 4,   /* Permit Join 중 — 100ms 빠른 블링크 */
} led_pwr_state_t;

void led_power_init(void);
void led_power_set_state(led_pwr_state_t state);

/* Identify 블링크 — times 회 후 원래 상태로 복귀 (0 = stop 호출까지) */
void light_blink_start(u8 times, u16 ledOnTime, u16 ledOffTime);
void light_blink_stop(void);
