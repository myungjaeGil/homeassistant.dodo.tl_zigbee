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
#define PWM_EP_CH3          4   /* CH3 (PB4 PWM4) */
#define PWM_EP_CH4          5   /* CH4 (PB5 PWM5) */
#define PWM_EP_MASTER       6   /* 마스터 디밍    */

#define PWM_EP_COUNT        6

/*--------------------------------------------------------------------
 * PWM 하드웨어 설정
 *
 * [수정] PWM_MAX_TICK 1000 → 200 으로 축소, 주파수 5배 상승
 *   - 기존 1000 tick 설정으로 계산상 48kHz 였음에도 실측 플리커 발생
 *     → TLSR8258 의 clock_time()/타이머 계열이 시스템 클럭 설정값과
 *       달리 실제로는 16MHz 도메인에서 동작하는 사례가 있었음
 *       (IR 수신 NEC 타이밍 디버깅에서 동일 현상 확인).
 *       PWM 모듈도 동일하게 실제 입력 클럭이 설계값보다 낮을 가능성을
 *       배제할 수 없어, 계산값에 의존하지 않고 tick 수 자체를 줄여
 *       실제 클럭이 무엇이든 비례해서 주파수가 오르도록 함.
 *   - LEVEL_TO_TICK 매핑은 0~254 그대로 유지되므로 디밍 분해능 손실 없음
 *     (PWM 카운터가 200 tick 분해능을 가지면 254단계 매핑 시 일부 단계가
 *      동일 duty로 겹칠 수 있으나, 육안 디밍 곡선에는 거의 영향 없음)
 *------------------------------------------------------------------*/
#define PWM_CLK_DIV         0           /* 분주 없음 */
#define PWM_MAX_TICK        2000        /* 주기 축소 → 주파수 5배 (48kHz→240kHz 또는 16kHz→80kHz) */

/* level(0~254) → PWM 듀티 tick 변환 (0=OFF, 254=100%) */
#define LEVEL_TO_TICK(lv)   ((u16)(((u32)(lv) * PWM_MAX_TICK + 127) / 254))

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
