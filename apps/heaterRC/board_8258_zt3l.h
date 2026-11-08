/*****************************************************************************
 * @file    board_8258_zt3l.h
 * @brief   Tuya ZT3L 모듈 (TLSR8258F1KAT32) — 무시동히터 리모컨 보드 핀 정의
 *
 * PWM5CH 보드와 같은 핀 배치를 사용하되, PWM 채널을 일반 GPIO 출력으로 사용.
 *
 * ZT3L 핀 배치:
 *
 *   LEFT  (위→아래)                RIGHT (위→아래)
 *   Pin1  : RST                    Pin16 : PB1 — (미사용)
 *   Pin2  : PC4 — I2C_SDA         Pin15 : PB7 — I2C_SCL
 *   Pin3  : EN                     Pin14 : PB5 — OUT4  DOWN
 *   Pin4  : PD7 — DEBUG TX         Pin13 : PB4 — OUT3  UP   (PD2 에서 이동)
 *   Pin5  : PD2 — (사용 안 함)       Pin12 : PA0 — IR 수신 입력
 *   Pin6  : PC3 — OUT2  OFF        Pin11 : PD4 — BUTTON1
 *   Pin7  : PC2 — OUT1  ON         Pin10 : PC0 — LED_POWER
 *   Pin8  : 3V3                    Pin9  : GND
 *   Pin17 : SWS (우하단)
 *
 * 히터 버튼 라인 (양방향 — 평소 입력 대기, 누를 때만 출력):
 *   OUT1  GPIO_PC2  Pin7   ON   버튼 (단일 전원 버튼 모드에서는 POWER 버튼)
 *   OUT2  GPIO_PC3  Pin6   OFF  버튼 (단일 전원 버튼 모드에서는 미사용)
 *   OUT3  GPIO_PB4  Pin13  UP   버튼
 *   OUT4  GPIO_PB5  Pin14  DOWN 버튼
 *   ----  GPIO_PD2  Pin5   사용 안 함 — 풀 저항 없는 입력(Hi-Z)
 *         (PD2 에 UP 을 연결하면 이상동작 → PB4 로 이동. 옆 Pin4 PD7 은
 *          디버그 printf 출력(soft-TX)이라, 브리지/간섭 시 로그가 찍힐 때마다
 *          UP 라인이 흔들림 → PD2 는 아무것도 당기지 않게 둠)
 *
 * GPIO:
 *   BUTTON1    GPIO_PD4  Pin11  — 입력, 내부 풀업 (짧게 = 히터 토글, 5초 = factory reset)
 *   LED_POWER  GPIO_PC0  Pin10  — 출력, active high
 *   IR_RECV    GPIO_PA0  Pin12  — NEC IR 수신, PA 계열 GPIO IRQ 지원
 *
 * I2C (software bit-bang) — SSD1306 OLED 128x32:
 *   SDA  GPIO_PC4  Pin2   (내부 풀업)
 *   SCL  GPIO_PB7  Pin15  (내부 풀업)
 *
 * UART:
 *   PD7  Pin4   DEBUG soft-TX (printf 출력, UART_PRINTF_MODE=1)
 *****************************************************************************/
#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/*--------------------------------------------------------------------------
 * Button — BUTTON1 (PD4, Pin11)
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
 * 히터 버튼 라인 — 양방향 GPIO
 *   대기: 입력 + 풀 저항(눌림의 반대 레벨) → 외부 눌림 감지
 *   누름: 눌림 레벨 출력 → 뗌: 출력 끄고 다시 입력
 *   gpio_init() 단계부터 입력으로 두어 부팅 중 버튼이 눌리지 않게 함
 *------------------------------------------------------------------------*/
#define HEATER_PIN_ON           GPIO_PC2    /* OUT1 Pin7  */
#define HEATER_PIN_OFF          GPIO_PC3    /* OUT2 Pin6  */
#define HEATER_PIN_UP           GPIO_PB4    /* OUT3 Pin13 (PD2 에서 이동) */
#define HEATER_PIN_DOWN         GPIO_PB5    /* OUT4 Pin14 */
#define HEATER_PIN_UNUSED       GPIO_PD2    /* Pin5 — 사용 안 함, Hi-Z */
#define HEATER_PIN_POWER        HEATER_PIN_ON   /* 단일 전원 버튼 모드 */

#define HEATER_IDLE_LEVEL       (HEATER_ACTIVE_LEVEL ? 0 : 1)
#define HEATER_IDLE_PULL        (HEATER_ACTIVE_LEVEL ? PM_PIN_PULLDOWN_100K : PM_PIN_PULLUP_10K)

#define PC2_FUNC                AS_GPIO
#define PC2_OUTPUT_ENABLE       0
#define PC2_INPUT_ENABLE        1
#define PC2_DATA_OUT            HEATER_ACTIVE_LEVEL     /* 출력 켤 때 바로 눌림 레벨 */
#define PULL_WAKEUP_SRC_PC2     HEATER_IDLE_PULL

#define PC3_FUNC                AS_GPIO
#define PC3_OUTPUT_ENABLE       0
#define PC3_INPUT_ENABLE        1
#define PC3_DATA_OUT            HEATER_ACTIVE_LEVEL     /* 출력 켤 때 바로 눌림 레벨 */
#define PULL_WAKEUP_SRC_PC3     HEATER_IDLE_PULL


#define PB4_FUNC                AS_GPIO
#define PB4_OUTPUT_ENABLE       0
#define PB4_INPUT_ENABLE        1
#define PB4_DATA_OUT            HEATER_ACTIVE_LEVEL     /* 출력 켤 때 바로 눌림 레벨 */
#define PULL_WAKEUP_SRC_PB4     HEATER_IDLE_PULL

#define PB5_FUNC                AS_GPIO
#define PB5_OUTPUT_ENABLE       0
#define PB5_INPUT_ENABLE        1
#define PB5_DATA_OUT            HEATER_ACTIVE_LEVEL     /* 출력 켤 때 바로 눌림 레벨 */
#define PULL_WAKEUP_SRC_PB5     HEATER_IDLE_PULL

/* PD2 (Pin5) — 사용 안 함: 입력, 풀 저항 없음 (옆 핀/배선에 영향 주지 않음) */
#define PD2_FUNC                AS_GPIO
#define PD2_OUTPUT_ENABLE       0
#define PD2_INPUT_ENABLE        1
#define PULL_WAKEUP_SRC_PD2     PM_PIN_UP_DOWN_FLOAT

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
 *   PA 그룹 풀업은 analog_write(0x0e) 로 설정 → PULL_WAKEUP_SRC_PA0 필수
 *------------------------------------------------------------------------*/
#define IR_RECV_PIN             GPIO_PA0

#define PA0_FUNC                AS_GPIO
#define PA0_OUTPUT_ENABLE       0
#define PA0_INPUT_ENABLE        1
#define PULL_WAKEUP_SRC_PA0     PM_PIN_PULLUP_10K   /* analog 풀업 — 필수 */

/*--------------------------------------------------------------------------
 * I2C (software bit-bang) — SSD1306 OLED 128x32
 *------------------------------------------------------------------------*/
#define I2C_SW_SDA_PIN          GPIO_PC4
#define I2C_SW_SCL_PIN          GPIO_PB7
#define I2C_SW_HALF_BIT_US      2           /* half-bit 딜레이 */

#define PC4_FUNC                AS_GPIO
#define PC4_OUTPUT_ENABLE       0           /* SDA: 초기 Hi-Z */
#define PC4_INPUT_ENABLE        1

#define PB7_FUNC                AS_GPIO
#define PB7_OUTPUT_ENABLE       0           /* SCL: 초기 Hi-Z */
#define PB7_INPUT_ENABLE        0

/*--------------------------------------------------------------------------
 * UART / DEBUG
 *------------------------------------------------------------------------*/
#define UART_RX_PIN             UART_RX_PB7
#define DEBUG_INFO_TX_PIN       GPIO_PD7

#define PD7_FUNC                AS_GPIO
#define PD7_OUTPUT_ENABLE       1
#define PD7_INPUT_ENABLE        0

#if defined(__cplusplus)
}
#endif
