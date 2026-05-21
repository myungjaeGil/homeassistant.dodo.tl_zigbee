/********************************************************************************************************
 * @file    app_cfg.h
 * @brief   ZT3L PWM 5ch 컨트롤러 — 앱 설정
 *******************************************************************************************************/
#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/*--------------------------------------------------------------------
 * Debug
 *------------------------------------------------------------------*/
#define UART_PRINTF_MODE                        1
#define DEBUG_BAUDRATE                          1000000
#define USB_PRINTF_MODE                         0

/*--------------------------------------------------------------------
 * HCI
 *------------------------------------------------------------------*/
#define ZBHCI_UART                              0

/*--------------------------------------------------------------------
 * BDB
 *------------------------------------------------------------------*/
#define TOUCHLINK_SUPPORT                       1
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
 *  EP1~EP5 : OnOff + Level Control (PWM 개별)
 *  EP6     : OnOff + Level Control (마스터 디밍)
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
