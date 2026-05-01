#pragma once
/*********************************************************************
 * @file    juntek_ep.h
 *
 * @brief   ZT3L JUNTEK BMS 모니터 — 엔드포인트 / 공용 선언
 *
 *  EP1 : Electrical Measurement (Voltage / Current / Power)
 *  EP2 : Temperature Measurement
 *  EP3 : Binary Input (Relay State: 충전/방전)
 *  EP4 : Simple Metering (Remain Ah)
 *  EP5 : Analog Input (Elapsed Minutes)
 *********************************************************************/

/*--------------------------------------------------------------------
 * SDK 헤더 먼저 — 속성 ID가 SDK에 이미 정의되어 있으면 그 값을 우선 사용
 *------------------------------------------------------------------*/
#include "zcl_include.h"

/*--------------------------------------------------------------------
 * 엔드포인트 번호
 *------------------------------------------------------------------*/
#ifndef JUNTEK_ENDPOINT_ELEC
#define JUNTEK_ENDPOINT_ELEC       1   /* EP1: Electrical Measurement */
#endif
#define JUNTEK_ENDPOINT_TEMP       2   /* EP2: Temperature */
#define JUNTEK_ENDPOINT_RELAY      3   /* EP3: Binary Input (relay state) */
#define JUNTEK_ENDPOINT_METERING   4   /* EP4: Metering (remain_ah) */
#define JUNTEK_ENDPOINT_ANALOG     5   /* EP5: Analog Input (elapsed_min) */

#define JUNTEK_EP_COUNT            5   /* 총 엔드포인트 수 */

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
 * Binary Input Cluster (0x000F)
 *------------------------------------------------------------------*/
#ifndef ZCL_ATTRID_BINARY_INPUT_PRESENT_VALUE
#define ZCL_ATTRID_BINARY_INPUT_PRESENT_VALUE              0x0055
#endif
#ifndef ZCL_ATTRID_BINARY_INPUT_STATUS_FLAGS
#define ZCL_ATTRID_BINARY_INPUT_STATUS_FLAGS               0x006F
#endif

/*--------------------------------------------------------------------
 * Simple Metering Cluster (0x0702)
 *------------------------------------------------------------------*/
#ifndef ZCL_ATTRID_METERING_CURRENT_SUMMATION_DELIVERD
#define ZCL_ATTRID_METERING_CURRENT_SUMMATION_DELIVERD     0x0000
#endif
#ifndef ZCL_ATTRID_METERING_UNIT_OF_MEASURE
#define ZCL_ATTRID_METERING_UNIT_OF_MEASURE                0x0300
#endif
#ifndef ZCL_ATTRID_METERING_MULTIPLIER
#define ZCL_ATTRID_METERING_MULTIPLIER                     0x0301
#endif
#ifndef ZCL_ATTRID_METERING_DIVISOR
#define ZCL_ATTRID_METERING_DIVISOR                        0x0302
#endif
#ifndef ZCL_ATTRID_METERING_SUMMATION_FORMATTING
#define ZCL_ATTRID_METERING_SUMMATION_FORMATTING           0x0303
#endif
#ifndef ZCL_ATTRID_METERING_METERING_DEVICE_TYPE
#define ZCL_ATTRID_METERING_METERING_DEVICE_TYPE           0x0306
#endif

/*--------------------------------------------------------------------
 * Analog Input Cluster (0x000C)
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
 * BMS Filter Config — Manufacturer-Specific 속성 (EP1, 0x0B04)
 * Attribute ID: 0xFF00 ~ 0xFF0B (manufacturer-specific 범위)
 * 모두 SINGLE_PREC (float, 4바이트), READ/WRITE
 *------------------------------------------------------------------*/
#define ZCL_ATTRID_FILTER_VOLT_MIN      0xFF00  /* 전압 하한 (V) */
#define ZCL_ATTRID_FILTER_VOLT_MAX      0xFF01  /* 전압 상한 (V) */
#define ZCL_ATTRID_FILTER_CURR_MIN      0xFF02  /* 전류 하한 (A, 음수=방전) */
#define ZCL_ATTRID_FILTER_CURR_MAX      0xFF03  /* 전류 상한 (A) */
#define ZCL_ATTRID_FILTER_TEMP_MIN      0xFF04  /* 온도 하한 (°C) */
#define ZCL_ATTRID_FILTER_TEMP_MAX      0xFF05  /* 온도 상한 (°C) */
#define ZCL_ATTRID_FILTER_AH_MAX        0xFF06  /* 잔량 상한 (Ah) */
#define ZCL_ATTRID_FILTER_VOLT_RATE     0xFF07  /* 전압 rate limit (V/sample) */
#define ZCL_ATTRID_FILTER_CURR_RATE     0xFF08  /* 전류 rate limit (A/sample) */
#define ZCL_ATTRID_FILTER_TEMP_RATE     0xFF09  /* 온도 rate limit (°C/sample) */

#define ZCL_FILTER_ATTR_NUM             10

/* NV Item ID — NV_MODULE_APP 영역, APP 전용 아이템 */
#define NV_ITEM_JUNTEK_FILTER_CFG       0x2D   /* NV_MODULE_APP 내 앱 전용 아이템 */

/* 필터 설정 구조체 — NV에 저장되는 단위 */
typedef struct {
    float volt_min;     /* 기본: 9.0  V  */
    float volt_max;     /* 기본: 15.6 V  */
    float curr_min;     /* 기본: -210 A  */
    float curr_max;     /* 기본: 130  A  */
    float temp_min;     /* 기본: -20  °C */
    float temp_max;     /* 기본: 60   °C */
    float ah_max;       /* 기본: 265  Ah */
    float volt_rate;    /* 기본: 0.5  V/sample  */
    float curr_rate;    /* 기본: 80   A/sample  */
    float temp_rate;    /* 기본: 2.0  °C/sample */
} juntek_filter_cfg_t;

extern juntek_filter_cfg_t g_juntek_filterCfg;

void juntek_filter_cfg_load(void);
void juntek_filter_cfg_save(void);

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

/* Basic / Identify 공용 속성 */
extern zcl_basicAttr_t    g_zcl_basicAttrs;
extern zcl_identifyAttr_t g_zcl_identifyAttrs;

/* ZCL 콜백 */

/* EP1: Electrical Measurement 속성 */
typedef struct {
    s16  measuredVoltage;      /* 0x0505: ÷100 = V */
    s16  measuredCurrent;      /* 0x0508: ÷100 = A */
    s16  activePower;          /* 0x050B: W */
    u16  acVoltageMultiplier;  /* 0x0600 */
    u16  acVoltageDivisor;     /* 0x0601 */
    u16  acCurrentMultiplier;  /* 0x0602 */
    u16  acCurrentDivisor;     /* 0x0603 */
    u16  acPowerMultiplier;    /* 0x0604 */
    u16  acPowerDivisor;       /* 0x0605 */
} juntek_elecAttr_t;

/* EP2: Temperature Measurement 속성 */
typedef struct {
    s16  measuredValue;        /* 0x0000: 0.01°C 단위 */
    s16  minMeasuredValue;     /* 0x0001 */
    s16  maxMeasuredValue;     /* 0x0002 */
} juntek_tempAttr_t;

/* EP3: Binary Input 속성 (Relay State) */
typedef struct {
    bool presentValue;         /* 0x0055: true=충전, false=방전 */
    u8   statusFlags;          /* 0x006F */
} juntek_relayAttr_t;

/* EP4: Simple Metering 속성 (Remain Ah) */
typedef struct {
    u8   currentSummation[6];  /* 0x0000: UINT48, mAh 단위 (LSB first) */
    u8   unitOfMeasure;        /* 0x0300: 0x01 = A (암페어) */
    u32  multiplier;           /* 0x0301 */
    u32  divisor;              /* 0x0302: 1000 → ÷1000 = Ah */
    u8   summationFormatting;  /* 0x0303 */
    u8   meteringDeviceType;   /* 0x0306: 0x00 = Electric Metering */
} juntek_meteringAttr_t;

/* EP5: Analog Input 속성 (Elapsed Minutes) */
typedef struct {
    float presentValue;        /* 0x0055: 경과 시간(분), IEEE 754 float */
    u8    statusFlags;         /* 0x006F */
} juntek_analogAttr_t;

extern juntek_elecAttr_t     g_juntek_elecAttrs;
extern juntek_tempAttr_t     g_juntek_tempAttrs;
extern juntek_relayAttr_t    g_juntek_relayAttrs;
extern juntek_meteringAttr_t g_juntek_meteringAttrs;
extern juntek_analogAttr_t   g_juntek_analogAttrs;

/* Simple Descriptor */
extern const af_simple_descriptor_t juntek_ep1_simpleDesc;  /* Electrical */
extern const af_simple_descriptor_t juntek_ep2_simpleDesc;  /* Temperature */
extern const af_simple_descriptor_t juntek_ep3_simpleDesc;  /* Binary Input */
extern const af_simple_descriptor_t juntek_ep4_simpleDesc;  /* Metering */
extern const af_simple_descriptor_t juntek_ep5_simpleDesc;  /* Analog Input */

/* EP별 클러스터 리스트 포인터 테이블 */
extern const zcl_specClusterInfo_t * const g_epClusterList[JUNTEK_EP_COUNT];
extern u8 g_epClusterNum[JUNTEK_EP_COUNT];  /* EP별 클러스터 수 */

/*--------------------------------------------------------------------
 * JUNTEK 데이터 구조체 (파싱 결과)
 *------------------------------------------------------------------*/
typedef struct {
    float voltage;      /* V */
    float current;      /* A (방전 시 음수) */
    float power;        /* W */
    float remain_ah;    /* Ah */
    float temperature;  /* °C */
    long  elapsed_min;  /* 분 */
    int   relay;        /* 1=충전, 0=방전 */
    bool  valid;        /* 파싱 성공 여부 */
} juntek_data_t;

/*--------------------------------------------------------------------
 * ZCL 콜백 (juntekEpCfg.c / juntekMonitor.c 에서 선언)
 *------------------------------------------------------------------*/
status_t juntek_basicCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);
status_t juntek_identifyCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload);

/*--------------------------------------------------------------------
 * JUNTEK 속성 업데이트 — ZCL 속성에 값 반영 후 리포팅
 *------------------------------------------------------------------*/
void juntek_attrs_update(const juntek_data_t *d);
void juntek_attrs_init(void);
