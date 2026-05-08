/*********************************************************************
 * @file    juntekEpCfg.c
 *
 * @brief   ZT3L JUNTEK BMS 모니터 — 엔드포인트 구성
 *
 *  EP1 : Electrical Measurement (Voltage/Current/Power)
 *  EP2 : Temperature Measurement
 *  EP3 : Binary Input (Relay State: 충전/방전)
 *  EP4 : Simple Metering (Remain Ah)
 *  EP5 : Analog Input (Elapsed Minutes)
 *********************************************************************/

#include "tl_common.h"
#include "zcl_include.h"
#include "juntekMonitor.h"
#include "juntek_ep.h"

extern u8 zcl_seqNum;

/*====================================================================
 * cluster_registerFunc_t 래퍼
 *
 * zcl_register()가 호출하는 시그니처:
 *   status_t fn(u8 endpoint, u16 manuCode, u8 attrNum,
 *               const zclAttrInfo_t attrTbl[], cluster_forAppCb_t cb)
 *
 * 실제 zcl_registerCluster() 시그니처:
 *   status_t zcl_registerCluster(u8 endpoint, u16 clusterId, u16 manuCode,
 *                                 u8 attrNum, const zclAttrInfo_t *pAttrTbl,
 *                                 cluster_cmdHdlr_t cmdHdlrFn,
 *                                 cluster_forAppCb_t cb)
 *
 * clusterId를 하드코딩한 래퍼로 각 클러스터를 등록
 *==================================================================*/
static status_t elec_register(u8 ep, u16 manuCode, u8 attrNum,
                               const zclAttrInfo_t *tbl, cluster_forAppCb_t cb)
{
    return zcl_registerCluster(ep, ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                                manuCode, attrNum, tbl, NULL, cb);
}

static status_t temp_register(u8 ep, u16 manuCode, u8 attrNum,
                               const zclAttrInfo_t *tbl, cluster_forAppCb_t cb)
{
    return zcl_registerCluster(ep, ZCL_CLUSTER_MS_TEMPERATURE_MEASUREMENT,
                                manuCode, attrNum, tbl, NULL, cb);
}

static status_t relay_register(u8 ep, u16 manuCode, u8 attrNum,
                                const zclAttrInfo_t *tbl, cluster_forAppCb_t cb)
{
    return zcl_registerCluster(ep, ZCL_CLUSTER_GEN_BINARY_INPUT_BASIC,
                                manuCode, attrNum, tbl, NULL, cb);
}

static status_t metering_register(u8 ep, u16 manuCode, u8 attrNum,
                                   const zclAttrInfo_t *tbl, cluster_forAppCb_t cb)
{
    return zcl_registerCluster(ep, ZCL_CLUSTER_SE_METERING,
                                manuCode, attrNum, tbl, NULL, cb);
}

static status_t analog_register(u8 ep, u16 manuCode, u8 attrNum,
                                 const zclAttrInfo_t *tbl, cluster_forAppCb_t cb)
{
    return zcl_registerCluster(ep, ZCL_CLUSTER_GEN_ANALOG_INPUT_BASIC,
                                manuCode, attrNum, tbl, NULL, cb);
}


#ifndef ZCL_BASIC_MFG_NAME
#define ZCL_BASIC_MFG_NAME    {4, 'D','O','D','O'}
#endif
#ifndef ZCL_BASIC_MODEL_ID
#define ZCL_BASIC_MODEL_ID    {12, 'J','U','N','T','E','K','-','Z','T','3','L','B'}
#endif
#ifndef ZCL_BASIC_SW_BUILD_ID
#define ZCL_BASIC_SW_BUILD_ID {7, '1','.','0','.','0','0','0'}
#endif

/*====================================================================
 * Basic 속성 — 공용 1개
 *==================================================================*/
zcl_basicAttr_t g_zcl_basicAttrs = {
    .zclVersion   = 0x03,
    .appVersion   = 0x00,
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

/*====================================================================
 * Identify cluster — 공용
 *==================================================================*/
zcl_identifyAttr_t g_zcl_identifyAttrs = { .identifyTime = 0 };

static const zclAttrInfo_t identify_attrTbl[] = {
    { ZCL_ATTRID_IDENTIFY_TIME,           ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_zcl_identifyAttrs.identifyTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                        (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_IDENTIFY_ATTR_NUM (sizeof(identify_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * EP1: Electrical Measurement 속성
 *==================================================================*/
juntek_elecAttr_t g_juntek_elecAttrs = {
    .measuredVoltage     = 0,
    .measuredCurrent     = 0,
    .activePower         = 0,
    .acVoltageMultiplier = 1,
    .acVoltageDivisor    = 100,   /* ÷100 → V */
    .acCurrentMultiplier = 1,
    .acCurrentDivisor    = 100,   /* ÷100 → A */
    .acPowerMultiplier   = 1,
    .acPowerDivisor      = 1,
};

/*====================================================================
 * 필터 설정 — NV 저장/로드 + ZCL 속성
 *==================================================================*/

/* 기본값 */
juntek_filter_cfg_t g_juntek_filterCfg = {
    .volt_min   = 9.0f,
    .volt_max   = 15.6f,
    .curr_min   = -210.0f,
    .curr_max   = 130.0f,
    .temp_min   = -20.0f,
    .temp_max   = 60.0f,
    .ah_max     = 265.0f,
    .volt_rate  = 0.5f,
    .curr_rate  = 80.0f,
    .temp_rate  = 2.0f,
};

void juntek_filter_cfg_save(void)
{
    nv_sts_t st = nv_flashWriteNew(1, NV_MODULE_APP,
                                    NV_ITEM_JUNTEK_FILTER_CFG,
                                    sizeof(juntek_filter_cfg_t),
                                    (u8*)&g_juntek_filterCfg);
    if (st == NV_SUCC) {
        //printf("filter cfg: saved\r\n");
    } else {
        //printf("filter cfg: save fail %d\r\n", (int)st);
    }
}

void juntek_filter_cfg_load(void)
{
    nv_sts_t st = nv_flashReadNew(1, NV_MODULE_APP,
                                   NV_ITEM_JUNTEK_FILTER_CFG,
                                   sizeof(juntek_filter_cfg_t),
                                   (u8*)&g_juntek_filterCfg);
    if (st != NV_SUCC) {
        juntek_filter_cfg_save();
        //printf("filter cfg: defaults saved\r\n");
    } else {
        //printf("filter cfg: loaded from NV\r\n");
    }
}

/* ZCL write 콜백 — Z2M에서 write 시 NV 저장 */
static status_t filter_cfg_writeCb(zclIncomingAddrInfo_t *pAddrInfo,
                                    u8 cmdId, void *cmdPayload)
{
    (void)pAddrInfo; (void)cmdId; (void)cmdPayload;
    juntek_filter_cfg_save();
    //printf("filter cfg: saved\r\n");
    return ZCL_STA_SUCCESS;
}

/* 필터 설정 속성 테이블 (EP1 추가 속성) */
static const zclAttrInfo_t filter_attrTbl[] = {
    { ZCL_ATTRID_FILTER_VOLT_MIN,   ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_juntek_filterCfg.volt_min  },
    { ZCL_ATTRID_FILTER_VOLT_MAX,   ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_juntek_filterCfg.volt_max  },
    { ZCL_ATTRID_FILTER_CURR_MIN,   ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_juntek_filterCfg.curr_min  },
    { ZCL_ATTRID_FILTER_CURR_MAX,   ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_juntek_filterCfg.curr_max  },
    { ZCL_ATTRID_FILTER_TEMP_MIN,   ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_juntek_filterCfg.temp_min  },
    { ZCL_ATTRID_FILTER_TEMP_MAX,   ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_juntek_filterCfg.temp_max  },
    { ZCL_ATTRID_FILTER_AH_MAX,     ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_juntek_filterCfg.ah_max    },
    { ZCL_ATTRID_FILTER_VOLT_RATE,  ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_juntek_filterCfg.volt_rate },
    { ZCL_ATTRID_FILTER_CURR_RATE,  ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_juntek_filterCfg.curr_rate },
    { ZCL_ATTRID_FILTER_TEMP_RATE,  ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_juntek_filterCfg.temp_rate },
};
#define ZCL_FILTER_ATTR_NUM_DEF  (sizeof(filter_attrTbl) / sizeof(zclAttrInfo_t))

/* 필터 클러스터 등록 래퍼 — EP1에 별도 클러스터(0xFF00)로 등록 */
#define ZCL_CLUSTER_JUNTEK_FILTER   0xFF00   /* manufacturer-specific cluster */

static status_t filter_register(u8 ep, u16 manuCode, u8 attrNum,
                                 const zclAttrInfo_t *tbl, cluster_forAppCb_t cb)
{
    return zcl_registerCluster(ep, ZCL_CLUSTER_JUNTEK_FILTER,
                                manuCode, attrNum, tbl, NULL, cb);
}

static const zclAttrInfo_t elec_attrTbl[] = {
    { ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE,         ZCL_DATA_TYPE_INT16,    ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_elecAttrs.measuredVoltage },
    { ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,         ZCL_DATA_TYPE_INT16,    ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_elecAttrs.measuredCurrent },
    { ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER,        ZCL_DATA_TYPE_INT16,    ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_elecAttrs.activePower },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_MULTIPLIER,  ZCL_DATA_TYPE_UINT16,   ACCESS_CONTROL_READ,                             (u8*)&g_juntek_elecAttrs.acVoltageMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_DIVISOR,     ZCL_DATA_TYPE_UINT16,   ACCESS_CONTROL_READ,                             (u8*)&g_juntek_elecAttrs.acVoltageDivisor },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_MULTIPLIER, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_juntek_elecAttrs.acCurrentMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_DIVISOR,  ZCL_DATA_TYPE_UINT16,   ACCESS_CONTROL_READ,                             (u8*)&g_juntek_elecAttrs.acCurrentDivisor },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_MULTIPLIER, ZCL_DATA_TYPE_UINT16,   ACCESS_CONTROL_READ,                             (u8*)&g_juntek_elecAttrs.acPowerMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_DIVISOR,    ZCL_DATA_TYPE_UINT16,   ACCESS_CONTROL_READ,                             (u8*)&g_juntek_elecAttrs.acPowerDivisor },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,             ZCL_DATA_TYPE_UINT16,   ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_ELEC_ATTR_NUM  (sizeof(elec_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * EP2: Temperature Measurement 속성
 *==================================================================*/
juntek_tempAttr_t g_juntek_tempAttrs = {
    .measuredValue    = 0,
    .minMeasuredValue = -10000,   /* -100.00°C */
    .maxMeasuredValue =  10000,   /* +100.00°C */
};

static const zclAttrInfo_t temp_attrTbl[] = {
    { ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MEAS_VAL,     ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_tempAttrs.measuredValue },
    { ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MIN_MEAS_VAL, ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ,                             (u8*)&g_juntek_tempAttrs.minMeasuredValue },
    { ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MAX_MEAS_VAL, ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ,                             (u8*)&g_juntek_tempAttrs.maxMeasuredValue },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,              ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_TEMP_ATTR_NUM  (sizeof(temp_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * EP3: Binary Input 속성 (Relay State)
 *==================================================================*/
juntek_relayAttr_t g_juntek_relayAttrs = {
    .presentValue = FALSE,
    .statusFlags  = 0,
};

static const zclAttrInfo_t relay_attrTbl[] = {
    { ZCL_ATTRID_BINARY_INPUT_PRESENT_VALUE, ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_relayAttrs.presentValue },
    { ZCL_ATTRID_BINARY_INPUT_STATUS_FLAGS,  ZCL_DATA_TYPE_BITMAP8, ACCESS_CONTROL_READ,                             (u8*)&g_juntek_relayAttrs.statusFlags },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,    ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_RELAY_ATTR_NUM  (sizeof(relay_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * EP4: Simple Metering 속성 (Remain Ah)
 *
 * ZCL Simple Metering 클러스터(0x0702)
 * currentSummation : UINT48, mAh 단위 (÷1000 → Ah)
 * unitOfMeasure    : 0x06 = Ah (암페어시)
 * divisor          : 1000
 *==================================================================*/
juntek_meteringAttr_t g_juntek_meteringAttrs = {
    .currentSummation  = {0, 0, 0, 0, 0, 0},
    .unitOfMeasure     = 0x06,   /* 0x06 = A²h / Ah */
    .multiplier        = 1,
    .divisor           = 1000,   /* mAh ÷ 1000 = Ah */
    .summationFormatting = 0x92, /* 2자리 소수점 */
    .meteringDeviceType  = 0x00, /* Electric Metering */
};

static const zclAttrInfo_t metering_attrTbl[] = {
    { ZCL_ATTRID_METERING_CURRENT_SUMMATION_DELIVERD, ZCL_DATA_TYPE_UINT48, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)g_juntek_meteringAttrs.currentSummation },
    { ZCL_ATTRID_METERING_UNIT_OF_MEASURE,            ZCL_DATA_TYPE_ENUM8,  ACCESS_CONTROL_READ,                             (u8*)&g_juntek_meteringAttrs.unitOfMeasure },
    { ZCL_ATTRID_METERING_MULTIPLIER,                 ZCL_DATA_TYPE_UINT24, ACCESS_CONTROL_READ,                             (u8*)&g_juntek_meteringAttrs.multiplier },
    { ZCL_ATTRID_METERING_DIVISOR,                    ZCL_DATA_TYPE_UINT24, ACCESS_CONTROL_READ,                             (u8*)&g_juntek_meteringAttrs.divisor },
    { ZCL_ATTRID_METERING_SUMMATION_FORMATTING,       ZCL_DATA_TYPE_BITMAP8,ACCESS_CONTROL_READ,                             (u8*)&g_juntek_meteringAttrs.summationFormatting },
    { ZCL_ATTRID_METERING_METERING_DEVICE_TYPE,       ZCL_DATA_TYPE_BITMAP8,ACCESS_CONTROL_READ,                             (u8*)&g_juntek_meteringAttrs.meteringDeviceType },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,             ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_METERING_ATTR_NUM  (sizeof(metering_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * EP5: Analog Input 속성 (Elapsed Minutes)
 *
 * ZCL Analog Input 클러스터(0x000C)
 * presentValue : SINGLE (IEEE 754 float), 분 단위
 *==================================================================*/
juntek_analogAttr_t g_juntek_analogAttrs = {
    .presentValue = 0.0f,
    .statusFlags  = 0,
};

static const zclAttrInfo_t analog_attrTbl[] = {
    { ZCL_ATTRID_ANALOG_INPUT_PRESENT_VALUE, ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_analogAttrs.presentValue },
    { ZCL_ATTRID_ANALOG_INPUT_STATUS_FLAGS,  ZCL_DATA_TYPE_BITMAP8,     ACCESS_CONTROL_READ,                             (u8*)&g_juntek_analogAttrs.statusFlags },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,    ZCL_DATA_TYPE_UINT16,      ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_ANALOG_ATTR_NUM  (sizeof(analog_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * Simple Descriptor
 *==================================================================*/

/* EP1 입력 클러스터: Basic + Identify + Electrical Measurement */
static const u16 ep1_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,   /* 0x0B04 — 표준 Electrical Measurement */
};
static const u16 ep1_outClusterList[] = {
#ifdef ZCL_OTA
    ZCL_CLUSTER_OTA,
#endif
};

const af_simple_descriptor_t juntek_ep1_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_SIMPLE_SENSOR,
    JUNTEK_ENDPOINT_ELEC,
    1, 0,
    sizeof(ep1_inClusterList)  / sizeof(u16),
    sizeof(ep1_outClusterList) / sizeof(u16),
    (u16*)ep1_inClusterList,
    (u16*)ep1_outClusterList,
};

/* EP2 입력 클러스터: Basic + Identify + Temperature */
static const u16 ep2_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_MS_TEMPERATURE_MEASUREMENT,
};
static const u16 ep2_outClusterList[] = { 0 };

const af_simple_descriptor_t juntek_ep2_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_TEMPERATURE_SENSOR,
    JUNTEK_ENDPOINT_TEMP,
    1, 0,
    sizeof(ep2_inClusterList) / sizeof(u16),
    0,
    (u16*)ep2_inClusterList,
    NULL,
};

/* EP3 입력 클러스터: Basic + Identify + Binary Input */
static const u16 ep3_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_GEN_BINARY_INPUT_BASIC,
};
static const u16 ep3_outClusterList[] = { 0 };

const af_simple_descriptor_t juntek_ep3_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_SIMPLE_SENSOR,
    JUNTEK_ENDPOINT_RELAY,
    1, 0,
    sizeof(ep3_inClusterList) / sizeof(u16),
    0,
    (u16*)ep3_inClusterList,
    NULL,
};

/* EP4 입력 클러스터: Basic + Identify + Simple Metering */
static const u16 ep4_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_SE_METERING,
};

const af_simple_descriptor_t juntek_ep4_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_SIMPLE_SENSOR,
    JUNTEK_ENDPOINT_METERING,
    1, 0,
    sizeof(ep4_inClusterList) / sizeof(u16),
    0,
    (u16*)ep4_inClusterList,
    NULL,
};

/* EP5 입력 클러스터: Basic + Identify + Analog Input */
static const u16 ep5_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_GEN_ANALOG_INPUT_BASIC,
};

const af_simple_descriptor_t juntek_ep5_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_SIMPLE_SENSOR,
    JUNTEK_ENDPOINT_ANALOG,
    1, 0,
    sizeof(ep5_inClusterList) / sizeof(u16),
    0,
    (u16*)ep5_inClusterList,
    NULL,
};

/* EP1: Electrical Measurement */
static const zcl_specClusterInfo_t g_clusterList_ep1[] = {
    { ZCL_CLUSTER_GEN_BASIC,                   MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,      basic_attrTbl,    zcl_basic_register,    (void*)juntek_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,                MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM,   identify_attrTbl, zcl_identify_register, (void*)juntek_identifyCb },
    { ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,   MANUFACTURER_CODE_NONE, ZCL_ELEC_ATTR_NUM,       elec_attrTbl,     elec_register,         NULL                     },
    { ZCL_CLUSTER_JUNTEK_FILTER,               MANUFACTURER_CODE_NONE, ZCL_FILTER_ATTR_NUM_DEF, filter_attrTbl,   filter_register,       (void*)filter_cfg_writeCb},
};

/* EP2: Temperature */
static const zcl_specClusterInfo_t g_clusterList_ep2[] = {
    { ZCL_CLUSTER_GEN_BASIC,                   MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)juntek_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,                MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)juntek_identifyCb },
    { ZCL_CLUSTER_MS_TEMPERATURE_MEASUREMENT,  MANUFACTURER_CODE_NONE, ZCL_TEMP_ATTR_NUM,     temp_attrTbl,     temp_register,         NULL                     },
};

/* EP3: Binary Input (Relay) */
static const zcl_specClusterInfo_t g_clusterList_ep3[] = {
    { ZCL_CLUSTER_GEN_BASIC,                   MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)juntek_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,                MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)juntek_identifyCb },
    { ZCL_CLUSTER_GEN_BINARY_INPUT_BASIC,      MANUFACTURER_CODE_NONE, ZCL_RELAY_ATTR_NUM,    relay_attrTbl,    relay_register,        NULL                     },
};

/* EP4: Simple Metering (Remain Ah) */
static const zcl_specClusterInfo_t g_clusterList_ep4[] = {
    { ZCL_CLUSTER_GEN_BASIC,                   MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)juntek_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,                MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)juntek_identifyCb },
    { ZCL_CLUSTER_SE_METERING,                 MANUFACTURER_CODE_NONE, ZCL_METERING_ATTR_NUM, metering_attrTbl, metering_register,     NULL                     },
};

/* EP5: Analog Input (Elapsed Minutes) */
static const zcl_specClusterInfo_t g_clusterList_ep5[] = {
    { ZCL_CLUSTER_GEN_BASIC,                   MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)juntek_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,                MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)juntek_identifyCb },
    { ZCL_CLUSTER_GEN_ANALOG_INPUT_BASIC,      MANUFACTURER_CODE_NONE, ZCL_ANALOG_ATTR_NUM,   analog_attrTbl,   analog_register,       NULL                     },
};

/* EP별 클러스터 수 — zcl_register()에 EP별로 전달 */
u8 g_epClusterNum[JUNTEK_EP_COUNT] = {
    sizeof(g_clusterList_ep1) / sizeof(zcl_specClusterInfo_t),  /* EP1 */
    sizeof(g_clusterList_ep2) / sizeof(zcl_specClusterInfo_t),  /* EP2 */
    sizeof(g_clusterList_ep3) / sizeof(zcl_specClusterInfo_t),  /* EP3 */
    sizeof(g_clusterList_ep4) / sizeof(zcl_specClusterInfo_t),  /* EP4 */
    sizeof(g_clusterList_ep5) / sizeof(zcl_specClusterInfo_t),  /* EP5 */
};

const zcl_specClusterInfo_t * const g_epClusterList[JUNTEK_EP_COUNT] = {
    g_clusterList_ep1,
    g_clusterList_ep2,
    g_clusterList_ep3,
    g_clusterList_ep4,
    g_clusterList_ep5,
};

/*====================================================================
 * BMS 속성 초기화
 *==================================================================*/
void juntek_attrs_init(void)
{
    u8 i;

    g_juntek_elecAttrs.measuredVoltage  = 0;
    g_juntek_elecAttrs.measuredCurrent  = 0;
    g_juntek_elecAttrs.activePower      = 0;
    g_juntek_tempAttrs.measuredValue    = 0;
    g_juntek_relayAttrs.presentValue    = FALSE;

    for (i = 0; i < 6; i++) g_juntek_meteringAttrs.currentSummation[i] = 0;
    g_juntek_analogAttrs.presentValue = 0.0f;

    /* 필터 설정 NV 로드 */
    juntek_filter_cfg_load();
}

/*====================================================================
 * JUNTEK 속성 업데이트 — 파싱된 데이터를 ZCL 속성에 반영
 *==================================================================*/
void juntek_attrs_update(const juntek_data_t *d)
{
    u32 mAh;

    if (!d || !d->valid) return;

    /* EP1: Electrical — 단위 변환 (÷divisor로 실수 표현) */
    g_juntek_elecAttrs.measuredVoltage  = (s16)(d->voltage  * 100.0f);  /* 0.01V 단위 */
    g_juntek_elecAttrs.measuredCurrent  = (s16)(d->current  * 100.0f);  /* 0.01A 단위 */
    g_juntek_elecAttrs.activePower      = (s16)(d->power);               /* 1W 단위 */

    /* EP2: Temperature — 0.01°C 단위 */
    g_juntek_tempAttrs.measuredValue    = (s16)(d->temperature * 100.0f);

    /* EP3: Relay State — 1=충전(true), 0=방전(false) */
    g_juntek_relayAttrs.presentValue    = (d->relay == 1) ? TRUE : FALSE;

    /* EP4: Metering — remain_ah → mAh (UINT48, LSB first) */
    mAh = (u32)(d->remain_ah * 1000.0f);
    g_juntek_meteringAttrs.currentSummation[0] = (u8)(mAh & 0xFF);
    g_juntek_meteringAttrs.currentSummation[1] = (u8)((mAh >>  8) & 0xFF);
    g_juntek_meteringAttrs.currentSummation[2] = (u8)((mAh >> 16) & 0xFF);
    g_juntek_meteringAttrs.currentSummation[3] = (u8)((mAh >> 24) & 0xFF);
    g_juntek_meteringAttrs.currentSummation[4] = 0;
    g_juntek_meteringAttrs.currentSummation[5] = 0;

    /* EP5: Analog Input — elapsed_min (float, 분 단위) */
    g_juntek_analogAttrs.presentValue = (float)d->elapsed_min;

    //printf("BMS update: V=%d.%02d A=%c%d.%02d W=%c%d.%01d T=%d.%01d Ah=%d.%03d min=%d relay=%s\r\n",
    //       (int)d->voltage, (int)(d->voltage * 100) % 100,
    //       d->current < 0 ? '-' : '+',
    //       (int)(d->current < 0 ? -d->current : d->current),
    //       (int)((d->current < 0 ? -d->current : d->current) * 100) % 100,
    //       d->power < 0 ? '-' : '+',
    //       (int)(d->power < 0 ? -d->power : d->power),
    //       (int)((d->power < 0 ? -d->power : d->power) * 10) % 10,
    //       (int)d->temperature,
    //       (int)(d->temperature < 0 ? -d->temperature : d->temperature) % 10,
    //       (int)d->remain_ah, (int)(d->remain_ah * 1000) % 1000,
    //       (int)d->elapsed_min,
    //       d->relay ? "CHG" : "DCH");

    /* ZCL 리포팅 — 5초마다 코디네이터로 직접 전송 */
    {
        static u32 s_last_report_tick = 0;
        u32 now = clock_time();

        if (!s_last_report_tick ||
            (u32)(now - s_last_report_tick) >= 5 * 1000 * CLOCK_16M_SYS_TIMER_CLK_1MS) {

            s_last_report_tick = now;

            epInfo_t dstEp;
            TL_SETSTRUCTCONTENT(dstEp, 0);
            dstEp.dstAddrMode = APS_SHORT_DSTADDR_WITHEP;
            dstEp.dstAddr.shortAddr = 0x0000;  /* 코디네이터 */
            dstEp.dstEp = 1;
            dstEp.profileId = HA_PROFILE_ID;

            /* EP1: Voltage */
            zcl_report(JUNTEK_ENDPOINT_ELEC, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_juntek_elecAttrs.measuredVoltage);

            /* EP1: Current */
            zcl_report(JUNTEK_ENDPOINT_ELEC, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_juntek_elecAttrs.measuredCurrent);

            /* EP1: Power */
            zcl_report(JUNTEK_ENDPOINT_ELEC, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_juntek_elecAttrs.activePower);

            /* EP2: Temperature */
            zcl_report(JUNTEK_ENDPOINT_TEMP, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_TEMPERATURE_MEASUREMENT,
                       ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MEAS_VAL,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_juntek_tempAttrs.measuredValue);

            /* EP3: Relay */
            {
                u8 relay = g_juntek_relayAttrs.presentValue ? 1 : 0;
                zcl_report(JUNTEK_ENDPOINT_RELAY, &dstEp, TRUE,
                           ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                           MANUFACTURER_CODE_NONE,
                           ZCL_CLUSTER_GEN_BINARY_INPUT_BASIC,
                           ZCL_ATTRID_BINARY_INPUT_PRESENT_VALUE,
                           ZCL_DATA_TYPE_BOOLEAN,
                           &relay);
            }

            /* EP4: Metering */
            zcl_report(JUNTEK_ENDPOINT_METERING, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_SE_METERING,
                       ZCL_ATTRID_METERING_CURRENT_SUMMATION_DELIVERD,
                       ZCL_DATA_TYPE_UINT48,
                       g_juntek_meteringAttrs.currentSummation);

            /* EP5: Analog */
            zcl_report(JUNTEK_ENDPOINT_ANALOG, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_GEN_ANALOG_INPUT_BASIC,
                       ZCL_ATTRID_ANALOG_INPUT_PRESENT_VALUE,
                       ZCL_DATA_TYPE_SINGLE_PREC,
                       (u8*)&g_juntek_analogAttrs.presentValue);
        }
    }
}
