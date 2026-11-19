/********************************************************************************************************
 * @file    app_cfg.h
 * @brief   ZT3L 크리스마스 트리 3CH 전등 — 앱 설정
 *******************************************************************************************************/
#pragma once

/*--------------------------------------------------------------------
 * IR 수신 활성화 — irq_handler.c 의 멀티 프로젝트 분기에서 사용
 *------------------------------------------------------------------*/
#define IR_RECV_ENABLE                          1

#if defined(__cplusplus)
extern "C" {
#endif

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
#define TOUCHLINK_SUPPORT                       0   /* 불필요 + 근처 touchlink 리셋 위험 → 비활성 */
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
 * 크리스마스 트리 효과 설정
 *   아래 값은 첫 부팅 기본값. 효과·속도·자동순환 간격은 Zigbee 속성으로
 *   바꿀 수 있고 NV 에 저장됨 (xmas_ep.h 참고)
 *------------------------------------------------------------------*/
#define XMAS_FX_TICK_MS                         20      /* 효과 갱신 주기 (50Hz) */
#define XMAS_FX_DEFAULT                         1       /* 0=점등 1=물결 ... (xmas_fx.h) */
#define XMAS_FX_SPEED_DEFAULT                   5       /* 1(느림) ~ 10(빠름) */
#define XMAS_FX_AUTO_SEC_DEFAULT                30      /* 자동 순환 시 효과 교체 간격(초) */
#define XMAS_BUTTON_FX_ENABLE                   1       /* BUTTON1 짧게 = 다음 효과 */

/*--------------------------------------------------------------------
 * ZCL 클러스터 활성화
 *  EP1~EP3 : OnOff + Level Control (트리 채널 개별)
 *  EP4     : OnOff + Level Control (전체 전원·밝기) + 효과 속성
 *------------------------------------------------------------------*/
#define ZCL_ON_OFF_SUPPORT                      1
#define ZCL_LEVEL_CTRL_SUPPORT                  1
#define ZCL_LIGHT_COLOR_CONTROL_SUPPORT         0
#define ZCL_DOOR_LOCK_SUPPORT                   0
#define ZCL_TEMPERATURE_MEASUREMENT_SUPPORT     0
#define ZCL_OCCUPANCY_SENSING_SUPPORT           0
#define ZCL_IAS_ZONE_SUPPORT                    0
#define ZCL_POLL_CTRL_SUPPORT                   0
#define ZCL_GROUP_SUPPORT                       1
#define ZCL_SCENE_SUPPORT                       1
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
