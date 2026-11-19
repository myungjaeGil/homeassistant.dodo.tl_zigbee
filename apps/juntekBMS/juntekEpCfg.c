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
 *  EP6 : OnOff — 주행충전 ON/OFF (신규)
 *  EP7 : OnOff — 충전전류 Full/Half (신규)
 *********************************************************************/

#include "tl_common.h"
#include "zcl_include.h"
#include "ota.h"            /* zcl_ota_register() — OTA 클러스터 핸들러 */
#include "juntekMonitor.h"
#include "juntek_ep.h"
#include "juntekCtrl.h"    /* relay_drvchg_set() / relay_chgcur_set() */

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

/*--------------------------------------------------------------------
 * Soft Reset 속성 (Manufacturer-Specific 0xFF00)
 *  HA UI에서 Write 1 → sys_reboot() 호출
 *  juntek_basicCb() 의 ZCL_CMD_WRITE 핸들러에서 감지
 *------------------------------------------------------------------*/
#define ZCL_ATTRID_SOFT_RESET       0xFF00
static u8 g_soft_reset_attr = 0;   /* 항상 0으로 유지; write=1 → 리셋 트리거 */

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
    { ZCL_ATTRID_SOFT_RESET,              ZCL_DATA_TYPE_UINT8,    ACCESS_CONTROL_READ | ACCESS_CONTROL_WRITE, (u8*)&g_soft_reset_attr },
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

/* 마지막으로 NV 에 저장된(또는 NV 에서 읽은) 값 — 변경 감지·복원용 */
static juntek_filter_cfg_t s_filterSaved;

void juntek_filter_cfg_save(void)
{
    memcpy(&s_filterSaved, &g_juntek_filterCfg, sizeof(juntek_filter_cfg_t));
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
        DBG_LOG("filter cfg: defaults saved\r\n");
    } else {
        memcpy(&s_filterSaved, &g_juntek_filterCfg, sizeof(juntek_filter_cfg_t));
        DBG_LOG("filter cfg: loaded from NV\r\n");
    }
}

/* float 유효성 — NaN/Inf 는 지수부가 전부 1 */
static bool filter_val_ok(float f)
{
    u32 bits;
    memcpy(&bits, &f, 4);
    return ((bits & 0x7F800000) != 0x7F800000);
}

static bool filter_cfg_valid(const juntek_filter_cfg_t *c)
{
    const float *f = (const float *)c;
    u8 i;
    for (i = 0; i < sizeof(juntek_filter_cfg_t) / sizeof(float); i++) {
        if (!filter_val_ok(f[i])) return FALSE;
    }
    if (c->volt_min >= c->volt_max) return FALSE;
    if (c->curr_min >= c->curr_max) return FALSE;
    if (c->temp_min >= c->temp_max) return FALSE;
    if (c->ah_max   <= 0.0f)        return FALSE;
    if (c->volt_rate <= 0.0f || c->curr_rate <= 0.0f || c->temp_rate <= 0.0f) return FALSE;
    return TRUE;
}

/* [추가] 필터 설정 변경 감시
 *  Z2M 의 전역 Write Attributes 는 스택이 속성 메모리에 직접 쓰고 클러스터 콜백
 *  (filter_cfg_writeCb)은 부르지 않으므로, 기존에는 값이 NV 에 저장되지 않았음.
 *  ZCL_CMD_WRITE 경로와 app_task(500ms)에서 호출해 값이 바뀌었으면
 *  유효성 검사 후 저장, 잘못된 값(NaN, min>=max, rate<=0)이면 이전 값으로 되돌림. */
void juntek_filter_cfg_check(void)
{
    if (memcmp(&s_filterSaved, &g_juntek_filterCfg, sizeof(juntek_filter_cfg_t)) == 0) {
        return;
    }
    if (filter_cfg_valid(&g_juntek_filterCfg)) {
        juntek_filter_cfg_save();
        DBG_LOG("filter cfg: changed -> saved\r\n");
    } else {
        memcpy(&g_juntek_filterCfg, &s_filterSaved, sizeof(juntek_filter_cfg_t));
        DBG_LOG("filter cfg: invalid value -> restored\r\n");
    }
}

/* [수정] 클러스터 콜백은 클러스터 전용 명령에만 호출되고 전역 Write 에는 호출되지 않음.
 * 이 클러스터에는 전용 명령이 없으므로 콜백을 두지 않고 juntek_filter_cfg_check() 로 저장 */

/* [추가] RS485 통신 상태 속성 값 (부팅 직후는 정상으로 시작, 5초 안에 프레임이 없으면 타임아웃) */
juntek_rs485Attr_t g_juntek_rs485Attrs = {
    .status = RS485_STATUS_OK,
    .rxAge  = 0,
    .errCnt = 0,
};

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
    /* [추가] RS485 통신 상태 — 읽기/리포트 전용 (값은 juntek_rs485_tick() 이 갱신) */
    { ZCL_ATTRID_RS485_STATUS,      ZCL_DATA_TYPE_ENUM8,       ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_rs485Attrs.status  },
    { ZCL_ATTRID_RS485_RX_AGE,      ZCL_DATA_TYPE_UINT16,      ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_rs485Attrs.rxAge   },
    { ZCL_ATTRID_RS485_ERR_CNT,     ZCL_DATA_TYPE_UINT16,      ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_rs485Attrs.errCnt  },
};
#define ZCL_FILTER_ATTR_NUM_DEF  (sizeof(filter_attrTbl) / sizeof(zclAttrInfo_t))

/* 필터 클러스터 등록 래퍼 — EP1에 별도 클러스터(0xFF00)로 등록 */
/* ZCL_CLUSTER_JUNTEK_FILTER (0xFF00) 는 juntek_ep.h 로 이동 */

static status_t filter_register(u8 ep, u16 manuCode, u8 attrNum,
                                 const zclAttrInfo_t *tbl, cluster_forAppCb_t cb)
{
    return zcl_registerCluster(ep, ZCL_CLUSTER_JUNTEK_FILTER,
                                manuCode, attrNum, tbl, NULL, cb);
}

static const zclAttrInfo_t elec_attrTbl[] = {
    /* [수정] ZCL Electrical Measurement 스펙: rmsVoltage/rmsCurrent 는 UINT16
     * (음수 불가능한 RMS 값). 기존 INT16 정의는 zigbee-herdsman 의
     * configReport 데이터 타입 검증에서 거부되어(INVALID_DATA_TYPE)
     * Z2M 의 configure 단계가 실패했음.
     * activePower 는 방향성이 있어 INT16 유지(스펙과 일치, 정상 동작). */
    { ZCL_ATTRID_ELECTRICAL_MEAS_RMS_VOLTAGE,         ZCL_DATA_TYPE_UINT16,   ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_elecAttrs.measuredVoltage },
    { ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,         ZCL_DATA_TYPE_UINT16,   ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_elecAttrs.measuredCurrent },
    { ZCL_ATTRID_ELECTRICAL_MEAS_ACTIVE_POWER,        ZCL_DATA_TYPE_INT16,    ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_elecAttrs.activePower },
    /* [추가] 부호 있는 DC 전류 — 충/방전 방향을 전류 값 자체로 전달 (HA에서 방전 시 음수) */
    { ZCL_ATTRID_ELECTRICAL_MEAS_DC_CURRENT,          ZCL_DATA_TYPE_INT16,    ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE, (u8*)&g_juntek_elecAttrs.dcCurrent },
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
 * [추가] EP6/EP7 릴레이 상태 — NV 저장/복원 + 보고
 *
 *  - onOff 속성은 READ | REPORTABLE (쓰기 불가, ZCL 스펙과 동일).
 *    상태 변경은 On/Off/Toggle 명령으로만 → 항상 콜백을 거쳐 GPIO 와 일치.
 *  - 명령으로 상태가 바뀔 때마다 NV 저장 → 재부팅/전원 순단 후 그대로 복원.
 *  - 가입(또는 재부팅 후 기존 네트워크 복귀) 시 현재 상태를 코디네이터에 보고.
 *==================================================================*/
void juntek_relay_state_save(void)
{
    juntek_relay_nv_t nv;
    nv.magic  = JUNTEK_RELAY_NV_MAGIC;
    nv.drvChg = g_juntek_drvChgAttrs.onOff ? 1 : 0;
    nv.chgCur = g_juntek_chgCurAttrs.onOff ? 1 : 0;
    nv.rsv    = 0;

    nv_sts_t st = nv_flashWriteNew(1, NV_MODULE_APP, NV_ITEM_JUNTEK_RELAY_STATE,
                                   sizeof(nv), (u8 *)&nv);
    DBG_LOG("relay nv: save drv=%d cur=%d st=%d\r\n", nv.drvChg, nv.chgCur, (int)st);
    (void)st;
}

/* juntek_attrs_init() 에서 호출 — 저장된 상태를 속성 + GPIO 에 반영 */
void juntek_relay_state_restore(void)
{
    juntek_relay_nv_t nv;
    nv_sts_t st = nv_flashReadNew(1, NV_MODULE_APP, NV_ITEM_JUNTEK_RELAY_STATE,
                                  sizeof(nv), (u8 *)&nv);

    if (st == NV_SUCC && nv.magic == JUNTEK_RELAY_NV_MAGIC) {
        g_juntek_drvChgAttrs.onOff = nv.drvChg ? 1 : 0;
        g_juntek_chgCurAttrs.onOff = nv.chgCur ? 1 : 0;
        DBG_LOG("relay nv: restored drv=%d cur=%d\r\n", nv.drvChg, nv.chgCur);
    } else {
        /* 최초 부팅(NV 없음) — 기본값 OFF/Full 로 저장 */
        g_juntek_drvChgAttrs.onOff = 0;
        g_juntek_chgCurAttrs.onOff = 0;
        juntek_relay_state_save();
        DBG_LOG("relay nv: none -> default OFF saved\r\n");
    }

    relay_drvchg_set(g_juntek_drvChgAttrs.onOff ? TRUE : FALSE);
    relay_chgcur_set(g_juntek_chgCurAttrs.onOff ? TRUE : FALSE);
}

/* EP6/EP7 onOff 현재 값을 코디네이터로 즉시 보고 */
void juntek_relay_report(u8 ep)
{
    u8 *pVal;

    if (!zb_isDeviceJoinedNwk()) return;

    if (ep == JUNTEK_ENDPOINT_DRVCHG)      pVal = &g_juntek_drvChgAttrs.onOff;
    else if (ep == JUNTEK_ENDPOINT_CHGCUR) pVal = &g_juntek_chgCurAttrs.onOff;
    else return;

    epInfo_t dstEp;
    TL_SETSTRUCTCONTENT(dstEp, 0);
    dstEp.dstAddrMode       = APS_SHORT_DSTADDR_WITHEP;
    dstEp.dstAddr.shortAddr = 0x0000;  /* 코디네이터 */
    dstEp.dstEp             = 1;
    dstEp.profileId         = HA_PROFILE_ID;

    zcl_report(ep, &dstEp, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
               MANUFACTURER_CODE_NONE, ZCL_CLUSTER_GEN_ON_OFF,
               ZCL_ATTRID_ONOFF, ZCL_DATA_TYPE_BOOLEAN, pVal);
}

void juntek_relay_report_all(void)
{
    juntek_relay_report(JUNTEK_ENDPOINT_DRVCHG);
    juntek_relay_report(JUNTEK_ENDPOINT_CHGCUR);
}

/* [추가] RS485 통신 상태 3개 속성을 코디네이터로 즉시 보고 (EP1, 클러스터 0xFF00)
 *  BMS 데이터 보고(juntek_attrs_update)는 :r50 이 들어올 때만 실행되므로
 *  RS485 가 끊기면 아무것도 보고되지 않음 → 이 상태 보고는 별도 1초 타이머에서 보냄 */
void juntek_rs485_report(void)
{
    if (!zb_isDeviceJoinedNwk()) return;

    epInfo_t dstEp;
    TL_SETSTRUCTCONTENT(dstEp, 0);
    dstEp.dstAddrMode       = APS_SHORT_DSTADDR_WITHEP;
    dstEp.dstAddr.shortAddr = 0x0000;  /* 코디네이터 */
    dstEp.dstEp             = 1;
    dstEp.profileId         = HA_PROFILE_ID;

    zcl_report(JUNTEK_ENDPOINT_ELEC, &dstEp, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
               MANUFACTURER_CODE_NONE, ZCL_CLUSTER_JUNTEK_FILTER,
               ZCL_ATTRID_RS485_STATUS, ZCL_DATA_TYPE_ENUM8, (u8*)&g_juntek_rs485Attrs.status);
    zcl_report(JUNTEK_ENDPOINT_ELEC, &dstEp, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
               MANUFACTURER_CODE_NONE, ZCL_CLUSTER_JUNTEK_FILTER,
               ZCL_ATTRID_RS485_RX_AGE, ZCL_DATA_TYPE_UINT16, (u8*)&g_juntek_rs485Attrs.rxAge);
    zcl_report(JUNTEK_ENDPOINT_ELEC, &dstEp, TRUE, ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
               MANUFACTURER_CODE_NONE, ZCL_CLUSTER_JUNTEK_FILTER,
               ZCL_ATTRID_RS485_ERR_CNT, ZCL_DATA_TYPE_UINT16, (u8*)&g_juntek_rs485Attrs.errCnt);
}

/* 명령 → 새 상태 계산. 지원하지 않는 명령이면 FALSE */
static bool onoff_cmd_to_state(u8 cmdId, u8 cur, bool *pOn)
{
    switch (cmdId) {
    case ZCL_CMD_ONOFF_ON:     *pOn = TRUE;               return TRUE;
    case ZCL_CMD_ONOFF_OFF:    *pOn = FALSE;              return TRUE;
    case ZCL_CMD_ONOFF_TOGGLE: *pOn = cur ? FALSE : TRUE; return TRUE;
    default:                                              return FALSE;
    }
}

/*====================================================================
 * EP6: OnOff 속성 (주행충전 ON/OFF) — 신규 채널
 *
 * [주의] zcl_onOff_register() 는 SDK(zcl_onOff.h/.c) 제공 함수로 가정함.
 *        juntekCtrl.c 의 sampleLight_onOffInit/Update 스텁이 이미
 *        SDK 쪽에서 OnOff 클러스터 모듈을 참조하고 있다는 증거이므로
 *        존재할 가능성이 높지만, 실제 시그니처/매크로명은
 *        SDK의 zcl_onOff.h 를 한 번 대조해 주세요.
 *==================================================================*/
juntek_onoffAttr_t g_juntek_drvChgAttrs = { .onOff = 0 };

static const zclAttrInfo_t drvChg_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                   ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE,                        (u8*)&g_juntek_drvChgAttrs.onOff },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_DRVCHG_ATTR_NUM  (sizeof(drvChg_attrTbl) / sizeof(zclAttrInfo_t))

/* OnOff 명령(ON/OFF/Toggle) 처리 콜백
 * [수정] SDK가 cb 호출 전에 onOff 속성을 자동으로 갱신해줄 것으로
 * 가정했으나, 실제 로그 확인 결과 그렇지 않음(cmdId=0x01인데도
 * onOff가 항상 0으로 남아있었음). cmdId로 직접 on/off를 판단해서
 * 속성과 GPIO를 우리가 직접 갱신한다. */
status_t juntek_drvChgCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    (void)pAddrInfo; (void)cmdPayload;
    bool on;

    if (!onoff_cmd_to_state(cmdId, g_juntek_drvChgAttrs.onOff, &on)) {
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }

    u8 prev = g_juntek_drvChgAttrs.onOff;
    g_juntek_drvChgAttrs.onOff = on ? 1 : 0;
    DBG_LOG("[EP6 drvChg] cmdId=0x%02x -> onOff=%d -> PD2\r\n",
            cmdId, g_juntek_drvChgAttrs.onOff);
    relay_drvchg_set(on);

    if (prev != g_juntek_drvChgAttrs.onOff) {
        juntek_relay_state_save();          /* [추가] 재부팅 복원용 */
    }
    juntek_relay_report(JUNTEK_ENDPOINT_DRVCHG);

    return ZCL_STA_SUCCESS;
}

/*====================================================================
 * EP7: OnOff 속성 (충전전류 Full/Half) — 신규 채널
 *  onOff=1 → Half, onOff=0 → Full
 *==================================================================*/
juntek_onoffAttr_t g_juntek_chgCurAttrs = { .onOff = 0 };

static const zclAttrInfo_t chgCur_attrTbl[] = {
    { ZCL_ATTRID_ONOFF,                   ZCL_DATA_TYPE_BOOLEAN, ACCESS_CONTROL_READ | ACCESS_CONTROL_REPORTABLE,                        (u8*)&g_juntek_chgCurAttrs.onOff },
    { ZCL_ATTRID_GLOBAL_CLUSTER_REVISION, ZCL_DATA_TYPE_UINT16,  ACCESS_CONTROL_READ,                             (u8*)&zcl_attr_global_clusterRevision },
};
#define ZCL_CHGCUR_ATTR_NUM  (sizeof(chgCur_attrTbl) / sizeof(zclAttrInfo_t))

/* EP6 과 동일 — cmdId 로 직접 판단, 변경 시 NV 저장 + 보고 */
status_t juntek_chgCurCb(zclIncomingAddrInfo_t *pAddrInfo, u8 cmdId, void *cmdPayload)
{
    (void)pAddrInfo; (void)cmdPayload;
    bool on;

    if (!onoff_cmd_to_state(cmdId, g_juntek_chgCurAttrs.onOff, &on)) {
        return ZCL_STA_UNSUP_CLUSTER_COMMAND;
    }

    u8 prev = g_juntek_chgCurAttrs.onOff;
    g_juntek_chgCurAttrs.onOff = on ? 1 : 0;
    DBG_LOG("[EP7 chgCur] cmdId=0x%02x -> onOff=%d -> PC3\r\n",
            cmdId, g_juntek_chgCurAttrs.onOff);
    relay_chgcur_set(on);

    if (prev != g_juntek_chgCurAttrs.onOff) {
        juntek_relay_state_save();          /* [추가] 재부팅 복원용 */
    }
    juntek_relay_report(JUNTEK_ENDPOINT_CHGCUR);

    return ZCL_STA_SUCCESS;
}

/*====================================================================
 * Simple Descriptor
 *==================================================================*/

/* EP1 입력 클러스터: Basic + Identify + Electrical Measurement */
static const u16 ep1_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,   /* 0x0B04 — 표준 Electrical Measurement */
    ZCL_CLUSTER_JUNTEK_FILTER,               /* [추가] 0xFF00 — 필터 설정 (등록은 돼 있었으나 디스크립터에 없었음) */
};
static const u16 ep1_outClusterList[] = {
#if ZCL_OTA_SUPPORT
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

/* EP6 입력 클러스터: Basic + Identify + OnOff (주행충전) */
static const u16 ep6_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_GEN_ON_OFF,
};

const af_simple_descriptor_t juntek_ep6_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_ON_OFF_SWITCH,
    JUNTEK_ENDPOINT_DRVCHG,
    1, 0,
    sizeof(ep6_inClusterList) / sizeof(u16),
    0,
    (u16*)ep6_inClusterList,
    NULL,
};

/* EP7 입력 클러스터: Basic + Identify + OnOff (충전전류) */
static const u16 ep7_inClusterList[] = {
    ZCL_CLUSTER_GEN_BASIC,
    ZCL_CLUSTER_GEN_IDENTIFY,
    ZCL_CLUSTER_GEN_ON_OFF,
};

const af_simple_descriptor_t juntek_ep7_simpleDesc = {
    HA_PROFILE_ID,
    HA_DEV_ON_OFF_SWITCH,
    JUNTEK_ENDPOINT_CHGCUR,
    1, 0,
    sizeof(ep7_inClusterList) / sizeof(u16),
    0,
    (u16*)ep7_inClusterList,
    NULL,
};

/* EP1: Electrical Measurement */
static const zcl_specClusterInfo_t g_clusterList_ep1[] = {
    { ZCL_CLUSTER_GEN_BASIC,                   MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,      basic_attrTbl,    zcl_basic_register,    (void*)juntek_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,                MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM,   identify_attrTbl, zcl_identify_register, (void*)juntek_identifyCb },
    { ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,   MANUFACTURER_CODE_NONE, ZCL_ELEC_ATTR_NUM,       elec_attrTbl,     elec_register,         NULL                     },
    { ZCL_CLUSTER_JUNTEK_FILTER,               MANUFACTURER_CODE_NONE, ZCL_FILTER_ATTR_NUM_DEF, filter_attrTbl,   filter_register,       NULL                     },
    /* [수정] OTA 클러스터는 여기서 등록하지 않음.
     *  ota_init() 이 SDK 의 g_otaClusterList(ota_attrTbl + zcl_otaCb)로 직접 등록한다.
     *  여기에 (속성 0개, 콜백 NULL)로 먼저 등록하면 이 빈 항목이 먼저 검색되어
     *   - genOta 속성 읽기 → UNSUPPORTED_ATTRIBUTE
     *   - Image Notify     → clusterAppCb 없음 → ZCL_STA_FAILURE 즉시 응답
     *  이 되어 Z2M OTA 가 "Device didn't respond to OTA request" 로 즉시 실패했음.
     *  (simple descriptor 의 OUT 클러스터 목록에는 ZCL_CLUSTER_OTA 유지) */
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

/* EP6: OnOff (주행충전 ON/OFF) */
static const zcl_specClusterInfo_t g_clusterList_ep6[] = {
    { ZCL_CLUSTER_GEN_BASIC,                   MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)juntek_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,                MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)juntek_identifyCb },
    { ZCL_CLUSTER_GEN_ON_OFF,                  MANUFACTURER_CODE_NONE, ZCL_DRVCHG_ATTR_NUM,   drvChg_attrTbl,   zcl_onOff_register,    (void*)juntek_drvChgCb   },
};

/* EP7: OnOff (충전전류 Full/Half) */
static const zcl_specClusterInfo_t g_clusterList_ep7[] = {
    { ZCL_CLUSTER_GEN_BASIC,                   MANUFACTURER_CODE_NONE, ZCL_BASIC_ATTR_NUM,    basic_attrTbl,    zcl_basic_register,    (void*)juntek_basicCb    },
    { ZCL_CLUSTER_GEN_IDENTIFY,                MANUFACTURER_CODE_NONE, ZCL_IDENTIFY_ATTR_NUM, identify_attrTbl, zcl_identify_register, (void*)juntek_identifyCb },
    { ZCL_CLUSTER_GEN_ON_OFF,                  MANUFACTURER_CODE_NONE, ZCL_CHGCUR_ATTR_NUM,   chgCur_attrTbl,   zcl_onOff_register,    (void*)juntek_chgCurCb   },
};

/* EP별 클러스터 수 — zcl_register()에 EP별로 전달 */
u8 g_epClusterNum[JUNTEK_EP_COUNT] = {
    sizeof(g_clusterList_ep1) / sizeof(zcl_specClusterInfo_t),  /* EP1 */
    sizeof(g_clusterList_ep2) / sizeof(zcl_specClusterInfo_t),  /* EP2 */
    sizeof(g_clusterList_ep3) / sizeof(zcl_specClusterInfo_t),  /* EP3 */
    sizeof(g_clusterList_ep4) / sizeof(zcl_specClusterInfo_t),  /* EP4 */
    sizeof(g_clusterList_ep5) / sizeof(zcl_specClusterInfo_t),  /* EP5 */
    sizeof(g_clusterList_ep6) / sizeof(zcl_specClusterInfo_t),  /* EP6 */
    sizeof(g_clusterList_ep7) / sizeof(zcl_specClusterInfo_t),  /* EP7 */
};

const zcl_specClusterInfo_t * const g_epClusterList[JUNTEK_EP_COUNT] = {
    g_clusterList_ep1,
    g_clusterList_ep2,
    g_clusterList_ep3,
    g_clusterList_ep4,
    g_clusterList_ep5,
    g_clusterList_ep6,
    g_clusterList_ep7,
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

    /* [수정] EP6/EP7 릴레이 — 0 으로 초기화하던 것을 NV 복원으로 변경 */
    juntek_relay_state_restore();
}

/*====================================================================
 * JUNTEK 속성 업데이트 — 파싱된 데이터를 ZCL 속성에 반영
 *==================================================================*/
void juntek_attrs_update(const juntek_data_t *d)
{
    u32 mAh;

    if (!d || !d->valid) return;

    /* EP1: Electrical — 단위 변환 (÷divisor로 실수 표현) */
    /* [수정] rmsCurrent 는 ZCL 스펙상 UINT16(절댓값).
     * 충전/방전 방향은 EP3 relay_state(genBinaryInput) 로 이미 구분되므로
     * 여기서는 절댓값만 저장 — 이전에는 음수를 INT16에 담아 보냈으나
     * zigbee-herdsman 의 configReport 데이터 타입 검증에서 거부됨 */
    {
        float cur_abs = (d->current < 0.0f) ? -d->current : d->current;
        g_juntek_elecAttrs.measuredVoltage  = (u16)(d->voltage  * 100.0f);   /* 0.01V 단위 */
        g_juntek_elecAttrs.measuredCurrent  = (u16)(cur_abs     * 100.0f);  /* 0.01A 단위(절댓값) */
        /* [추가] 부호 있는 전류 (방전 -). 반올림 후 INT16 범위로 제한 */
        {
            float c100 = d->current * 100.0f;
            c100 += (c100 < 0.0f) ? -0.5f : 0.5f;
            if (c100 >  32767.0f) c100 =  32767.0f;
            if (c100 < -32768.0f) c100 = -32768.0f;
            g_juntek_elecAttrs.dcCurrent = (s16)c100;
        }
    }
    g_juntek_elecAttrs.activePower      = (s16)(d->power);               /* 1W 단위 */

    /* EP2: Temperature — 0.01°C 단위 */
    g_juntek_tempAttrs.measuredValue    = (s16)(d->temperature * 100.0f);

    /* EP3: Relay State — 1=충전(true), 0=방전(false)
     * [수정] PC3는 EP7(충전전류)이 직접 소유하므로 여기서는
     * 더 이상 물리 릴레이를 구동하지 않고, BMS 파싱 데이터를
     * 그대로 보고하는 순수 소프트웨어 속성으로만 사용 */
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

    /* ZCL 리포팅 — 5초마다 코디네이터로 직접 전송
     * [수정] JUNTEK_MANUAL_REPORT 로 끌 수 있게 하고, 미가입 상태에서는 보내지 않음 */
#if JUNTEK_MANUAL_REPORT
    if (zb_isDeviceJoinedNwk()) {
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
                       ZCL_DATA_TYPE_UINT16,   /* [수정] 속성 테이블(UINT16)과 타입 일치 */
                       (u8*)&g_juntek_elecAttrs.measuredVoltage);

            /* EP1: Current */
            zcl_report(JUNTEK_ENDPOINT_ELEC, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_RMS_CURRENT,
                       ZCL_DATA_TYPE_UINT16,   /* [수정] 속성 테이블(UINT16)과 타입 일치 */
                       (u8*)&g_juntek_elecAttrs.measuredCurrent);

            /* EP1: DC Current (부호 있음) [추가] */
            zcl_report(JUNTEK_ENDPOINT_ELEC, &dstEp, TRUE,
                       ZCL_FRAME_SERVER_CLIENT_DIR, zcl_seqNum++,
                       MANUFACTURER_CODE_NONE,
                       ZCL_CLUSTER_MS_ELECTRICAL_MEASUREMENT,
                       ZCL_ATTRID_ELECTRICAL_MEAS_DC_CURRENT,
                       ZCL_DATA_TYPE_INT16,
                       (u8*)&g_juntek_elecAttrs.dcCurrent);

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
#endif
}
