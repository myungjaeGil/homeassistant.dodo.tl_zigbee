/*********************************************************************
 * @file    pwm5ch_epCfg.c
 * @brief   ZT3L PWM 5채널 컨트롤러 — 엔드포인트 구성
 *
 *  EP1~EP5 : 개별 채널 OnOff + LevelControl
 *  EP6     : 마스터 디밍 OnOff + LevelControl
 *********************************************************************/

#include "tl_common.h"
#include "zcl_include.h"
#include "pwm5ch_ep.h"

extern u8 zcl_seqNum;

/*====================================================================
 * NV 저장 ID
 *==================================================================*/
#define NV_ITEM_PWM_CH_BASE     0x30  /* 0x30~0x35 (채널 6개) */
#define NV_ITEM_PWM_DIM_RANGE   0x36  /* 채널별 디밍 노출 구간 (min/max %) */

/*====================================================================
 * OnOff 확장 속성 ID — SDK에 없는 경우 직접 정의
 *==================================================================*/
#ifndef ZCL_ATTRID_ONOFF_ON_TIME
#define ZCL_ATTRID_ONOFF_ON_TIME            0x4001
#endif
#ifndef ZCL_ATTRID_ONOFF_OFF_WAIT_TIME
#define ZCL_ATTRID_ONOFF_OFF_WAIT_TIME      0x4002
#endif
#ifndef ZCL_ATTRID_ONOFF_START_UP_ONOFF
#define ZCL_ATTRID_ONOFF_START_UP_ONOFF     0x4003
#endif
#ifndef ZCL_ATTRID_LEVEL_START_UP_CURRENT_LEVEL
#define ZCL_ATTRID_LEVEL_START_UP_CURRENT_LEVEL  0x4000
#endif

/*====================================================================
 * Basic / Identify 속성
 *==================================================================*/
#ifndef ZCL_BASIC_MFG_NAME
#define ZCL_BASIC_MFG_NAME    {4, 'D','O','D','O'}
#endif
#ifndef ZCL_BASIC_MODEL_ID
#define ZCL_BASIC_MODEL_ID    {11, 'Z','T','3','L','-','P','W','M','5','C','H'}
#endif
#ifndef ZCL_BASIC_SW_BUILD_ID
#define ZCL_BASIC_SW_BUILD_ID {5, '1','.','0','.','0'}
#endif

zcl_basicAttr_t g_zcl_basicAttrs = {
    .zclVersion   = 0x03,
    .appVersion   = 0x01,
    .stackVersion = 0x02,
    .hwVersion    = 0x00,
    .manuName     = ZCL_BASIC_MFG_NAME,
    .modelId      = ZCL_BASIC_MODEL_ID,
    .powerSource  = POWER_SOURCE_MAINS_1_PHASE,
    .swBuildId    = ZCL_BASIC_SW_BUILD_ID,
    .deviceEnable = TRUE,
};

static const zclAttrInfo_t basic_attrTbl[] = {
    { ZCL_ATTRID_BASIC_ZCL_VER,           ZCL_DATA_TYPE_UINT8,    ACCESS_CONTROL_READ,                        (u8*)&g_zcl_basicAttrs.zclVersion },
    { ZCL_ATTRID_BASIC_APP_VER,           ZCL_DATA_TYPE_UINT8,    ACCESS_CONTROL_READ,                        (u8*)&g_zcl_basicAttrs.appVersion },
    { ZCL_ATTRID_BASIC_STACK_VER,         ZCL_DATA_TYPE_UINT8,    ACCESS_CONTROL_READ,                        (u8*)&g_zcl_basicAttrs.stackVersion },
    { ZCL_ATTRID_BASIC_HW_VER,            ZCL_DATA_TYPE_UINT8,    ACCESS_CONTROL_READ,                        (u8*)&g_zcl_basicAttrs.hwVersion },
    { ZCL_ATTRID_BASIC_MFR_NAME,          ZCL_DATA_TYPE_CHAR_STR, ACCESS_CONTROL_READ,                        (u8*)g_zcl_basicAttrs.manuName },
    { ZCL_ATTRID_BASIC_MODEL_ID,          ZCL_DATA_TYPE_CHAR_STR, ACCESS_CONTROL_READ,                        (u8*)g_zcl_basicAttrs.modelId },
    { ZCL_ATTRID_BASIC_POWER_SOURCE,      ZCL_DATA_TYPE_ENUM8,    ACCESS_CONTROL_READ,                        (u8*)&g_zcl_basicAttrs.powerSource },
    { ZCL_ATTRID_BASIC_DEV_ENABLED,       ZCL_DATA_TYPE_BOOLEAN,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_zcl_basicAttrs.deviceEnable },
    { ZCL_ATTRID_BASIC_SW_BUILD_ID,       ZCL_DATA_TYPE_CHAR_STR, ACCESS_CONTROL_READ,                        (u8*)g_zcl_basicAttrs.swBuildId },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,   ACCESS_CONTROL_READ,                        (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_BASIC_ATTR_NUM  (sizeof(basic_attrTbl) / sizeof(zclAttrInfo_t))

zcl_identifyAttr_t g_zcl_identifyAttrs = { .identifyTime = 0 };

static const zclAttrInfo_t identify_attrTbl[] = {
    { ZCL_ATTRID_IDENTIFY_TIME,           ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_zcl_identifyAttrs.identifyTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                        (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_IDENTIFY_ATTR_NUM (sizeof(identify_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * Groups 클러스터 — HA/Z2M 에서 Zigbee 그룹 추가 가능하도록 등록
 *==================================================================*/
#ifdef ZCL_GROUP
static u8 g_zcl_group_nameSupport = 0;

static const zclAttrInfo_t group_attrTbl[] = {
    { ZCL_ATTRID_GROUP_NAME_SUPPORT,      ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ, (u8*)&g_zcl_group_nameSupport },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ, (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_GROUP_ATTR_NUM  (sizeof(group_attrTbl) / sizeof(zclAttrInfo_t))
#define GROUP_CLUSTER_ENTRY { ZCL_CLUSTER_GEN_GROUPS, MANUFACTURER_CODE_NONE, ZCL_GROUP_ATTR_NUM, group_attrTbl, zcl_group_register, NULL },
#else
#define GROUP_CLUSTER_ENTRY
#endif

/*====================================================================
 * 디밍 노출 구간 속성 저장소 (CH1~CH5) — Level 클러스터 0xF000/0xF001
 *==================================================================*/
u8 g_pwmDimMinPct[PWM_PHY_CH_COUNT] = {
    PWM_LEVEL_MIN_PCT, PWM_LEVEL_MIN_PCT, PWM_LEVEL_MIN_PCT, PWM_LEVEL_MIN_PCT, PWM_LEVEL_MIN_PCT
};
u8 g_pwmDimMaxPct[PWM_PHY_CH_COUNT] = {
    PWM_LEVEL_MAX_PCT, PWM_LEVEL_MAX_PCT, PWM_LEVEL_MAX_PCT, PWM_LEVEL_MAX_PCT, PWM_LEVEL_MAX_PCT
};

/*====================================================================
 * 채널 속성 배열 (EP1=idx0 ... EP6=idx5)
 *==================================================================*/
pwm_ch_attr_t g_pwmChAttrs[PWM_EP_COUNT] = {
    /* CH0 */ { FALSE, 254, 0xFF, 0, 0xFF, 0xFF, 0, 0 },
    /* CH1 */ { FALSE, 254, 0xFF, 0, 0xFF, 0xFF, 0, 0 },
    /* CH2 */ { FALSE, 254, 0xFF, 0, 0xFF, 0xFF, 0, 0 },
    /* CH3 */ { FALSE, 254, 0xFF, 0, 0xFF, 0xFF, 0, 0 },
    /* CH4 */ { FALSE, 254, 0xFF, 0, 0xFF, 0xFF, 0, 0 },
    /* MST */ { FALSE, 254, 0xFF, 0, 0xFF, 0xFF, 0, 0 },
};

/*====================================================================
 * OnOff 속성 테이블 — 채널별 개별 선언 (TC32: static const 배열은 글로벌 주소만 허용)
 *==================================================================*/
static const zclAttrInfo_t ch0_oo_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                   ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[0].onOff },
    { ZCL_ATTRID_ONOFF_ON_TIME,           ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[0].onTime },
    { ZCL_ATTRID_ONOFF_OFF_WAIT_TIME,     ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[0].offWaitTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
static const zclAttrInfo_t ch1_oo_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                   ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[1].onOff },
    { ZCL_ATTRID_ONOFF_ON_TIME,           ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[1].onTime },
    { ZCL_ATTRID_ONOFF_OFF_WAIT_TIME,     ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[1].offWaitTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
static const zclAttrInfo_t ch2_oo_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                   ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[2].onOff },
    { ZCL_ATTRID_ONOFF_ON_TIME,           ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[2].onTime },
    { ZCL_ATTRID_ONOFF_OFF_WAIT_TIME,     ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[2].offWaitTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
static const zclAttrInfo_t ch3_oo_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                   ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[3].onOff },
    { ZCL_ATTRID_ONOFF_ON_TIME,           ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[3].onTime },
    { ZCL_ATTRID_ONOFF_OFF_WAIT_TIME,     ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[3].offWaitTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
static const zclAttrInfo_t ch4_oo_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                   ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[4].onOff },
    { ZCL_ATTRID_ONOFF_ON_TIME,           ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[4].onTime },
    { ZCL_ATTRID_ONOFF_OFF_WAIT_TIME,     ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[4].offWaitTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
static const zclAttrInfo_t mst_oo_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                   ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[5].onOff },
    { ZCL_ATTRID_ONOFF_ON_TIME,           ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[5].onTime },
    { ZCL_ATTRID_ONOFF_OFF_WAIT_TIME,     ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[5].offWaitTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_CH_ONOFF_ATTR_NUM  (sizeof(ch0_oo_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * LevelControl 속성 테이블 — 채널별 개별 선언
 *==================================================================*/
static const zclAttrInfo_t ch0_lv_attrTbl[] = {
    { ZCL_ATTRID_LEVEL_CURRENT_LEVEL,  ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[0].currentLevel },
    { ZCL_ATTRID_LEVEL_REMAINING_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_pwmChAttrs[0].remainTime },
    { ZCL_ATTRID_LEVEL_ON_LEVEL,       ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[0].onLevel },
    { ZCL_ATTRID_PWM_DIM_MIN_PCT,      ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmDimMinPct[0] },
    { ZCL_ATTRID_PWM_DIM_MAX_PCT,      ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmDimMaxPct[0] },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                          (u8*)&zcl_attr_global_clusterRevision },
};
static const zclAttrInfo_t ch1_lv_attrTbl[] = {
    { ZCL_ATTRID_LEVEL_CURRENT_LEVEL,  ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[1].currentLevel },
    { ZCL_ATTRID_LEVEL_REMAINING_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_pwmChAttrs[1].remainTime },
    { ZCL_ATTRID_LEVEL_ON_LEVEL,       ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[1].onLevel },
    { ZCL_ATTRID_PWM_DIM_MIN_PCT,      ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmDimMinPct[1] },
    { ZCL_ATTRID_PWM_DIM_MAX_PCT,      ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmDimMaxPct[1] },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                          (u8*)&zcl_attr_global_clusterRevision },
};
static const zclAttrInfo_t ch2_lv_attrTbl[] = {
    { ZCL_ATTRID_LEVEL_CURRENT_LEVEL,  ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[2].currentLevel },
    { ZCL_ATTRID_LEVEL_REMAINING_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_pwmChAttrs[2].remainTime },
    { ZCL_ATTRID_LEVEL_ON_LEVEL,       ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[2].onLevel },
    { ZCL_ATTRID_PWM_DIM_MIN_PCT,      ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmDimMinPct[2] },
    { ZCL_ATTRID_PWM_DIM_MAX_PCT,      ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmDimMaxPct[2] },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                          (u8*)&zcl_attr_global_clusterRevision },
};
static const zclAttrInfo_t ch3_lv_attrTbl[] = {
    { ZCL_ATTRID_LEVEL_CURRENT_LEVEL,  ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[3].currentLevel },
    { ZCL_ATTRID_LEVEL_REMAINING_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_pwmChAttrs[3].remainTime },
    { ZCL_ATTRID_LEVEL_ON_LEVEL,       ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[3].onLevel },
    { ZCL_ATTRID_PWM_DIM_MIN_PCT,      ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmDimMinPct[3] },
    { ZCL_ATTRID_PWM_DIM_MAX_PCT,      ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmDimMaxPct[3] },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                          (u8*)&zcl_attr_global_clusterRevision },
};
static const zclAttrInfo_t ch4_lv_attrTbl[] = {
    { ZCL_ATTRID_LEVEL_CURRENT_LEVEL,  ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[4].currentLevel },
    { ZCL_ATTRID_LEVEL_REMAINING_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_pwmChAttrs[4].remainTime },
    { ZCL_ATTRID_LEVEL_ON_LEVEL,       ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[4].onLevel },
    { ZCL_ATTRID_PWM_DIM_MIN_PCT,      ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmDimMinPct[4] },
    { ZCL_ATTRID_PWM_DIM_MAX_PCT,      ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmDimMaxPct[4] },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                          (u8*)&zcl_attr_global_clusterRevision },
};
static const zclAttrInfo_t mst_lv_attrTbl[] = {
    { ZCL_ATTRID_LEVEL_CURRENT_LEVEL,  ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_pwmChAttrs[5].currentLevel },
    { ZCL_ATTRID_LEVEL_REMAINING_TIME, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_pwmChAttrs[5].remainTime },
    { ZCL_ATTRID_LEVEL_ON_LEVEL,       ZCL_DATA_TYPE_UINT8,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_pwmChAttrs[5].onLevel },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                          (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_CH_LVL_ATTR_NUM  (sizeof(ch0_lv_attrTbl) / sizeof(zclAttrInfo_t))
#define ZCL_MST_LVL_ATTR_NUM (sizeof(mst_lv_attrTbl) / sizeof(zclAttrInfo_t))   /* 마스터는 디밍 구간 속성 없음 */

/*====================================================================
 * Simple Descriptor
 *==================================================================*/
static const u16 ep_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
#ifdef ZCL_GROUP
    ZCL_CLUSTER_GEN_GROUPS,
#endif
    ZCL_CLUSTER_GEN_ON_OFF,
    ZCL_CLUSTER_GEN_LEVEL_CONTROL,
};
#define EP_IN_CLUSTER_NUM   (sizeof(ep_inClusterList) / sizeof(u16))

/* OTA 클라이언트는 EP1 에만 노출 (ota_init 도 EP1 디스크립터 사용) */
static const u16 ep1_outClusterList[] = {
#ifdef ZCL_OTA
    ZCL_CLUSTER_OTA,
#endif
};
#define EP1_OUT_CLUSTER_NUM (sizeof(ep1_outClusterList) / sizeof(u16))

const af_simple_descriptor_t pwm_ep1_simpleDesc = {
    HA_PROFILE_ID, HA_DEV_DIMMABLE_LIGHT, PWM_EP_CH0, 1, 0,
    EP_IN_CLUSTER_NUM, EP1_OUT_CLUSTER_NUM,
    (u16*)ep_inClusterList, (u16*)ep1_outClusterList
};
const af_simple_descriptor_t pwm_ep2_simpleDesc = {
    HA_PROFILE_ID, HA_DEV_DIMMABLE_LIGHT, PWM_EP_CH1, 1, 0,
    EP_IN_CLUSTER_NUM, 0,
    (u16*)ep_inClusterList, NULL
};
const af_simple_descriptor_t pwm_ep3_simpleDesc = {
    HA_PROFILE_ID, HA_DEV_DIMMABLE_LIGHT, PWM_EP_CH2, 1, 0,
    EP_IN_CLUSTER_NUM, 0,
    (u16*)ep_inClusterList, NULL
};
const af_simple_descriptor_t pwm_ep4_simpleDesc = {
    HA_PROFILE_ID, HA_DEV_DIMMABLE_LIGHT, PWM_EP_CH3, 1, 0,
    EP_IN_CLUSTER_NUM, 0,
    (u16*)ep_inClusterList, NULL
};
const af_simple_descriptor_t pwm_ep5_simpleDesc = {
    HA_PROFILE_ID, HA_DEV_DIMMABLE_LIGHT, PWM_EP_CH4, 1, 0,
    EP_IN_CLUSTER_NUM, 0,
    (u16*)ep_inClusterList, NULL
};
const af_simple_descriptor_t pwm_ep6_simpleDesc = {
    HA_PROFILE_ID, HA_DEV_DIMMABLE_LIGHT, PWM_EP_MASTER, 1, 0,
    EP_IN_CLUSTER_NUM, 0,
    (u16*)ep_inClusterList, NULL
};

/*====================================================================
 * zcl_specClusterInfo_t 테이블
 *==================================================================*/
static const zcl_specClusterInfo_t ep1_clusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,         MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)pwm_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,      MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)pwm_identifyCb },
    GROUP_CLUSTER_ENTRY
    { ZCL_CLUSTER_GEN_ON_OFF,        MANUFACTURER_CODE_NONE, ZCL_CH_ONOFF_ATTR_NUM, ch0_oo_attrTbl,   zcl_onOff_register,    (void*)pwm_onOffCb    },
    { ZCL_CLUSTER_GEN_LEVEL_CONTROL, MANUFACTURER_CODE_NONE, ZCL_CH_LVL_ATTR_NUM,   ch0_lv_attrTbl,   zcl_level_register,    (void*)pwm_levelCtrlCb},
};
static const zcl_specClusterInfo_t ep2_clusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,         MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)pwm_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,      MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)pwm_identifyCb },
    GROUP_CLUSTER_ENTRY
    { ZCL_CLUSTER_GEN_ON_OFF,        MANUFACTURER_CODE_NONE, ZCL_CH_ONOFF_ATTR_NUM, ch1_oo_attrTbl,   zcl_onOff_register,    (void*)pwm_onOffCb    },
    { ZCL_CLUSTER_GEN_LEVEL_CONTROL, MANUFACTURER_CODE_NONE, ZCL_CH_LVL_ATTR_NUM,   ch1_lv_attrTbl,   zcl_level_register,    (void*)pwm_levelCtrlCb},
};
static const zcl_specClusterInfo_t ep3_clusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,         MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)pwm_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,      MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)pwm_identifyCb },
    GROUP_CLUSTER_ENTRY
    { ZCL_CLUSTER_GEN_ON_OFF,        MANUFACTURER_CODE_NONE, ZCL_CH_ONOFF_ATTR_NUM, ch2_oo_attrTbl,   zcl_onOff_register,    (void*)pwm_onOffCb    },
    { ZCL_CLUSTER_GEN_LEVEL_CONTROL, MANUFACTURER_CODE_NONE, ZCL_CH_LVL_ATTR_NUM,   ch2_lv_attrTbl,   zcl_level_register,    (void*)pwm_levelCtrlCb},
};
static const zcl_specClusterInfo_t ep4_clusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,         MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)pwm_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,      MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)pwm_identifyCb },
    GROUP_CLUSTER_ENTRY
    { ZCL_CLUSTER_GEN_ON_OFF,        MANUFACTURER_CODE_NONE, ZCL_CH_ONOFF_ATTR_NUM, ch3_oo_attrTbl,   zcl_onOff_register,    (void*)pwm_onOffCb    },
    { ZCL_CLUSTER_GEN_LEVEL_CONTROL, MANUFACTURER_CODE_NONE, ZCL_CH_LVL_ATTR_NUM,   ch3_lv_attrTbl,   zcl_level_register,    (void*)pwm_levelCtrlCb},
};
static const zcl_specClusterInfo_t ep5_clusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,         MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)pwm_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,      MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)pwm_identifyCb },
    GROUP_CLUSTER_ENTRY
    { ZCL_CLUSTER_GEN_ON_OFF,        MANUFACTURER_CODE_NONE, ZCL_CH_ONOFF_ATTR_NUM, ch4_oo_attrTbl,   zcl_onOff_register,    (void*)pwm_onOffCb    },
    { ZCL_CLUSTER_GEN_LEVEL_CONTROL, MANUFACTURER_CODE_NONE, ZCL_CH_LVL_ATTR_NUM,   ch4_lv_attrTbl,   zcl_level_register,    (void*)pwm_levelCtrlCb},
};
static const zcl_specClusterInfo_t ep6_clusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,         MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)pwm_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,      MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)pwm_identifyCb },
    GROUP_CLUSTER_ENTRY
    { ZCL_CLUSTER_GEN_ON_OFF,        MANUFACTURER_CODE_NONE, ZCL_CH_ONOFF_ATTR_NUM, mst_oo_attrTbl,   zcl_onOff_register,    (void*)pwm_onOffCb    },
    { ZCL_CLUSTER_GEN_LEVEL_CONTROL, MANUFACTURER_CODE_NONE, ZCL_MST_LVL_ATTR_NUM,  mst_lv_attrTbl,   zcl_level_register,    (void*)pwm_levelCtrlCb},
};

u8 g_epClusterNum[PWM_EP_COUNT] = {
    sizeof(ep1_clusterList) / sizeof(zcl_specClusterInfo_t),
    sizeof(ep2_clusterList) / sizeof(zcl_specClusterInfo_t),
    sizeof(ep3_clusterList) / sizeof(zcl_specClusterInfo_t),
    sizeof(ep4_clusterList) / sizeof(zcl_specClusterInfo_t),
    sizeof(ep5_clusterList) / sizeof(zcl_specClusterInfo_t),
    sizeof(ep6_clusterList) / sizeof(zcl_specClusterInfo_t),
};

const zcl_specClusterInfo_t * const g_epClusterList[PWM_EP_COUNT] = {
    ep1_clusterList,
    ep2_clusterList,
    ep3_clusterList,
    ep4_clusterList,
    ep5_clusterList,
    ep6_clusterList,
};

/*====================================================================
 * pwm_attrs_init — NV 복원
 *==================================================================*/
void pwm_attrs_init(void)
{
    u8 i;
    for (i = 0; i < PWM_EP_COUNT; i++) {
        pwm_ch_attr_t saved;
        nv_sts_t st = nv_flashReadNew(1, NV_MODULE_APP,
                                      NV_ITEM_PWM_CH_BASE + i,
                                      sizeof(pwm_ch_attr_t),
                                      (u8*)&saved);
        if (st == NV_SUCC) {
            /* [마이그레이션] 이전 펌웨어는 IR 조작 시 startUpOnOff/Level 을
             * 고정값(0x00/0x01/레벨)으로 저장했음. 이 속성들은 외부에서
             * 쓸 수 없으므로 항상 0xFF(이전 상태 복원)로 정상화한다. */
            saved.startUpOnOff = 0xFF;
            saved.startUpLevel = 0xFF;

            u8 suo = saved.startUpOnOff;
            if (suo == 0x00)      g_pwmChAttrs[i].onOff = FALSE;
            else if (suo == 0x01) g_pwmChAttrs[i].onOff = TRUE;
            else if (suo == 0x02) g_pwmChAttrs[i].onOff = !saved.onOff;
            else                  g_pwmChAttrs[i].onOff = saved.onOff;

            g_pwmChAttrs[i].currentLevel = (saved.startUpLevel == 0xFF)
                                           ? saved.currentLevel
                                           : saved.startUpLevel;
            if (g_pwmChAttrs[i].currentLevel < 1)   g_pwmChAttrs[i].currentLevel = 1;
            if (g_pwmChAttrs[i].currentLevel > 254) g_pwmChAttrs[i].currentLevel = 254;
            g_pwmChAttrs[i].onLevel      = saved.onLevel;
            g_pwmChAttrs[i].startUpOnOff = saved.startUpOnOff;
            g_pwmChAttrs[i].startUpLevel = saved.startUpLevel;
            g_pwmChAttrs[i].onTime       = 0;
            g_pwmChAttrs[i].offWaitTime  = 0;
            g_pwmChAttrs[i].remainTime   = 0;
            DBG_LOG("[NV] CH%d restored: onOff=%d lv=%d suOnOff=0x%02x suLv=0x%02x\r\n",
                    (int)i,
                    (int)g_pwmChAttrs[i].onOff,
                    (int)g_pwmChAttrs[i].currentLevel,
                    (int)saved.startUpOnOff,
                    (int)saved.startUpLevel);
        } else {
            /* NV 미발견 시 기본값 설정 */
            g_pwmChAttrs[i].onOff        = FALSE;
            g_pwmChAttrs[i].currentLevel = 254;   /* MASTER 포함 전채널 254 */
            g_pwmChAttrs[i].startUpOnOff = 0xFF;
            g_pwmChAttrs[i].startUpLevel = 0xFF;
            DBG_LOG("[NV] CH%d not found (st=%d) -> default\r\n", (int)i, (int)st);
        }
    }
}

/*====================================================================
 * pwm_attrs_save — NV 저장 (지연 일괄 저장)
 *   IR UP/DN 연속 입력, ALL ON/OFF 등에서 플래시 쓰기가 몰리지 않도록
 *   마지막 변경 후 PWM_NV_SAVE_DELAY_MS 가 지나면 변경된 채널만 저장
 *==================================================================*/
static u8                s_nv_dirty_mask = 0;
static ev_timer_event_t *s_nv_save_tmr   = NULL;

void pwm_attrs_flush(void)
{
    u8 i;
    for (i = 0; i < PWM_EP_COUNT; i++) {
        if (s_nv_dirty_mask & (1 << i)) {
            nv_flashWriteNew(1, NV_MODULE_APP,
                             NV_ITEM_PWM_CH_BASE + i,
                             sizeof(pwm_ch_attr_t),
                             (u8*)&g_pwmChAttrs[i]);
        }
    }
    s_nv_dirty_mask = 0;
}

static s32 pwm_nv_save_cb(void *arg)
{
    (void)arg;
    s_nv_save_tmr = NULL;
    pwm_attrs_flush();
    return -1;
}

void pwm_attrs_save(u8 ep_idx)
{
    if (ep_idx >= PWM_EP_COUNT) return;
    s_nv_dirty_mask |= (1 << ep_idx);

    /* 재시작 방식 — 연속 변경 중에는 저장 미룸 */
    if (s_nv_save_tmr) {
        TL_ZB_TIMER_CANCEL(&s_nv_save_tmr);
        s_nv_save_tmr = NULL;
    }
    s_nv_save_tmr = TL_ZB_TIMER_SCHEDULE(pwm_nv_save_cb, NULL, PWM_NV_SAVE_DELAY_MS);
}

/*====================================================================
 * 디밍 노출 구간 — NV 복원 / 변경 감지·보정·저장
 *==================================================================*/
typedef struct {
    u8 minPct[PWM_PHY_CH_COUNT];
    u8 maxPct[PWM_PHY_CH_COUNT];
} pwm_dim_range_nv_t;

static pwm_dim_range_nv_t s_dimSaved;   /* 마지막으로 NV 에 저장된 값 */

/* 범위 보정: 0 <= min < max <= 100 */
static bool pwm_dimRange_sanitize(u8 i)
{
    u8 mn = g_pwmDimMinPct[i];
    u8 mx = g_pwmDimMaxPct[i];
    u8 omn = mn, omx = mx;

    if (mx > 100) mx = 100;
    if (mx < 1)   mx = 1;
    if (mn > 99)  mn = 99;
    if (mn >= mx) {
        /* 방금 바뀐 쪽을 존중: max 가 바뀌었으면 min 을 내리고, 아니면 max 를 올림 */
        if (mx != s_dimSaved.maxPct[i]) mn = mx - 1;
        else                            mx = mn + 1;
    }
    g_pwmDimMinPct[i] = mn;
    g_pwmDimMaxPct[i] = mx;
    return (mn != omn) || (mx != omx);
}

void pwm_dimRange_init(void)
{
    pwm_dim_range_nv_t nv;
    u8 i;

    if (nv_flashReadNew(1, NV_MODULE_APP, NV_ITEM_PWM_DIM_RANGE,
                        sizeof(nv), (u8 *)&nv) == NV_SUCC) {
        for (i = 0; i < PWM_PHY_CH_COUNT; i++) {
            g_pwmDimMinPct[i] = nv.minPct[i];
            g_pwmDimMaxPct[i] = nv.maxPct[i];
        }
    }
    for (i = 0; i < PWM_PHY_CH_COUNT; i++) {
        s_dimSaved.minPct[i] = g_pwmDimMinPct[i];
        s_dimSaved.maxPct[i] = g_pwmDimMaxPct[i];
        pwm_dimRange_sanitize(i);
        s_dimSaved.minPct[i] = g_pwmDimMinPct[i];
        s_dimSaved.maxPct[i] = g_pwmDimMaxPct[i];
        DBG_LOG("[DIM] CH%d range %d~%d%%\r\n", (int)(i + 1),
                (int)g_pwmDimMinPct[i], (int)g_pwmDimMaxPct[i]);
    }
}

bool pwm_dimRange_check(void)
{
    bool changed = FALSE;
    u8 i;

    for (i = 0; i < PWM_PHY_CH_COUNT; i++) {
        if (g_pwmDimMinPct[i] != s_dimSaved.minPct[i] ||
            g_pwmDimMaxPct[i] != s_dimSaved.maxPct[i]) {
            pwm_dimRange_sanitize(i);
            changed = TRUE;
        }
    }
    if (!changed) return FALSE;

    for (i = 0; i < PWM_PHY_CH_COUNT; i++) {
        s_dimSaved.minPct[i] = g_pwmDimMinPct[i];
        s_dimSaved.maxPct[i] = g_pwmDimMaxPct[i];
        DBG_LOG("[DIM] CH%d range %d~%d%%\r\n", (int)(i + 1),
                (int)g_pwmDimMinPct[i], (int)g_pwmDimMaxPct[i]);
    }
    /* 설정 변경은 드물므로 즉시 저장 */
    nv_flashWriteNew(1, NV_MODULE_APP, NV_ITEM_PWM_DIM_RANGE,
                     sizeof(s_dimSaved), (u8 *)&s_dimSaved);

    pwm_hw_apply(5);    /* 전 채널 재계산 — 새 구간 즉시 반영 */
    return TRUE;
}
