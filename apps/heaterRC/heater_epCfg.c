/*********************************************************************
 * @file    heater_epCfg.c
 * @brief   ZT3L 무시동히터 리모컨 — 엔드포인트 구성 / 속성 / NV
 *
 *  EP1 : Basic, Identify, Groups, OnOff(히터 전원 + 펄스 설정 + 레벨/마지막 입력), OTA client
 *  EP2 : Basic, Identify, Groups, OnOff(UP 버튼)
 *  EP3 : Basic, Identify, Groups, OnOff(DOWN 버튼)
 *********************************************************************/

#include "tl_common.h"
#include "zcl_include.h"
#include "heater_ep.h"

/*====================================================================
 * NV 저장 ID (NV_MODULE_APP)
 *==================================================================*/
#define NV_ITEM_HEATER_STATE    0x40    /* 히터 전원 상태 */
#define NV_ITEM_HEATER_CFG      0x41    /* 펄스 시간 설정 */
#define NV_ITEM_HEATER_LEVEL    0x42    /* 히터 레벨 0~9 */

/*====================================================================
 * Basic / Identify 속성
 *==================================================================*/
#ifndef ZCL_BASIC_MFG_NAME
#define ZCL_BASIC_MFG_NAME    {4, 'D','O','D','O'}
#endif
#ifndef ZCL_BASIC_MODEL_ID
#define ZCL_BASIC_MODEL_ID    {11, 'Z','T','3','L','-','H','E','A','T','E','R'}
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
    .powerSource  = POWER_SOURCE_MAINS_1_PHASE,  /* 상시 전원 라우터 (PWM5CH 와 동일 표기) */
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
 * Groups 클러스터
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
 * 히터 속성
 *==================================================================*/
bool g_heaterOnOff[HEATER_EP_COUNT] = { FALSE, FALSE, FALSE };

u8 g_heaterLevel      = HEATER_LEVEL_MIN;
u8 g_heaterLastAction = HEATER_LAST_NONE;
u8 g_heaterLastSource = HEATER_SRC_REMOTE;

heater_cfg_t g_heaterCfg = {
    .powerPulseMs = HEATER_POWER_PULSE_MS_DEF,
    .btnPulseMs   = HEATER_BTN_PULSE_MS_DEF,
    .gapMs        = HEATER_GAP_MS_DEF,
};

/* EP1 — 히터 전원 + 펄스 설정 */
static const zclAttrInfo_t power_oo_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                    ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_heaterOnOff[HEATER_IDX_POWER] },
    { ZCL_ATTRID_HEATER_POWER_PULSE_MS,    ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_heaterCfg.powerPulseMs },
    { ZCL_ATTRID_HEATER_BTN_PULSE_MS,      ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_heaterCfg.btnPulseMs },
    { ZCL_ATTRID_HEATER_GAP_MS,            ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE,      (u8*)&g_heaterCfg.gapMs },
    { ZCL_ATTRID_HEATER_LEVEL,             ZCL_DATA_TYPE_UINT8,   ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_heaterLevel },
    { ZCL_ATTRID_HEATER_LAST_ACTION,       ZCL_DATA_TYPE_ENUM8,   ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_heaterLastAction },
    { ZCL_ATTRID_HEATER_LAST_SOURCE,       ZCL_DATA_TYPE_ENUM8,   ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_heaterLastSource },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,  ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_POWER_OO_ATTR_NUM  (sizeof(power_oo_attrTbl) / sizeof(zclAttrInfo_t))

/* EP2 — UP 버튼 (순간 동작) */
static const zclAttrInfo_t up_oo_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                    ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_heaterOnOff[HEATER_IDX_UP] },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,  ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};

/* EP3 — DOWN 버튼 (순간 동작) */
static const zclAttrInfo_t down_oo_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                    ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_heaterOnOff[HEATER_IDX_DOWN] },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,  ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_BTN_OO_ATTR_NUM  (sizeof(up_oo_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * Simple Descriptor
 *   장치 ID 0x0002 = HA On/Off Output
 *==================================================================*/
#define HEATER_HA_DEV_ID    0x0002

static const u16 ep_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
#ifdef ZCL_GROUP
    ZCL_CLUSTER_GEN_GROUPS,
#endif
    ZCL_CLUSTER_GEN_ON_OFF,
};
#define EP_IN_CLUSTER_NUM   (sizeof(ep_inClusterList) / sizeof(u16))

static const u16 ep1_outClusterList[] = {
#ifdef ZCL_OTA
    ZCL_CLUSTER_OTA,
#endif
};
#define EP1_OUT_CLUSTER_NUM (sizeof(ep1_outClusterList) / sizeof(u16))

const af_simple_descriptor_t heater_ep1_simpleDesc = {
    HA_PROFILE_ID, HEATER_HA_DEV_ID, HEATER_EP_POWER, 1, 0,
    EP_IN_CLUSTER_NUM, EP1_OUT_CLUSTER_NUM,
    (u16*)ep_inClusterList, (u16*)ep1_outClusterList
};
const af_simple_descriptor_t heater_ep2_simpleDesc = {
    HA_PROFILE_ID, HEATER_HA_DEV_ID, HEATER_EP_UP, 1, 0,
    EP_IN_CLUSTER_NUM, 0,
    (u16*)ep_inClusterList, NULL
};
const af_simple_descriptor_t heater_ep3_simpleDesc = {
    HA_PROFILE_ID, HEATER_HA_DEV_ID, HEATER_EP_DOWN, 1, 0,
    EP_IN_CLUSTER_NUM, 0,
    (u16*)ep_inClusterList, NULL
};

/*====================================================================
 * zcl_specClusterInfo_t 테이블
 *==================================================================*/
static const zcl_specClusterInfo_t ep1_clusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,    MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)heater_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY, MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)heater_identifyCb },
    GROUP_CLUSTER_ENTRY
    { ZCL_CLUSTER_GEN_ON_OFF,   MANUFACTURER_CODE_NONE, ZCL_POWER_OO_ATTR_NUM, power_oo_attrTbl, zcl_onOff_register,    (void*)heater_onOffCb    },
};
static const zcl_specClusterInfo_t ep2_clusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,    MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)heater_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY, MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)heater_identifyCb },
    GROUP_CLUSTER_ENTRY
    { ZCL_CLUSTER_GEN_ON_OFF,   MANUFACTURER_CODE_NONE, ZCL_BTN_OO_ATTR_NUM,   up_oo_attrTbl,    zcl_onOff_register,    (void*)heater_onOffCb    },
};
static const zcl_specClusterInfo_t ep3_clusterList[] = {
    { ZCL_CLUSTER_GEN_BASIC,    MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)heater_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY, MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)heater_identifyCb },
    GROUP_CLUSTER_ENTRY
    { ZCL_CLUSTER_GEN_ON_OFF,   MANUFACTURER_CODE_NONE, ZCL_BTN_OO_ATTR_NUM,   down_oo_attrTbl,  zcl_onOff_register,    (void*)heater_onOffCb    },
};

u8 g_epClusterNum[HEATER_EP_COUNT] = {
    sizeof(ep1_clusterList) / sizeof(zcl_specClusterInfo_t),
    sizeof(ep2_clusterList) / sizeof(zcl_specClusterInfo_t),
    sizeof(ep3_clusterList) / sizeof(zcl_specClusterInfo_t),
};

const zcl_specClusterInfo_t * const g_epClusterList[HEATER_EP_COUNT] = {
    ep1_clusterList,
    ep2_clusterList,
    ep3_clusterList,
};

/*====================================================================
 * 설정값 보정
 *==================================================================*/
static u16 clamp_u16(u16 v, u16 mn, u16 mx)
{
    if (v < mn) return mn;
    if (v > mx) return mx;
    return v;
}

static void heater_cfg_sanitize(heater_cfg_t *c)
{
    c->powerPulseMs = clamp_u16(c->powerPulseMs, HEATER_PULSE_MS_MIN, HEATER_PULSE_MS_MAX);
    c->btnPulseMs   = clamp_u16(c->btnPulseMs,   HEATER_PULSE_MS_MIN, HEATER_PULSE_MS_MAX);
    c->gapMs        = clamp_u16(c->gapMs,        HEATER_GAP_MS_MIN,   HEATER_GAP_MS_MAX);
}

static heater_cfg_t s_cfgSaved;     /* 마지막으로 NV 에 저장된 설정 */

/*====================================================================
 * heater_attrs_init — NV 복원
 *   히터 전원 상태는 표시용으로만 복원하고, 부팅 시 버튼을 누르지 않음
 *   (실제 히터 상태와 다를 수 있으므로 자동 복원 펄스 금지)
 *==================================================================*/
void heater_attrs_init(void)
{
    u8 state = 0;
    heater_cfg_t cfg;

    if (nv_flashReadNew(1, NV_MODULE_APP, NV_ITEM_HEATER_STATE,
                        sizeof(state), &state) == NV_SUCC) {
        g_heaterOnOff[HEATER_IDX_POWER] = state ? TRUE : FALSE;
    }
    if (nv_flashReadNew(1, NV_MODULE_APP, NV_ITEM_HEATER_LEVEL,
                        sizeof(g_heaterLevel), &g_heaterLevel) != NV_SUCC ||
        g_heaterLevel > HEATER_LEVEL_MAX) {
        g_heaterLevel = HEATER_LEVEL_MIN;
    }
#if (HEATER_LEVEL_MIN > 0)
    if (g_heaterLevel < HEATER_LEVEL_MIN) g_heaterLevel = HEATER_LEVEL_MIN;
#endif

    g_heaterOnOff[HEATER_IDX_UP]   = FALSE;
    g_heaterOnOff[HEATER_IDX_DOWN] = FALSE;

    if (nv_flashReadNew(1, NV_MODULE_APP, NV_ITEM_HEATER_CFG,
                        sizeof(cfg), (u8 *)&cfg) == NV_SUCC) {
        g_heaterCfg = cfg;
    }
    heater_cfg_sanitize(&g_heaterCfg);
    s_cfgSaved = g_heaterCfg;

    DBG_LOG("[NV] heater power=%d level=%d, pulse power=%dms btn=%dms gap=%dms\r\n",
            (int)g_heaterOnOff[HEATER_IDX_POWER], (int)g_heaterLevel,
            (int)g_heaterCfg.powerPulseMs, (int)g_heaterCfg.btnPulseMs,
            (int)g_heaterCfg.gapMs);
}

static ev_timer_event_t *s_stateSaveTmr = NULL;

static s32 heater_state_save_cb(void *arg)
{
    u8 state = g_heaterOnOff[HEATER_IDX_POWER] ? 1 : 0;
    (void)arg;
    s_stateSaveTmr = NULL;
    nv_flashWriteNew(1, NV_MODULE_APP, NV_ITEM_HEATER_STATE, sizeof(state), &state);
    nv_flashWriteNew(1, NV_MODULE_APP, NV_ITEM_HEATER_LEVEL, sizeof(g_heaterLevel), &g_heaterLevel);
    return -1;
}

void heater_state_save(void)
{
    /* 재시작 방식 — 연속 변경 중에는 저장을 미루고 마지막 상태만 저장 */
    if (s_stateSaveTmr) {
        TL_ZB_TIMER_CANCEL(&s_stateSaveTmr);
        s_stateSaveTmr = NULL;
    }
    s_stateSaveTmr = TL_ZB_TIMER_SCHEDULE(heater_state_save_cb, NULL, HEATER_STATE_SAVE_DELAY_MS);
}

/*====================================================================
 * heater_cfg_check — 설정 속성(0xF000~0xF002) 변경 감지
 *   ZCL Write 처리 직후 + 메인 루프 500ms 주기로 호출
 *==================================================================*/
bool heater_cfg_check(void)
{
    if (memcmp(&g_heaterCfg, &s_cfgSaved, sizeof(heater_cfg_t)) == 0) return FALSE;

    heater_cfg_sanitize(&g_heaterCfg);
    s_cfgSaved = g_heaterCfg;
    nv_flashWriteNew(1, NV_MODULE_APP, NV_ITEM_HEATER_CFG,
                     sizeof(heater_cfg_t), (u8 *)&g_heaterCfg);

    DBG_LOG("[CFG] pulse power=%dms btn=%dms gap=%dms\r\n",
            (int)g_heaterCfg.powerPulseMs, (int)g_heaterCfg.btnPulseMs,
            (int)g_heaterCfg.gapMs);
    return TRUE;
}
