#pragma once
/*********************************************************************
 * @file    epever_ep.h
 *
 * @brief   ZT3L EPever Tracer AN MPPT 모니터 — 엔드포인트 / 공용 선언
 *
 *  EP1 : Electrical Measurement (Solar Voltage / Current / Power)
 *  EP2 : Electrical Measurement (Battery Voltage / Current / Power)
 *  EP3 : Electrical Measurement (Load Voltage / Current / Power)
 *  EP4 : Temperature Measurement (Battery + Device Temp)
 *  EP5 : Analog Input (Battery SOC %)
 *********************************************************************/

#include "zcl_include.h"

/*--------------------------------------------------------------------
 * 엔드포인트 번호
 *------------------------------------------------------------------*/
#define EPEVER_ENDPOINT_SOLAR      1   /* EP1: Solar Electrical Measurement  */
#define EPEVER_ENDPOINT_BATTERY    2   /* EP2: Battery Electrical Measurement */
#define EPEVER_ENDPOINT_LOAD       3   /* EP3: Load Electrical Measurement   */
#define EPEVER_ENDPOINT_TEMP       4   /* EP4: Temperature Measurement       */
#define EPEVER_ENDPOINT_SOC        5   /* EP5: Analog Input (SOC %)          */

#define EPEVER_EP_COUNT            5

/*--------------------------------------------------------------------
 * Global Attribute ID
 *------------------------------------------------------------------*/
#ifndef ZCL_ATTRID_GLOBAL_CLUSTER_REVISION
#define ZCL_ATTRID_GLOBAL_CLUSTER_REVISION                 0xFFFD
#endif

/*--------------------------------------------------------------------
 * Electrical Measurement Cluster (0x0B04)
 *------------------------------------------------------------------*/
#ifndef ZCL_ATTRID_ELECTRICAL_MEAS_MEAS_TYPE
#define ZCL_ATTRID_ELECTRICAL_MEAS_MEAS_TYPE               0x0000
#endif
#ifndef ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE
#define ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE             0x0505
#endif
#ifndef ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT
#define ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT             0x0508
#endif
#ifndef ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER
#define ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER            0x050B
#endif
#ifndef ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_MULTIPLIER
#define ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_MULTIPLIER      0x0600
#endif
#ifndef ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_DIVISOR
#define ZCL_ATTRID_ELECTRICAL_MEAS_AC_VOLT_DIVISOR         0x0601
#endif
#ifndef ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_MULTIPLIER
#define ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_MULTIPLIER   0x0602
#endif
#ifndef ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_DIVISOR
#define ZCL_ATTRID_ELECTRICAL_MEAS_AC_CURRENT_DIVISOR      0x0603
#endif
#ifndef ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_MULTIPLIER
#define ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_MULTIPLIER     0x0604
#endif
#ifndef ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_DIVISOR
#define ZCL_ATTRID_ELECTRICAL_MEAS_AC_POWER_DIVISOR        0x0605
#endif

/*--------------------------------------------------------------------
 * Temperature Measurement Cluster (0x0402)
 *------------------------------------------------------------------*/
#ifndef ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MEAS_VAL
#define ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MEAS_VAL        0x0000
#endif
#ifndef ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MIN_MEAS_VAL
#define ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MIN_MEAS_VAL    0x0001
#endif
#ifndef ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MAX_MEAS_VAL
#define ZCL_ATTRID_TEMPERATURE_MEASUREMENT_MAX_MEAS_VAL    0x0002
#endif

/*--------------------------------------------------------------------
 * Analog Input Cluster (0x000C) — SOC %
 *------------------------------------------------------------------*/
#ifndef ZCL_ATTRID_ANALOG_INPUT_PRESENT_VALUE
#define ZCL_ATTRID_ANALOG_INPUT_PRESENT_VALUE              0x0055
#endif
#ifndef ZCL_ATTRID_ANALOG_INPUT_DESCRIPTION
#define ZCL_ATTRID_ANALOG_INPUT_DESCRIPTION                0x001C
#endif
#ifndef ZCL_ATTRID_ANALOG_INPUT_STATUS_FLAGS
#define ZCL_ATTRID_ANALOG_INPUT_STATUS_FLAGS               0x006F
#endif

/*--------------------------------------------------------------------
 * ZCL Basic / Identify attribute type definition
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
 * EP1: Solar Electrical Measurement 속성
 *------------------------------------------------------------------*/
typedef struct {
    s16  measuredVoltage;      /* 0x0505: ÷100 = V  */
    s16  measuredCurrent;      /* 0x0508: ÷100 = A  */
    s16  activePower;          /* 0x050B: W          */
    u16  acVoltageMultiplier;  /* 0x0600             */
    u16  acVoltageDivisor;     /* 0x0601             */
    u16  acCurrentMultiplier;  /* 0x0602             */
    u16  acCurrentDivisor;     /* 0x0603             */
    u16  acPowerMultiplier;    /* 0x0604             */
    u16  acPowerDivisor;       /* 0x0605             */
} epever_solarAttr_t;

/*--------------------------------------------------------------------
 * EP2: Battery Electrical Measurement 속성
 *------------------------------------------------------------------*/
typedef struct {
    s16  measuredVoltage;
    s16  measuredCurrent;
    s16  activePower;
    u16  acVoltageMultiplier;
    u16  acVoltageDivisor;
    u16  acCurrentMultiplier;
    u16  acCurrentDivisor;
    u16  acPowerMultiplier;
    u16  acPowerDivisor;
} epever_batteryAttr_t;

/*--------------------------------------------------------------------
 * EP3: Load Electrical Measurement 속성
 *------------------------------------------------------------------*/
typedef struct {
    s16  measuredVoltage;
    s16  measuredCurrent;
    s16  activePower;
    u16  acVoltageMultiplier;
    u16  acVoltageDivisor;
    u16  acCurrentMultiplier;
    u16  acCurrentDivisor;
    u16  acPowerMultiplier;
    u16  acPowerDivisor;
} epever_loadAttr_t;

/*--------------------------------------------------------------------
 * EP4: Temperature Measurement 속성 (배터리 온도)
 *------------------------------------------------------------------*/
typedef struct {
    s16  measuredValue;        /* 0x0000: 0.01°C 단위 */
    s16  minMeasuredValue;     /* 0x0001              */
    s16  maxMeasuredValue;     /* 0x0002              */
} epever_tempAttr_t;

/*--------------------------------------------------------------------
 * EP5: Analog Input 속성 (Battery SOC %)
 *------------------------------------------------------------------*/
typedef struct {
    float presentValue;        /* 0x0055: SOC 0~100 % */
    u8    statusFlags;         /* 0x006F              */
} epever_socAttr_t;

extern epever_solarAttr_t   g_epever_solarAttrs;
extern epever_batteryAttr_t g_epever_batteryAttrs;
extern epever_loadAttr_t    g_epever_loadAttrs;
extern epever_tempAttr_t    g_epever_tempAttrs;
extern epever_socAttr_t     g_epever_socAttrs;

/* Simple Descriptor */
extern const af_simple_descriptor_t epever_ep1_simpleDesc;
extern const af_simple_descriptor_t epever_ep2_simpleDesc;
extern const af_simple_descriptor_t epever_ep3_simpleDesc;
extern const af_simple_descriptor_t epever_ep4_simpleDesc;
extern const af_simple_descriptor_t epever_ep5_simpleDesc;

/* EP별 클러스터 리스트 */
extern const zcl_specClusterInfo_t * const g_epClusterList[EPEVER_EP_COUNT];
extern u8 g_epClusterNum[EPEVER_EP_COUNT];

/*--------------------------------------------------------------------
 * EPever 파싱 결과 구조체
 *------------------------------------------------------------------*/
typedef struct {
    float solar_volt;    /* V  */
    float solar_curr;    /* A  */
    float solar_pow;     /* W  */
    float bat_volt;      /* V  */
    float bat_curr;      /* A  */
    float chg_pow;       /* W  */
    float load_volt;     /* V  */
    float load_curr;     /* A  */
    float load_pow;      /* W  */
    float bat_temp;      /* °C */
    float dev_temp;      /* °C */
    u8    bat_soc;       /* %  */
    bool  valid;
} epever_data_t;

/*--------------------------------------------------------------------
 * ZCL 콜백
 *------------------------------------------------------------------*/
status_t epever_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t epever_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);

/*--------------------------------------------------------------------
 * 속성 초기화 / 업데이트
 *------------------------------------------------------------------*/
void epever_attrs_init(void);
void epever_attrs_update(const epever_data_t *d);
