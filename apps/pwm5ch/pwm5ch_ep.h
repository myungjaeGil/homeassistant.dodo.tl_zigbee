#pragma once
/*********************************************************************
 * @file    pwm5ch_ep.h
 * @brief   ZT3L PWM 5채널 컨트롤러 — 엔드포인트 / 속성 선언
 *
 *  EP1~EP5 : 개별 채널 OnOff + LevelControl (PWM CH0~CH4)
 *  EP6     : 마스터 디밍  OnOff + LevelControl (전채널 동시)
 *********************************************************************/

#include "zcl_include.h"

/*--------------------------------------------------------------------
 * 엔드포인트 번호
 *------------------------------------------------------------------*/
#define PWM_EP_CH0          1   /* CH0 (PC2 PWM0) */
#define PWM_EP_CH1          2   /* CH1 (PC3 PWM1) */
#define PWM_EP_CH2          3   /* CH2 (PD2 PWM3) */
#define PWM_EP_CH3          4   /* CH3 (PB5 PWM5) */
#define PWM_EP_CH4          5   /* CH4 (PB4 PWM4) */
#define PWM_EP_MASTER       6   /* 마스터 디밍    */

#define PWM_EP_COUNT        6

/*--------------------------------------------------------------------
 * PWM 출력단 하드웨어 프리셋
 *
 *   PWM_HW_PC817_4K7  : 현재 보드 (PC817 + R7 4.7kΩ) — 무수정
 *   PWM_HW_PC817_1K   : 현재 보드에서 R7 만 1kΩ 로 교체
 *   PWM_HW_GATE_DRV   : 개정 보드 (UCC27517 게이트 드라이버)
 *
 *   각 프리셋 값은 아래 #ifndef 로 개별 덮어쓰기 가능 (보정 시 사용)
 *------------------------------------------------------------------*/
#define PWM_HW_PC817_4K7        1
#define PWM_HW_PC817_1K         2
#define PWM_HW_GATE_DRV         3

#ifndef PWM_HW_TYPE
#define PWM_HW_TYPE             PWM_HW_PC817_4K7
#endif

/*--------------------------------------------------------------------
 * 프리셋별 기본값
 *
 *   PWM_FREQ_HZ    : PWM 주파수. 18kHz 이상이면 발진음이 들리지 않음
 *   PWM_ON_MIN_US  : 빛이 나기 시작하는 최소 ON 시간 (켜짐 지연 보상)
 *   PWM_OFF_MIN_US : MOSFET 이 완전히 꺼지는 데 필요한 최소 OFF 시간
 *                    (이보다 OFF 가 짧으면 사실상 100% 와 같음 → 꺼짐 꼬리 보상)
 *
 *   레벨 1~253 은 [ON_MIN, 주기-OFF_MIN] 구간 안에만 매핑, 254 = 완전 ON.
 *
 *   PC817 값은 실측 근거로 잡은 "출발점" — 보드마다 다르므로 보정 권장
 *     (24kHz 실측: ON 1.2us 부터 밝아지고 2.9us 에서 포화
 *      → 켜짐 ≈ 1us, 꺼짐 꼬리 ≈ 41.7-2.9 ≈ 39us)
 *------------------------------------------------------------------*/
#if   (PWM_HW_TYPE == PWM_HW_PC817_4K7)
    #define PWM_PRESET_FREQ_HZ      20000
    #define PWM_PRESET_ON_MIN_US    1
    #define PWM_PRESET_OFF_MIN_US   38
#elif (PWM_HW_TYPE == PWM_HW_PC817_1K)
    #define PWM_PRESET_FREQ_HZ      20000
    #define PWM_PRESET_ON_MIN_US    1
    #define PWM_PRESET_OFF_MIN_US   12      /* 추정치 — 반드시 보정 */
#elif (PWM_HW_TYPE == PWM_HW_GATE_DRV)
    #define PWM_PRESET_FREQ_HZ      20000
    #define PWM_PRESET_ON_MIN_US    0
    #define PWM_PRESET_OFF_MIN_US   0
#else
    #error "PWM_HW_TYPE 이 올바르지 않습니다"
#endif

#ifndef PWM_FREQ_HZ
#define PWM_FREQ_HZ             PWM_PRESET_FREQ_HZ
#endif
#ifndef PWM_ON_MIN_US
#define PWM_ON_MIN_US           PWM_PRESET_ON_MIN_US
#endif
#ifndef PWM_OFF_MIN_US
#define PWM_OFF_MIN_US          PWM_PRESET_OFF_MIN_US
#endif

/*--------------------------------------------------------------------
 * PWM tick 계산
 *   PWM 클럭 = 시스템 클럭(48MHz), 분주 없음
 *   PWM_MAX_TICK = 48MHz / PWM_FREQ_HZ  (16bit → 최대 65535, 733Hz 이상)
 *------------------------------------------------------------------*/
#define PWM_TICK_PER_US         (CLOCK_SYS_CLOCK_HZ / 1000000)
#define PWM_MAX_TICK            (CLOCK_SYS_CLOCK_HZ / PWM_FREQ_HZ)
#define PWM_TICK_LO             (PWM_ON_MIN_US  * PWM_TICK_PER_US)
#define PWM_TICK_HI             (PWM_MAX_TICK - PWM_OFF_MIN_US * PWM_TICK_PER_US)

#if (PWM_MAX_TICK > 65535)
#error "PWM_FREQ_HZ 가 너무 낮습니다 (48MHz 기준 733Hz 이상)"
#endif
#if (PWM_ON_MIN_US + PWM_OFF_MIN_US) * PWM_FREQ_HZ >= 900000
#error "ON_MIN + OFF_MIN 이 PWM 주기의 90% 이상 — 주파수를 낮추거나 보정값 확인"
#endif

/*--------------------------------------------------------------------
 * 디밍 노출 구간 (채널별, %) — Zigbee 속성으로 노출, HA/Z2M 에서 변경 가능
 *
 *   HA 슬라이더 1~100% 를 실제 밝기 dimMin% ~ dimMax% 로 재매핑.
 *   밝기 변화가 없는 아래/위 구간을 슬라이더에서 잘라냄.
 *     예) 20 / 80 → 슬라이더 1% = 기존 20% 밝기, 100% = 기존 80% 밝기
 *     0 / 100 → 재매핑 없음
 *
 *   속성 (EP1~EP5, Level Control 클러스터 0x0008):
 *     0xF000  dimMinPct  UINT8  R/W  0~99
 *     0xF001  dimMaxPct  UINT8  R/W  1~100   (항상 min < max 로 보정)
 *   변경값은 NV 에 저장 (NV_ITEM_PWM_DIM_RANGE) — 재부팅/전원 차단 후 유지.
 *   아래 값은 NV 에 저장값이 없을 때(첫 부팅)의 기본값.
 *------------------------------------------------------------------*/
#define ZCL_ATTRID_PWM_DIM_MIN_PCT      0xF000
#define ZCL_ATTRID_PWM_DIM_MAX_PCT      0xF001

#ifndef PWM_LEVEL_MIN_PCT
#define PWM_LEVEL_MIN_PCT       20
#endif
#ifndef PWM_LEVEL_MAX_PCT
#define PWM_LEVEL_MAX_PCT       80
#endif

#if (PWM_LEVEL_MIN_PCT < 0) || (PWM_LEVEL_MAX_PCT > 100) || (PWM_LEVEL_MIN_PCT >= PWM_LEVEL_MAX_PCT)
#error "PWM_LEVEL_MIN_PCT / PWM_LEVEL_MAX_PCT 범위 오류 (0 <= MIN < MAX <= 100)"
#endif

#define PWM_PHY_CH_COUNT        5
extern u8 g_pwmDimMinPct[PWM_PHY_CH_COUNT];     /* 속성 저장소 (CH1~CH5) */
extern u8 g_pwmDimMaxPct[PWM_PHY_CH_COUNT];

void pwm_dimRange_init(void);   /* NV 복원 — pwm_attrs_init() 다음에 호출 */
bool pwm_dimRange_check(void);  /* 속성 변경 감지 → 보정·저장·PWM 재적용, 변경 시 TRUE */

/* 감마 보정 (1 = 사용, 0 = 선형) — 보정값 측정 시에는 0 으로 두면 편함 */
#ifndef PWM_GAMMA_ENABLE
#define PWM_GAMMA_ENABLE        1
#endif

/* 보정용 로그 — 레벨이 바뀔 때 채널별 ON 시간(us) 출력 (DEBUG_ENABLE 필요) */
#ifndef PWM_CAL_LOG
#define PWM_CAL_LOG             0
#endif

/* level(0~254) → PWM 듀티 tick 변환 (0=OFF, 254=100%) — pwm5ch_ctrl.c
 *   ch: 물리 채널 0~4 (채널별 디밍 노출 구간 적용) */
u16 pwm_level_to_tick(u8 ch, u8 lv);
#define LEVEL_TO_TICK(ch, lv)   pwm_level_to_tick((u8)(ch), (u8)(lv))

/*--------------------------------------------------------------------
 * Global Attribute
 *------------------------------------------------------------------*/
#ifndef ZCL_ATTRID_GLOBAL_CLUSTER_REVISION
#define ZCL_ATTRID_GLOBAL_CLUSTER_REVISION  0xFFFD
#endif

/*--------------------------------------------------------------------
 * 채널별 속성 구조체 (OnOff + Level)
 *------------------------------------------------------------------*/
typedef struct {
    bool  onOff;        /* ZCL_ATTRID_ONOFF: 0x0000          */
    u8    currentLevel; /* ZCL_ATTRID_LEVEL_CURRENT_LEVEL: 0x0000 */
    u8    onLevel;      /* ZCL_ATTRID_LEVEL_ON_LEVEL          */
    u16   remainTime;   /* ZCL_ATTRID_LEVEL_REMAINING_TIME    */
    u8    startUpOnOff; /* Start-Up OnOff (0=Off, 1=On, 2=Toggle, 0xFF=Prev) */
    u8    startUpLevel; /* Start-Up Level (0xFF=Prev)          */
    u16   onTime;       /* ZCL_ATTRID_ONOFF_ON_TIME           */
    u16   offWaitTime;  /* ZCL_ATTRID_ONOFF_OFF_WAIT_TIME     */
} pwm_ch_attr_t;

/*--------------------------------------------------------------------
 * 전역 채널 속성 (CH0~CH4 + MASTER)
 *------------------------------------------------------------------*/
extern pwm_ch_attr_t g_pwmChAttrs[PWM_EP_COUNT]; /* [0]=EP1/CH0 ... [5]=EP6/MASTER */

/*--------------------------------------------------------------------
 * Basic / Identify
 *------------------------------------------------------------------*/
#ifndef ZCL_BASIC_ATTR_T_DEFINED
#define ZCL_BASIC_ATTR_T_DEFINED
typedef struct {
    u8 zclVersion;
    u8 appVersion;
    u8 stackVersion;
    u8 hwVersion;
    u8 manuName[ZCL_BASIC_MAX_LENGTH];
    u8 modelId[ZCL_BASIC_MAX_LENGTH];
    u8 swBuildId[ZCL_BASIC_MAX_LENGTH];
    u8 powerSource;
    u8 deviceEnable;
} zcl_basicAttr_t;
#endif

#ifndef ZCL_IDENTIFY_ATTR_T_DEFINED
#define ZCL_IDENTIFY_ATTR_T_DEFINED
typedef struct {
    u16 identifyTime;
} zcl_identifyAttr_t;
#endif

extern zcl_basicAttr_t    g_zcl_basicAttrs;
extern zcl_identifyAttr_t g_zcl_identifyAttrs;

/*--------------------------------------------------------------------
 * Simple Descriptor (EP1~EP6)
 *------------------------------------------------------------------*/
extern const af_simple_descriptor_t pwm_ep1_simpleDesc;
extern const af_simple_descriptor_t pwm_ep2_simpleDesc;
extern const af_simple_descriptor_t pwm_ep3_simpleDesc;
extern const af_simple_descriptor_t pwm_ep4_simpleDesc;
extern const af_simple_descriptor_t pwm_ep5_simpleDesc;
extern const af_simple_descriptor_t pwm_ep6_simpleDesc;

/*--------------------------------------------------------------------
 * 클러스터 리스트
 *------------------------------------------------------------------*/
extern const zcl_specClusterInfo_t * const g_epClusterList[PWM_EP_COUNT];
extern u8 g_epClusterNum[PWM_EP_COUNT];

/*--------------------------------------------------------------------
 * ZCL 콜백
 *------------------------------------------------------------------*/
status_t pwm_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t pwm_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t pwm_onOffCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t pwm_levelCtrlCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);

/*--------------------------------------------------------------------
 * 초기화 / 업데이트
 *------------------------------------------------------------------*/
void pwm_attrs_init(void);
void pwm_hw_apply(u8 ep_idx); /* ep_idx: 0=CH0 ... 4=CH4, 5=MASTER */

/* NV 저장 — 즉시 쓰지 않고 마지막 변경 후 PWM_NV_SAVE_DELAY_MS 뒤 일괄 저장 */
#define PWM_NV_SAVE_DELAY_MS    2000
void pwm_attrs_save(u8 ep_idx);     /* 저장 예약 */
void pwm_attrs_flush(void);         /* 예약분 즉시 저장 */
