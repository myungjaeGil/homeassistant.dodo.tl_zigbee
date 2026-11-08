/********************************************************************************************************
 * @file    app_cfg.h
 * @brief   ZT3L 무시동히터 리모컨 — 앱 설정
 *
 *  히터 컨트롤러의 버튼(ON / OFF / UP / DOWN)을 GPIO 펄스로 눌러주고,
 *  외부에서 눌린 버튼도 같은 핀으로 읽어 상태(전원, 레벨 0~9)를 맞추는 Zigbee 리모컨.
 *  하드웨어는 PWM5CH 보드의 출력 채널을 그대로 사용 (PWM 대신 일반 GPIO 출력).
 *******************************************************************************************************/
#pragma once

/*--------------------------------------------------------------------
 * IR 수신 활성화 — irq_handler.c 의 멀티 프로젝트 분기에서 사용
 *------------------------------------------------------------------*/
#define IR_RECV_ENABLE                          1

#if defined(__cplusplus)
extern "C" {
#endif

/*====================================================================
 * 히터 리모컨 설정 — 히터/배선에 맞게 이 블록만 조정
 *==================================================================*/

/* 버튼 라인 동작 방식 — 핀은 평소 "입력"으로 대기하고, 누를 때만 잠깐 "출력"
 *
 *   HEATER_ACTIVE_LEVEL
 *     1 : 눌림 = HIGH. 대기 시 입력 + 내부 풀다운(100k).
 *         누를 때 HIGH 출력, 뗄 때 다시 입력(Hi-Z) — LOW 를 직접 출력하지 않으므로
 *         원래 리모컨/히터 버튼과 동시에 눌려도 충돌하지 않음
 *     0 : 눌림 = LOW. 대기 시 입력 + 내부 풀업(10k), 누를 때 LOW 출력
 *
 *   외부(원래 리모컨, 히터 본체 버튼)에서 라인이 눌림 레벨이 되면
 *   그 입력을 마지막 입력으로 반영 → 전원 상태 / 레벨 갱신
 *
 *   ※ 입력 전압은 3.3V 이하여야 함 (TLSR8258 은 5V 입력 불가)
 */
#define HEATER_ACTIVE_LEVEL                     1

/* 외부 입력 감시 (0 = 출력 전용, 외부 버튼 무시) */
#define HEATER_IN_MONITOR_ENABLE                1

/* 외부 입력 디바운스 — 이 시간 이상 눌림 레벨이 유지되어야 1회 입력으로 인정 */
#define HEATER_IN_DEBOUNCE_MS                   30

/* 진단용 — 라인 원시 레벨 변화(디바운스 전)를 로그로 출력 (1 = 사용)
 *   [RAW] DOWN 1 t=12345ms 처럼 라인별 HIGH/LOW 변화를 찍음
 *   초당 30회를 넘으면 나머지는 개수만 요약 (떠 있는 라인 진단용)
 *   부팅 직후 각 라인의 현재 레벨도 한 번 출력 */
#define HEATER_IN_RAW_LOG                       0

/* 직접 누른 뒤 라인이 대기 레벨로 돌아올 때까지 그 핀의 입력을 무시하는 시간 */
#define HEATER_IN_BLANK_MS                      80

/* 히터 레벨(단계) 범위 — UP/DOWN 1회 = ±1, 범위 밖으로는 변하지 않음 */
#define HEATER_LEVEL_MIN                        0
#define HEATER_LEVEL_MAX                        9

/* 전원/레벨 NV 저장 지연 (UP 연타 시 플래시 쓰기 몰림 방지) */
#define HEATER_STATE_SAVE_DELAY_MS              2000

/* 전원 버튼 방식
 *   0 : ON 버튼, OFF 버튼이 따로 있음 (CH1=ON, CH2=OFF)
 *   1 : 전원 버튼 하나로 켜고 끔 (CH1 만 사용, 같은 상태로의 재요청은 무시)
 */
#define HEATER_POWER_SINGLE_BUTTON              0

/* 히터가 꺼져 있을 때 UP/DOWN 요청 무시 (1 = 무시) */
#define HEATER_UPDN_REQUIRE_ON                  1

/* 펄스 시간 기본값 (ms) — Zigbee 속성으로 런타임 변경 가능, NV 저장
 *   POWER : ON/OFF 버튼 누름 시간 (길게 눌러야 켜지는 히터는 1000~3000)
 *   BTN   : UP/DOWN 버튼 누름 시간
 *   GAP   : 연속 명령 사이 버튼을 뗀 상태로 기다리는 시간
 */
#define HEATER_POWER_PULSE_MS_DEF               500
#define HEATER_BTN_PULSE_MS_DEF                 200
#define HEATER_GAP_MS_DEF                       300

/* 허용 범위 (속성 쓰기 시 이 범위로 보정) */
#define HEATER_PULSE_MS_MIN                     50
#define HEATER_PULSE_MS_MAX                     5000
#define HEATER_GAP_MS_MIN                       100
#define HEATER_GAP_MS_MAX                       3000

/* 대기 명령 최대 개수 (UP 연타 등) — 넘치면 버림 */
#define HEATER_QUEUE_SIZE                       8

/* SSD1306 OLED 사용 (0 = 미사용, I2C 핀 미초기화) */
#define HEATER_OLED_ENABLE                      1

/* BUTTON1 짧게 누름(< 1초) = 히터 전원 토글 (0 = 공장 초기화 전용) */
#define HEATER_BUTTON_TOGGLE_ENABLE             1

/*--------------------------------------------------------------------
 * Debug
 *------------------------------------------------------------------*/
#define UART_PRINTF_MODE                        1

/*--------------------------------------------------------------------------
 * 디버그 로그 출력 제어
 *   1: printf 출력 활성화
 *   0: printf 출력 비활성화 (릴리즈 빌드)
 *------------------------------------------------------------------------*/
#define DEBUG_ENABLE                            1

#if DEBUG_ENABLE
    #define DBG_LOG(...)    printf(__VA_ARGS__)
#else
    #define DBG_LOG(...)    do{}while(0)
#endif
#define DEBUG_BAUDRATE                          1000000
#define USB_PRINTF_MODE                         0

/*--------------------------------------------------------------------
 * HCI
 *------------------------------------------------------------------*/
#define ZBHCI_UART                              0

/*--------------------------------------------------------------------
 * BDB
 *------------------------------------------------------------------*/
#define TOUCHLINK_SUPPORT                       0
#define FIND_AND_BIND_SUPPORT                   0

/*--------------------------------------------------------------------
 * 기능 모듈
 *------------------------------------------------------------------*/
#define VOLTAGE_DETECT_ENABLE                   0
#define FLASH_PROTECT_ENABLE                    0
#define MODULE_WATCHDOG_ENABLE                  0

#if ZBHCI_UART
#define MODULE_UART_ENABLE                      1
#endif

#if (ZBHCI_USB_PRINT || ZBHCI_USB_CDC || ZBHCI_USB_HID || ZBHCI_UART)
#define ZBHCI_EN                                1
#endif

/*--------------------------------------------------------------------
 * Board 선택
 *------------------------------------------------------------------*/
#define BOARD_8258_ZT3L                         17

#define BOARD                                   BOARD_8258_ZT3L
#define CLOCK_SYS_CLOCK_HZ                      48000000

/*--------------------------------------------------------------------
 * Version / Board / Stack 설정 포함
 *------------------------------------------------------------------*/
#include "version_cfg.h"
#include "board_8258_zt3l.h"
#include "stack_cfg.h"

/*--------------------------------------------------------------------
 * ZCL 클러스터 활성화
 *  EP1 : OnOff (히터 전원) + 펄스 설정 / 레벨 / 마지막 입력 속성
 *  EP2 : OnOff (UP 버튼, 순간 동작)
 *  EP3 : OnOff (DOWN 버튼, 순간 동작)
 *------------------------------------------------------------------*/
#define ZCL_ON_OFF_SUPPORT                      1
#define ZCL_LEVEL_CTRL_SUPPORT                  0
#define ZCL_LIGHT_COLOR_CONTROL_SUPPORT         0
#define ZCL_DOOR_LOCK_SUPPORT                   0
#define ZCL_TEMPERATURE_MEASUREMENT_SUPPORT     0
#define ZCL_OCCUPANCY_SENSING_SUPPORT           0
#define ZCL_IAS_ZONE_SUPPORT                    0
#define ZCL_POLL_CTRL_SUPPORT                   0
#define ZCL_GROUP_SUPPORT                       1
#define ZCL_SCENE_SUPPORT                       0
#define ZCL_OTA_SUPPORT                         1
#define ZCL_GP_SUPPORT                          1

#define AF_TEST_ENABLE                          0

/*--------------------------------------------------------------------
 * EV poll
 *------------------------------------------------------------------*/
typedef enum {
    EV_POLL_ED_DETECT,
    EV_POLL_HCI,
    EV_POLL_IDLE,
    EV_POLL_MAX,
} ev_poll_e;

#if defined(__cplusplus)
}
#endif
