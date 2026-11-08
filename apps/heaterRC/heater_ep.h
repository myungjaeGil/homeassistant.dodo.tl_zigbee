#pragma once
/*********************************************************************
 * @file    heater_ep.h
 * @brief   ZT3L 무시동히터 리모컨 — 엔드포인트 / 속성 선언
 *
 *  EP1 : 히터 전원   OnOff  (ON/OFF 버튼 펄스) + 펄스 설정 / 레벨 / 마지막 입력 속성
 *  EP2 : UP 버튼     OnOff  (On 명령 → UP 펄스 1회, 끝나면 자동 Off)
 *  EP3 : DOWN 버튼   OnOff  (On 명령 → DOWN 펄스 1회, 끝나면 자동 Off)
 *********************************************************************/

#include "zcl_include.h"

/*--------------------------------------------------------------------
 * 엔드포인트 번호
 *------------------------------------------------------------------*/
#define HEATER_EP_POWER         1
#define HEATER_EP_UP            2
#define HEATER_EP_DOWN          3
#define HEATER_EP_COUNT         3

/* g_heaterOnOff[] 인덱스 */
#define HEATER_IDX_POWER        0
#define HEATER_IDX_UP           1
#define HEATER_IDX_DOWN         2

/*--------------------------------------------------------------------
 * 펄스 시간 설정 속성 — EP1 OnOff 클러스터(0x0006), 펌웨어 전용
 *   0xF000 powerPulseMs  UINT16 R/W  ON/OFF 버튼 누름 시간
 *   0xF001 btnPulseMs    UINT16 R/W  UP/DOWN 버튼 누름 시간
 *   0xF002 gapMs         UINT16 R/W  연속 명령 사이 간격
 *   범위를 벗어난 값은 app_cfg.h 의 MIN/MAX 로 보정 후 NV 저장
 *------------------------------------------------------------------*/
#define ZCL_ATTRID_HEATER_POWER_PULSE_MS    0xF000
#define ZCL_ATTRID_HEATER_BTN_PULSE_MS      0xF001
#define ZCL_ATTRID_HEATER_GAP_MS            0xF002

/*--------------------------------------------------------------------
 * 히터 상태 속성 — EP1 OnOff 클러스터, 읽기 + 리포팅 (쓰기 불가)
 *   0xF003 level       UINT8  현재 레벨 HEATER_LEVEL_MIN~MAX (0~9)
 *   0xF004 lastAction  ENUM8  마지막 입력 0=ON 1=OFF 2=UP 3=DOWN 0xFF=없음
 *   0xF005 lastSource  ENUM8  마지막 입력 출처 0=리모컨(이 장치가 누름) 1=외부(라인 감지)
 *------------------------------------------------------------------*/
#define ZCL_ATTRID_HEATER_LEVEL             0xF003
#define ZCL_ATTRID_HEATER_LAST_ACTION       0xF004
#define ZCL_ATTRID_HEATER_LAST_SOURCE       0xF005

#define HEATER_LAST_NONE        0xFF
#define HEATER_SRC_REMOTE       0
#define HEATER_SRC_EXTERNAL     1

extern u8 g_heaterLevel;
extern u8 g_heaterLastAction;
extern u8 g_heaterLastSource;

typedef struct {
    u16 powerPulseMs;
    u16 btnPulseMs;
    u16 gapMs;
} heater_cfg_t;

extern heater_cfg_t g_heaterCfg;

/* OnOff 속성 — [0]=EP1 히터 전원 (직접 누름 + 외부 입력 반영), [1]=EP2 UP, [2]=EP3 DOWN */
extern bool g_heaterOnOff[HEATER_EP_COUNT];

/*--------------------------------------------------------------------
 * Global Attribute
 *------------------------------------------------------------------*/
#ifndef ZCL_ATTRID_GLOBAL_CLUSTER_REVISION
#define ZCL_ATTRID_GLOBAL_CLUSTER_REVISION  0xFFFD
#endif

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
 * Simple Descriptor / 클러스터 리스트
 *------------------------------------------------------------------*/
extern const af_simple_descriptor_t heater_ep1_simpleDesc;
extern const af_simple_descriptor_t heater_ep2_simpleDesc;
extern const af_simple_descriptor_t heater_ep3_simpleDesc;

extern const zcl_specClusterInfo_t * const g_epClusterList[HEATER_EP_COUNT];
extern u8 g_epClusterNum[HEATER_EP_COUNT];

/*--------------------------------------------------------------------
 * ZCL 콜백
 *------------------------------------------------------------------*/
void     heater_zclProcessIncomingMsg(zclIncoming_t *pInHdlrMsg);
status_t heater_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t heater_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t heater_onOffCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);

/*--------------------------------------------------------------------
 * 속성 초기화 / NV
 *------------------------------------------------------------------*/
void heater_attrs_init(void);   /* NV 복원 (전원 상태, 레벨, 펄스 설정) */
void heater_state_save(void);   /* 전원 상태 + 레벨 NV 저장 예약 (지연 일괄 저장) */
bool heater_cfg_check(void);    /* 설정 속성 변경 감지 → 보정·저장, 변경 시 TRUE */
