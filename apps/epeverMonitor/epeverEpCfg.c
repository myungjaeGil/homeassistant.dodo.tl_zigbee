/*********************************************************************
 * @file    epeverEpCfg.c
 *
 * @brief   ZT3L EPever Tracer AN MPPT 모니터 — 엔드포인트 구성
 *
 *  EP1 : Electrical Measurement (Solar V/I/W)
 *  EP2 : Electrical Measurement (Battery V/I/W)
 *  EP3 : Electrical Measurement (Load V/I/W)
 *  EP4 : Temperature Measurement (Battery Temp)
 *  EP5 : Analog Input (Battery SOC %)
 *********************************************************************/

#include "tl_common.h"
#include "zcl_include.h"
#include "epeverMonitor.h"
#include "epever_ep.h"

extern u8 zcl_seqNum;

/*====================================================================
 * cluster_registerFunc_t 래퍼
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

static status_t analog_register(u8 ep, u16 manuCode, u8 attrNum,
                                 const zclAttrInfo_t *tbl, cluster_forAppCb_t cb)
{
    return zcl_registerCluster(ep, ZCL_CLUSTER_GEN_ANALOG_INPUT_BASIC,
                                manuCode, attrNum, tbl, NULL, cb);
}

/*====================================================================
 * Basic / Identify 속성
 *==================================================================*/
#ifndef ZCL_BASIC_MFG_NAME
#define ZCL_BASIC_MFG_NAME    {4, 'D','O','D','O'}
#endif
#ifndef ZCL_BASIC_MODEL_ID
#define ZCL_BASIC_MODEL_ID    {13, 'E','P','E','V','E','R','-','Z','T','3','L','M','P'}
#endif
#ifndef ZCL_BASIC_SW_BUILD_ID
#define ZCL_BASIC_SW_BUILD_ID {7, '1','.','0','.','0','0','0'}
#endif

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

zcl_identifyAttr_t g_zcl_identifyAttrs = { .identifyTime = 0 };

static const zclAttrInfo_t identify_attrTbl[] = {
    { ZCL_ATTRID_IDENTIFY_TIME,           ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_zcl_identifyAttrs.identifyTime },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                        (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_IDENTIFY_ATTR_NUM (sizeof(identify_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * EP1: Solar Electrical Measurement 속성
 *==================================================================*/
epever_solarAttr_t g_epever_solarAttrs = {
    .measuredVoltage     = 0,
    .measuredCurrent     = 0,
    .activePower         = 0,
    .acVoltageMultiplier = 1,
    .acVoltageDivisor    = 100,
    .acCurrentMultiplier = 1,
    .acCurrentDivisor    = 100,
    .acPowerMultiplier   = 1,
    .acPowerDivisor      = 100,
};

static const zclAttrInfo_t solar_attrTbl[] = {
    { ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE,           ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_solarAttrs.measuredVoltage },
    { ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,           ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_solarAttrs.measuredCurrent },
    { ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER,          ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_solarAttrs.activePower },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_MULTIPLIER,    ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_solarAttrs.acVoltageMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_DIVISOR,       ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_solarAttrs.acVoltageDivisor },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_MULTIPLIER, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_solarAttrs.acCurrentMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_DIVISOR,    ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_solarAttrs.acCurrentDivisor },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_MULTIPLIER,   ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_solarAttrs.acPowerMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_DIVISOR,      ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_solarAttrs.acPowerDivisor },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,               ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_SOLAR_ATTR_NUM  (sizeof(solar_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * EP2: Battery Electrical Measurement 속성
 *==================================================================*/
epever_batteryAttr_t g_epever_batteryAttrs = {
    .measuredVoltage     = 0,
    .measuredCurrent     = 0,
    .activePower         = 0,
    .acVoltageMultiplier = 1,
    .acVoltageDivisor    = 100,
    .acCurrentMultiplier = 1,
    .acCurrentDivisor    = 100,
    .acPowerMultiplier   = 1,
    .acPowerDivisor      = 100,
};

static const zclAttrInfo_t battery_attrTbl[] = {
    { ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE,           ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_batteryAttrs.measuredVoltage },
    { ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,           ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_batteryAttrs.measuredCurrent },
    { ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER,          ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_batteryAttrs.activePower },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_MULTIPLIER,    ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_batteryAttrs.acVoltageMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_DIVISOR,       ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_batteryAttrs.acVoltageDivisor },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_MULTIPLIER, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_batteryAttrs.acCurrentMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_DIVISOR,    ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_batteryAttrs.acCurrentDivisor },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_MULTIPLIER,   ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_batteryAttrs.acPowerMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_DIVISOR,      ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_batteryAttrs.acPowerDivisor },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,               ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_BATTERY_ATTR_NUM  (sizeof(battery_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * EP3: Load Electrical Measurement 속성
 *==================================================================*/
epever_loadAttr_t g_epever_loadAttrs = {
    .measuredVoltage     = 0,
    .measuredCurrent     = 0,
    .activePower         = 0,
    .acVoltageMultiplier = 1,
    .acVoltageDivisor    = 100,
    .acCurrentMultiplier = 1,
    .acCurrentDivisor    = 100,
    .acPowerMultiplier   = 1,
    .acPowerDivisor      = 100,
};

static const zclAttrInfo_t load_attrTbl[] = {
    { ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE,           ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_loadAttrs.measuredVoltage },
    { ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,           ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_loadAttrs.measuredCurrent },
    { ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER,          ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_loadAttrs.activePower },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_MULTIPLIER,    ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_loadAttrs.acVoltageMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_DIVISOR,       ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_loadAttrs.acVoltageDivisor },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_MULTIPLIER, ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_loadAttrs.acCurrentMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_DIVISOR,    ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_loadAttrs.acCurrentDivisor },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_MULTIPLIER,   ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_loadAttrs.acPowerMultiplier },
    { ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_DIVISOR,      ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&g_epever_loadAttrs.acPowerDivisor },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,               ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_LOAD_ATTR_NUM  (sizeof(load_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * EP4: Temperature Measurement 속성 (배터리 온도)
 *==================================================================*/
epever_tempAttr_t g_epever_tempAttrs = {
    .measuredValue    = 0,
    .minMeasuredValue = -10000,
    .maxMeasuredValue =  10000,
};

static const zclAttrInfo_t temp_attrTbl[] = {
    { ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MEAS_VAL,     ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_tempAttrs.measuredValue },
    { ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MIN_MEAS_VAL, ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ,                             (u8*)&g_epever_tempAttrs.minMeasuredValue },
    { ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MAX_MEAS_VAL, ZCL_DATA_TYPE_INT16,  ACCESS_CONTROL_READ,                             (u8*)&g_epever_tempAttrs.maxMeasuredValue },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,              ZCL_DATA_TYPE_UINT16, ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_TEMP_ATTR_NUM  (sizeof(temp_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * EP5: Analog Input 속성 (Battery SOC %)
 *==================================================================*/
epever_socAttr_t g_epever_socAttrs = {
    .presentValue = 0.0f,
    .statusFlags  = 0,
};

static const zclAttrInfo_t soc_attrTbl[] = {
    { ZCL_ATTRID_ANALOG_INPUT_PRESENT_VALUE, ZCL_DATA_TYPE_SINGLE_PREC, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_epever_socAttrs.presentValue },
    { ZCL_ATTRID_ANALOG_INPUT_STATUS_FLAGS,  ZCL_DATA_TYPE_BITMAP8,     ACCESS_CONTROL_READ,                             (u8*)&g_epever_socAttrs.statusFlags },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION,    ZCL_DATA_TYPE_UINT16,      ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_SOC_ATTR_NUM  (sizeof(soc_attrTbl) / sizeof(zclAttrInfo_t))

/*====================================================================
 * Simple Descriptor
 *==================================================================*/

/* EP1: Solar */
static const u16 ep1_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
};
static const u16 ep1_outClusterList[] = {
#ifdef ZCL_OTA
    ZCL_CLUSTER_OTA,
#endif
};

const af_simple_descriptor_t epever_ep1_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_SIMPLE_SENSOR,
    EPEVER_ENDPOINT_SOLAR,
    1, 0,
    sizeof(ep1_inClusterList)  / sizeof(u16),
    sizeof(ep1_outClusterList) / sizeof(u16),
    (u16*)ep1_inClusterList,
    (u16*)ep1_outClusterList,
};

/* EP2: Battery */
static const u16 ep2_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
};

const af_simple_descriptor_t epever_ep2_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_SIMPLE_SENSOR,
    EPEVER_ENDPOINT_BATTERY,
    1, 0,
    sizeof(ep2_inClusterList) / sizeof(u16),
    0,
    (u16*)ep2_inClusterList,
    NULL,
};

/* EP3: Load */
static const u16 ep3_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
};

const af_simple_descriptor_t epever_ep3_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_SIMPLE_SENSOR,
    EPEVER_ENDPOINT_LOAD,
    1, 0,
    sizeof(ep3_inClusterList) / sizeof(u16),
    0,
    (u16*)ep3_inClusterList,
    NULL,
};

/* EP4: Temperature */
static const u16 ep4_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_MS_TEMPERATURE_MEASUREMENT,
};

const af_simple_descriptor_t epever_ep4_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_TEMPERATURE_SENSOR,
    EPEVER_ENDPOINT_TEMP,
    1, 0,
    sizeof(ep4_inClusterList) / sizeof(u16),
    0,
    (u16*)ep4_inClusterList,
    NULL,
};

/* EP5: SOC */
static const u16 ep5_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_GEN_ANALOG_INPUT_BASIC,
};

const af_simple_descriptor_t epever_ep5_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_SIMPLE_SENSOR,
    EPEVER_ENDPOINT_SOC,
    1, 0,
    sizeof(ep5_inClusterList) / sizeof(u16),
    0,
    (u16*)ep5_inClusterList,
    NULL,
};

/*====================================================================
 * zcl_specClusterInfo_t 테이블
 *==================================================================*/

/* EP1: Solar Electrical Measurement */
static const zcl_specClusterInfo_t g_clusterList_ep1[] = {
    { ZCL_CLUSTER_GEN_BASIC,                 MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)epever_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,              MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)epever_identifyCb },
    { ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT, MANUFACTURER_CODE_NONE, ZCL_SOLAR_ATTR_NUM,    solar_attrTbl,    elec_register,         NULL                     },
};

/* EP2: Battery Electrical Measurement */
static const zcl_specClusterInfo_t g_clusterList_ep2[] = {
    { ZCL_CLUSTER_GEN_BASIC,                 MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)epever_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,              MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)epever_identifyCb },
    { ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT, MANUFACTURER_CODE_NONE, ZCL_BATTERY_ATTR_NUM,  battery_attrTbl,  elec_register,         NULL                     },
};

/* EP3: Load Electrical Measurement */
static const zcl_specClusterInfo_t g_clusterList_ep3[] = {
    { ZCL_CLUSTER_GEN_BASIC,                 MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)epever_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,              MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)epever_identifyCb },
    { ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT, MANUFACTURER_CODE_NONE, ZCL_LOAD_ATTR_NUM,     load_attrTbl,     elec_register,         NULL                     },
};

/* EP4: Temperature */
static const zcl_specClusterInfo_t g_clusterList_ep4[] = {
    { ZCL_CLUSTER_GEN_BASIC,                  MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)epever_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,               MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)epever_identifyCb },
    { ZCL_CLUSTER_MS_TEMPERATURE_MEASUREMENT, MANUFACTURER_CODE_NONE, ZCL_TEMP_ATTR_NUM,     temp_attrTbl,     temp_register,         NULL                     },
};

/* EP5: SOC Analog Input */
static const zcl_specClusterInfo_t g_clusterList_ep5[] = {
    { ZCL_CLUSTER_GEN_BASIC,              MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)epever_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,           MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)epever_identifyCb },
    { ZCL_CLUSTER_GEN_ANALOG_INPUT_BASIC, MANUFACTURER_CODE_NONE, ZCL_SOC_ATTR_NUM,      soc_attrTbl,      analog_register,       NULL                     },
};

u8 g_epClusterNum[EPEVER_EP_COUNT] = {
    sizeof(g_clusterList_ep1) / sizeof(zcl_specClusterInfo_t),
    sizeof(g_clusterList_ep2) / sizeof(zcl_specClusterInfo_t),
    sizeof(g_clusterList_ep3) / sizeof(zcl_specClusterInfo_t),
    sizeof(g_clusterList_ep4) / sizeof(zcl_specClusterInfo_t),
    sizeof(g_clusterList_ep5) / sizeof(zcl_specClusterInfo_t),
};

const zcl_specClusterInfo_t * const g_epClusterList[EPEVER_EP_COUNT] = {
    g_clusterList_ep1,
    g_clusterList_ep2,
    g_clusterList_ep3,
    g_clusterList_ep4,
    g_clusterList_ep5,
};

/*====================================================================
 * 속성 초기화
 *==================================================================*/
void epever_attrs_init(void)
{
    g_epever_solarAttrs.measuredVoltage   = 0;
    g_epever_solarAttrs.measuredCurrent   = 0;
    g_epever_solarAttrs.activePower       = 0;

    g_epever_batteryAttrs.measuredVoltage = 0;
    g_epever_batteryAttrs.measuredCurrent = 0;
    g_epever_batteryAttrs.activePower     = 0;

    g_epever_loadAttrs.measuredVoltage    = 0;
    g_epever_loadAttrs.measuredCurrent    = 0;
    g_epever_loadAttrs.activePower        = 0;

    g_epever_tempAttrs.measuredValue      = 0;
    g_epever_socAttrs.presentValue        = 0.0f;
}

/*====================================================================
 * EPever 속성 업데이트 — 파싱된 데이터를 ZCL 속성에 반영 후 리포팅
 *==================================================================*/
void epever_attrs_update(const epever_data_t *d)
{
    if (!d || !d->valid) return;

    /* EP1: Solar */
    g_epever_solarAttrs.measuredVoltage = (s16)(d->solar_volt * 100.0f);
    g_epever_solarAttrs.measuredCurrent = (s16)(d->solar_curr * 100.0f);
    g_epever_solarAttrs.activePower     = (s16)(d->solar_pow  * 100.0f);

    /* EP2: Battery */
    g_epever_batteryAttrs.measuredVoltage = (s16)(d->bat_volt * 100.0f);
    g_epever_batteryAttrs.measuredCurrent = (s16)(d->bat_curr * 100.0f);
    g_epever_batteryAttrs.activePower     = (s16)(d->chg_pow  * 100.0f);

    /* EP3: Load */
    g_epever_loadAttrs.measuredVoltage = (s16)(d->load_volt * 100.0f);
    g_epever_loadAttrs.measuredCurrent = (s16)(d->load_curr * 100.0f);
    g_epever_loadAttrs.activePower     = (s16)(d->load_pow  * 100.0f);

    /* EP4: Battery Temperature — 0.01°C 단위 */
    g_epever_tempAttrs.measuredValue = (s16)(d->bat_temp * 100.0f);

    /* EP5: Battery SOC */
    g_epever_socAttrs.presentValue = (float)d->bat_soc;

    /* ZCL 리포팅 */
    {
        static u32 s_last_report_tick = 0;
        u32 now = clock_time();

        if (!s_last_report_tick ||
            (u32)(now - s_last_report_tick) >= 10 * 1000 * CLOCK_16M_SYS_TIMER_CLK_1MS) {

            s_last_report_tick = now;

            printf("[ZCL] Report EP1-5 to coord\r\n");

            epInfo_t dstEp;
            TL_SETSTRUCTCONTENT(dstEp, 0);
            dstEp.dstAddrMode       = APS_SHORT_DSTADDR_WITHEP;
            dstEp.dstAddr.shortAddr = 0x0000;
            dstEp.dstEp             = 1;
            dstEp.profileId         = HA_PROFILE_ID;

            /* EP1: Solar V/I/W */
            zcl_report(EPEVER_ENDPOINT_SOLAR, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_epever_solarAttrs.measuredVoltage);

            zcl_report(EPEVER_ENDPOINT_SOLAR, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_epever_solarAttrs.measuredCurrent);

            zcl_report(EPEVER_ENDPOINT_SOLAR, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_epever_solarAttrs.activePower);

            /* EP2: Battery V/I/W */
            zcl_report(EPEVER_ENDPOINT_BATTERY, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_epever_batteryAttrs.measuredVoltage);

            zcl_report(EPEVER_ENDPOINT_BATTERY, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_epever_batteryAttrs.measuredCurrent);

            zcl_report(EPEVER_ENDPOINT_BATTERY, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_epever_batteryAttrs.activePower);

            /* EP3: Load V/I/W */
            zcl_report(EPEVER_ENDPOINT_LOAD, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_epever_loadAttrs.measuredVoltage);

            zcl_report(EPEVER_ENDPOINT_LOAD, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_epever_loadAttrs.measuredCurrent);

            zcl_report(EPEVER_ENDPOINT_LOAD, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_epever_loadAttrs.activePower);

            /* EP4: Battery Temperature */
            zcl_report(EPEVER_ENDPOINT_TEMP, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_TEMPERATURE_MEASUREMENT,
                       ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MEAS_VAL,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_epever_tempAttrs.measuredValue);

            /* EP5: Battery SOC */
            zcl_report(EPEVER_ENDPOINT_SOC, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_GEN_ANALOG_INPUT_BASIC,
                       ZCL_ATTRID_ANALOG_INPUT_PRESENT_VALUE,
                       ZCL_DATA_TYPE_SINGLE_PREC,
                       (u8*)&g_epever_socAttrs.presentValue);
        }
    }
}
