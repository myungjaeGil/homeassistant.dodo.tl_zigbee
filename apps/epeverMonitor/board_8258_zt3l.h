/********************************************************************************************************
 * @file    board_8258_zt3l.h
 *
 * @brief   Tuya ZT3L module (TLSR8258F1KAT32) — 5ch PWM + Button1
 *
 * @author  Zigbee Group (modified for ZT3L)
 * @date    2024
 *
 * ZT3L Pin mapping (5-ch PWM + Button1):
 *
 *   LEFT ROW  (top to bottom)       RIGHT ROW (top to bottom)
 *   Pin1  : RST                     Pin16 : TXD (PB1) - UART_TX
 *   Pin2  : C4  (PC4) - ADC         Pin15 : RXD (PB7) - UART_RX
 *   Pin3  : EN                      Pin14 : B5  (PB5) - PWM4  CH4
 *   Pin4  : D7  (PD7) - DEBUG TX    Pin13 : B4  (PB4) - PWM3  CH3
 *   Pin5  : D2  (PD2) - PWM2  CH2   Pin12 : A0  (PA0) - LED_PERMIT
 *   Pin6  : C3  (PC3) - PWM1  CH1   Pin11 : D4  (PD4) - BUTTON1
 *   Pin7  : C2  (PC2) - PWM0  CH0   Pin10 : C0  (PC0) - LED_POWER
 *   Pin8  : 3V3                     Pin9  : GND
 *   Pin17 : SWS (별도, 우하단)
 *
 * PWM channel (5ch):
 *   CH0  GPIO_PC2  PWM0  (Pin7)
 *   CH1  GPIO_PC3  PWM1  (Pin6)
 *   CH2  GPIO_PD2  PWM3  (Pin5)
 *   CH3  GPIO_PB4  PWM4  (Pin13)
 *   CH4  GPIO_PB5  PWM5  (Pin14)
 *
 * Button:
 *   BUTTON1  GPIO_PD4  (Pin11)
 *
 * Status LED (GPIO):
 *   LED_POWER   GPIO_PC0  (Pin10)
 *   LED_PERMIT  GPIO_PA0  (Pin12)
 *
 * Zigbee Endpoint:
 *   EP1~EP5 : PWM CH0~CH4 개별 OnOff+Level
 *   EP6     : 마스터 디밍 (전채널 동시)
 *******************************************************************************************************/
#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

///* 5채널 독립 PWM 모드 */
//#define COLOR_RGB_SUPPORT       0
//#define COLOR_CCT_SUPPORT       0
//#define BRIGHTNESS_SUPPORT      0

/*--------------------------------------------------------------------
 * Button
 *  BUTTON1 : PD4 (Pin11) — 실제 버튼 (GPIO 입력)
 *  BUTTON2 : 더미
 *------------------------------------------------------------------*/
#define BUTTON1                 GPIO_PD4
#define BUTTON2                 GPIO_PD4    /* 더미 */

enum {
    VK_SW1 = 0x01,
    VK_SW2 = 0x02,
};

#define KB_MAP_NORMAL           { {VK_SW1,}, {VK_SW2,}, }
#define KB_MAP_NUM              KB_MAP_NORMAL
#define KB_MAP_FN               KB_MAP_NORMAL
#define KB_DRIVE_PINS           {0}
//#define KB_SCAN_PINS            {BUTTON1, BUTTON2}
#define KB_SCAN_PINS            {BUTTON1}

/* PD4 — GPIO 입력 (버튼) */
#define PD4_FUNC                AS_GPIO
#define PD4_OUTPUT_ENABLE       0
#define PD4_INPUT_ENABLE        1

/*--------------------------------------------------------------------
 * 5채널 PWM 핀 정의
 *------------------------------------------------------------------*/
///* CH0: PC2 — PWM0 (Pin7) */
//#define PWM_CH0_PIN             GPIO_PC2
//#define PWM_CH0_CH              0
//#define PWM_CH0_SET()           do{ gpio_set_func(PWM_CH0_PIN, AS_PWM0); }while(0)
//
///* CH1: PC3 — PWM1 (Pin6) */
//#define PWM_CH1_PIN             GPIO_PC3
//#define PWM_CH1_CH              1
//#define PWM_CH1_SET()           do{ gpio_set_func(PWM_CH1_PIN, AS_PWM1); }while(0)
//
///* CH2: PD2 — PWM3 (Pin5) */
//#define PWM_CH2_PIN             GPIO_PD2
//#define PWM_CH2_CH              3
//#define PWM_CH2_SET()           do{ gpio_set_func(PWM_CH2_PIN, AS_PWM3); }while(0)
//
///* CH3: PB4 — PWM4 (Pin13) */
//#define PWM_CH3_PIN             GPIO_PB4
//#define PWM_CH3_CH              4
//#define PWM_CH3_SET()           do{ gpio_set_func(PWM_CH3_PIN, AS_PWM4); }while(0)
//
///* CH4: PB5 — PWM5 (Pin14) */
//#define PWM_CH4_PIN             GPIO_PB5
//#define PWM_CH4_CH              5
//#define PWM_CH4_SET()           do{ gpio_set_func(PWM_CH4_PIN, AS_PWM5); }while(0)
//
//#define PWM_CHANNEL_COUNT       5
//
/*
//#define PWM_ALL_CHANNEL_SET()   do{ \
//                                    PWM_CH0_SET(); \
//                                    PWM_CH1_SET(); \
//                                    PWM_CH2_SET(); \
//                                    PWM_CH3_SET(); \
//                                    PWM_CH4_SET(); \
//                                }while(0)
*/

/*--------------------------------------------------------------------
 * SDK 호환 매핑 — PWM 미사용(BMS 프로젝트), 링크 에러 방지용 주석 처리
 *------------------------------------------------------------------*/
//#define LED_Y                   PWM_CH3_PIN
//#define LED_W                   PWM_CH4_PIN
//#define PWM_Y_CHANNEL           PWM_CH3_CH
//#define PWM_W_CHANNEL           PWM_CH4_CH
//#define PWM_Y_CHANNEL_SET()     PWM_CH3_SET()
//#define PWM_W_CHANNEL_SET()     PWM_CH4_SET()
//#define WARM_LIGHT_PWM_CHANNEL  PWM_Y_CHANNEL
//#define COOL_LIGHT_PWM_CHANNEL  PWM_W_CHANNEL
//#define WARM_LIGHT_PWM_SET()    PWM_Y_CHANNEL_SET()
//#define COOL_LIGHT_PWM_SET()    PWM_W_CHANNEL_SET()

//#define LED_R                   PWM_CH2_PIN
//#define LED_G                   PWM_CH0_PIN
//#define LED_B                   PWM_CH1_PIN

/* PWM 핀 GPIO 모드 설정 — PWM 미사용이므로 주석 처리 */
//#define PC2_FUNC                AS_PWM0
//#define PC2_OUTPUT_ENABLE       1
//#define PC2_INPUT_ENABLE        0

//#define PC3_FUNC                AS_PWM1
//#define PC3_OUTPUT_ENABLE       1
//#define PC3_INPUT_ENABLE        0

//#define PD2_FUNC                AS_PWM3
//#define PD2_OUTPUT_ENABLE       1
//#define PD2_INPUT_ENABLE        0

//#define PB4_FUNC                AS_PWM4
//#define PB4_OUTPUT_ENABLE       1
//#define PB4_INPUT_ENABLE        0

//#define PB5_FUNC                AS_PWM5
//#define PB5_OUTPUT_ENABLE       1
//#define PB5_INPUT_ENABLE        0

/*--------------------------------------------------------------------
 * Status LED (GPIO)
 *  LED_POWER  : PC0 (Pin10) — 전원/상태 표시
 *  LED_PERMIT : PA0 (Pin12) — Permit Join 표시
 *------------------------------------------------------------------*/
#define LED_POWER               GPIO_PC0
#define LED_PERMIT              GPIO_PA0   /* [수정] PC0 → PA0 */

#define PC0_FUNC                AS_GPIO
#define PC0_OUTPUT_ENABLE       1
#define PC0_INPUT_ENABLE        0

#define PA0_FUNC                AS_GPIO
#define PA0_OUTPUT_ENABLE       1
#define PA0_INPUT_ENABLE        0

/*--------------------------------------------------------------------
 * ADC
 *------------------------------------------------------------------*/
#if 1
#define VOLTAGE_DETECT_ADC_PIN  GPIO_PC4
#endif

/*--------------------------------------------------------------------
 * UART
 *------------------------------------------------------------------*/
#define UART_TX_PIN             UART_TX_PB1
#define UART_RX_PIN             UART_RX_PB7
//#define DEBUG_INFO_TX_PIN		UART_RX_PIN
#define DEBUG_INFO_TX_PIN       GPIO_PD7    // Pin4 — printf 전용 debug TX drv_putchar.c soft_uart_putc 로 변경해야함

/* PD7 — DEBUG TX 핀: gpio_init() 시점에 output으로 설정 */
#define PD7_FUNC                AS_GPIO
#define PD7_OUTPUT_ENABLE       1
#define PD7_INPUT_ENABLE        0

#if defined(__cplusplus)
}
#endif
