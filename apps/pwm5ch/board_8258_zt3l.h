/*****************************************************************************
 * @file    board_8258_zt3l.h
 * @brief   Tuya ZT3L 모듈 (TLSR8258F1KAT32) — PWM 5CH + IR + I2C OLED 보드 핀 정의
 *
 * ZT3L 핀 배치:
 *
 *   LEFT  (위→아래)                RIGHT (위→아래)
 *   Pin1  : RST                    Pin16 : PB1 — (미사용)
 *   Pin2  : PC4 — I2C_SDA         Pin15 : PB7 — I2C_SCL
 *   Pin3  : EN                     Pin14 : PB5 — PWM5  CH4
 *   Pin4  : PD7 — DEBUG TX         Pin13 : PB4 — PWM4  CH3
 *   Pin5  : PD2 — PWM3  CH2        Pin12 : PA0 — IR 수신 입력
 *   Pin6  : PC3 — PWM1  CH1        Pin11 : PD4 — BUTTON1
 *   Pin7  : PC2 — PWM0  CH0        Pin10 : PC0 — LED_POWER
 *   Pin8  : 3V3                    Pin9  : GND
 *   Pin17 : SWS (우하단)
 *
 * PWM 채널:
 *   CH0  GPIO_PC2  SDK PWM0  Pin7
 *   CH1  GPIO_PC3  SDK PWM1  Pin6
 *   CH2  GPIO_PD2  SDK PWM3  Pin5
 *   CH3  GPIO_PB4  SDK PWM4  Pin13
 *   CH4  GPIO_PB5  SDK PWM5  Pin14
 *
 * GPIO:
 *   BUTTON1    GPIO_PD4  Pin11  — 입력, 내부 풀업 (5초 장누름 = factory reset)
 *   LED_POWER  GPIO_PC0  Pin10  — 출력, active high
 *   IR_RECV    GPIO_PA0  Pin12  — NEC IR 수신, PA 계열 GPIO IRQ 지원
 *
 * I2C (software bit-bang) — SSD1306 OLED 128x32:
 *   SDA  GPIO_PC4  Pin2   (내부 풀업)
 *   SCL  GPIO_PB7  Pin15  (내부 풀업)
 *
 * UART:
 *   PB1  Pin16  미사용
 *   PB7  Pin15  UART_RX → I2C_SCL 전용 전환
 *   PD7  Pin4   DEBUG soft-TX (printf 출력, UART_PRINTF_MODE=1)
 *****************************************************************************/
#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * Button — BUTTON1 (PD4, Pin11), 5초 장누름 = factory reset
 *------------------------------------------------------------------------*/
#define BUTTON1                 GPIO_PD4
#define BUTTON2                 BUTTON1     /* SDK 요구 더미 */

enum { VK_SW1 = 0x01, VK_SW2 = 0x02 };
#define KB_MAP_NORMAL           { {VK_SW1,}, {VK_SW2,}, }
#define KB_MAP_NUM              KB_MAP_NORMAL
#define KB_MAP_FN               KB_MAP_NORMAL
#define KB_DRIVE_PINS           {0}
#define KB_SCAN_PINS            {BUTTON1}

#define PD4_FUNC                AS_GPIO
#define PD4_OUTPUT_ENABLE       0
#define PD4_INPUT_ENABLE        1

/*--------------------------------------------------------------------------
 * PWM 5채널
 *   CH0  PC2  PWM0  Pin7
 *   CH1  PC3  PWM1  Pin6
 *   CH2  PD2  PWM3  Pin5
 *   CH3  PB4  PWM4  Pin13
 *   CH4  PB5  PWM5  Pin14
 *------------------------------------------------------------------------*/
#define PWM_CH0_PIN             GPIO_PC2
#define PWM_CH0_CH              0
#define PWM_CH0_SET()           do{ gpio_set_func(PWM_CH0_PIN, AS_PWM0); }while(0)

#define PWM_CH1_PIN             GPIO_PC3
#define PWM_CH1_CH              1
#define PWM_CH1_SET()           do{ gpio_set_func(PWM_CH1_PIN, AS_PWM1); }while(0)

#define PWM_CH2_PIN             GPIO_PD2
#define PWM_CH2_CH              3
#define PWM_CH2_SET()           do{ gpio_set_func(PWM_CH2_PIN, AS_PWM3); }while(0)

#define PWM_CH3_PIN             GPIO_PB4
#define PWM_CH3_CH              4
#define PWM_CH3_SET()           do{ gpio_set_func(PWM_CH3_PIN, AS_PWM4); }while(0)

#define PWM_CH4_PIN             GPIO_PB5
#define PWM_CH4_CH              5
#define PWM_CH4_SET()           do{ gpio_set_func(PWM_CH4_PIN, AS_PWM5); }while(0)

#define PWM_CHANNEL_COUNT       5

#define PWM_ALL_CHANNEL_SET()   do{ \
                                    PWM_CH0_SET(); \
                                    PWM_CH1_SET(); \
                                    PWM_CH2_SET(); \
                                    PWM_CH3_SET(); \
                                    PWM_CH4_SET(); \
                                }while(0)

/* SDK gpio_init() 가 참조하는 핀 모드 설정 */
#define PC2_FUNC                AS_PWM0
#define PC2_OUTPUT_ENABLE       1
#define PC2_INPUT_ENABLE        0

#define PC3_FUNC                AS_PWM1
#define PC3_OUTPUT_ENABLE       1
#define PC3_INPUT_ENABLE        0

#define PD2_FUNC                AS_PWM3
#define PD2_OUTPUT_ENABLE       1
#define PD2_INPUT_ENABLE        0

#define PB4_FUNC                AS_PWM4
#define PB4_OUTPUT_ENABLE       1
#define PB4_INPUT_ENABLE        0

#define PB5_FUNC                AS_PWM5
#define PB5_OUTPUT_ENABLE       1
#define PB5_INPUT_ENABLE        0

/*--------------------------------------------------------------------------
 * 상태 LED — PC0 (Pin10), active high
 *------------------------------------------------------------------------*/
#define LED_POWER               GPIO_PC0
#define LED_PERMIT              GPIO_PC0    /* LED_POWER 와 동일 핀 */

#define PC0_FUNC                AS_GPIO
#define PC0_OUTPUT_ENABLE       1
#define PC0_INPUT_ENABLE        0

/*--------------------------------------------------------------------------
 * IR 수신 — PA0 (Pin12), NEC 프로토콜
 *   idle HIGH, 신호 시 LOW 펄스 (TSOP1738 / VS1838B 38kHz 복조 출력)
 *   PA 계열 → GPIO_EN IRQ 지원
 *   PA 그룹 풀업은 analog_write(0x0e) 로 설정 → PULL_WAKEUP_SRC_PA0 필수
 *------------------------------------------------------------------------*/
#define IR_RECV_PIN             GPIO_PA0

#define PA0_FUNC                AS_GPIO
#define PA0_OUTPUT_ENABLE       0
#define PA0_INPUT_ENABLE        1
#define PULL_WAKEUP_SRC_PA0     PM_PIN_PULLUP_10K   /* analog 풀업 — 필수 */

/*--------------------------------------------------------------------------
 * I2C (software bit-bang) — SSD1306 OLED 128x32
 *   SDA  PC4  Pin2   — 출력+입력 (open-drain), 내부 10K 풀업
 *   SCL  PB7  Pin15  — 출력 (클럭), 내부 10K 풀업
 *------------------------------------------------------------------------*/
#define I2C_SW_SDA_PIN          GPIO_PC4
#define I2C_SW_SCL_PIN          GPIO_PB7
#define I2C_SW_HALF_BIT_US      2           /* half-bit 딜레이 → ~250 kHz */

#define PC4_FUNC                AS_GPIO
#define PC4_OUTPUT_ENABLE       0           /* SDA: 초기 Hi-Z, i2c_sw_init()에서 open-drain 제어 */
#define PC4_INPUT_ENABLE        1           /* SDA: ACK 확인을 위해 입력 활성화 */

#define PB7_FUNC                AS_GPIO
#define PB7_OUTPUT_ENABLE       0           /* SCL: 초기 Hi-Z, i2c_sw_init()에서 open-drain 제어 */
#define PB7_INPUT_ENABLE        0

/*--------------------------------------------------------------------------
 * UART / DEBUG
 *   PB1  Pin16  미사용
 *   PB7  Pin15  UART_RX (미사용, I2C_SCL 겸용)
 *   PD7  Pin4   DEBUG soft-TX (printf 출력, UART_PRINTF_MODE=1)
 *------------------------------------------------------------------------*/
/* #define UART_TX_PIN          UART_TX_PB1 */   /* 미사용 */
#define UART_RX_PIN             UART_RX_PB7
#define DEBUG_INFO_TX_PIN       GPIO_PD7

#define PD7_FUNC                AS_GPIO
#define PD7_OUTPUT_ENABLE       1
#define PD7_INPUT_ENABLE        0

#if defined(__cplusplus)
}
#endif
