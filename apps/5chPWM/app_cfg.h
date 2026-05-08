/********************************************************************************************************
 * @file    app_cfg.h
 *
 * @brief   This is the header file for app_cfg
 *
 * @author  Zigbee Group
 * @date    2021
 *
 * @par     Copyright (c) 2021, Telink Semiconductor (Shanghai) Co., Ltd. ("TELINK")
 *			All rights reserved.
 *
 *          Licensed under the Apache License, Version 2.0 (the "License");
 *          you may not use this file except in compliance with the License.
 *          You may obtain a copy of the License at
 *
 *              http://www.apache.org/licenses/LICENSE-2.0
 *
 *          Unless required by applicable law or agreed to in writing, software
 *          distributed under the License is distributed on an "AS IS" BASIS,
 *          WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *          See the License for the specific language governing permissions and
 *          limitations under the License.
 *
 *******************************************************************************************************/
#pragma once

/* Enable C linkage for C++ Compilers: */
#if defined(__cplusplus)
extern "C" {
#endif

/**********************************************************************
 * App configuration
 */
/* Debug mode */

#define	UART_PRINTF_MODE                        1
#define DEBUG_BAUDRATE              			1000000//1M
#define USB_PRINTF_MODE                         0

/* HCI interface */
#define	ZBHCI_UART                              0

/* BDB */
#define TOUCHLINK_SUPPORT                       1
#define FIND_AND_BIND_SUPPORT                   0

/* Voltage detect module */
/* If VOLTAGE_DETECT_ENABLE is set,
 * 1) if MCU_CORE_826x is defined, the DRV_ADC_VBAT_MODE mode is used by default,
 * and there is no need to configure the detection IO port;
 * 2) if MCU_CORE_8258 or MCU_CORE_8278 is defined, the DRV_ADC_VBAT_MODE mode is used by default,
 * we need to configure the detection IO port, and the IO must be in a floating state.
 * 3) if MCU_CORE_B91 is defined, the DRV_ADC_BASE_MODE mode is used by default,
 * we need to configure the detection IO port, and the IO must be connected to the target under test,
 * such as VCC.
 */
#define VOLTAGE_DETECT_ENABLE                   0

/* Flash protect module */
/* Only the firmware area will be locked, the NV data area will not be locked.
 * For details, please refer to drv_flash.c file.
 */
#define FLASH_PROTECT_ENABLE                    0

/* Watch dog module */
#define MODULE_WATCHDOG_ENABLE                  0

/* UART module */
#if ZBHCI_UART
#define	MODULE_UART_ENABLE                      1
#endif

#if (ZBHCI_USB_PRINT || ZBHCI_USB_CDC || ZBHCI_USB_HID || ZBHCI_UART)
#define ZBHCI_EN                                1
#endif

/**********************************************************************
 * Board definitions
 */
/* Board ID */
#define BOARD_826x_EVK                          0
#define BOARD_826x_DONGLE                       1
#define BOARD_8258_EVK                          2//DEPRECATED
#define BOARD_8258_EVK_V1P2                     3//C1T139A30_V1.2
#define BOARD_8258_DONGLE                       4
#define BOARD_8278_EVK                          5
#define BOARD_8278_DONGLE                       6
#define BOARD_B91_EVK                           7
#define BOARD_B91_DONGLE                        8
#define BOARD_B92_EVK                           9
#define BOARD_B92_DONGLE                        10
#define BOARD_TL721X_EVK                        11
#define BOARD_TL721X_DONGLE                     12
#define BOARD_TL321X_EVK                        13
#define BOARD_TL321X_DONGLE                     14
//Module
#define BOARD_ML7218D_MERCURY                   15//ML7218D-MERCURY-M0-PE11-V1.3
#define BOARD_ML7218A_GAIA                      16//ML7218A_GAIA-M0-PE11-V1.3

#define BOARD_8258_ZT3L                         17
#define BOARD_8258_5IN1							18


/* Board define */
#define BOARD                               BOARD_8258_ZT3L//BOARD_8258_5IN1//BOARD_8258_ZT3L//BOARD_8258_EVK_V1P2
#define CLOCK_SYS_CLOCK_HZ                  48000000

/**********************************************************************
 * Version configuration
 */
#include "version_cfg.h"

/**********************************************************************
 * Board configuration
 */
#include "board_8258_zt3l.h"

/**********************************************************************
 * Stack configuration
 */
#include "stack_cfg.h"

/**********************************************************************
 * ZCL cluster configuration
 */
#define ZCL_ON_OFF_SUPPORT                      0
#define ZCL_LEVEL_CTRL_SUPPORT                  0
#define ZCL_LIGHT_COLOR_CONTROL_SUPPORT         0
#define ZCL_DOOR_LOCK_SUPPORT                   0
#define ZCL_TEMPERATURE_MEASUREMENT_SUPPORT     0
#define ZCL_OCCUPANCY_SENSING_SUPPORT           0
#define ZCL_IAS_ZONE_SUPPORT                    0
#define ZCL_POLL_CTRL_SUPPORT                   0
#define ZCL_GROUP_SUPPORT                       0
#define ZCL_SCENE_SUPPORT                       0
#define ZCL_OTA_SUPPORT                         0
#define ZCL_GP_SUPPORT                          1
//test
#define AF_TEST_ENABLE                          0



/**********************************************************************
 * EV configuration
 */
typedef enum {
    EV_POLL_ED_DETECT,
    EV_POLL_HCI,
    EV_POLL_IDLE,
    EV_POLL_MAX,
} ev_poll_e;

/* Disable C linkage for C++ Compilers: */
#if defined(__cplusplus)
}
#endif
