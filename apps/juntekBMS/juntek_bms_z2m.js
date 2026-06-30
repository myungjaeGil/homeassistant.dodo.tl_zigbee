/**
 * Zigbee2MQTT External Converter
 * Device  : JUNTEK BMS Monitor (ZT3L / TLSR8258)
 * Model   : JUNTEK-ZT3LB
 * Vendor  : DODO
 *
 * EP 구성:
 *   EP1 (0x0B04) haElectricalMeasurement — Voltage / Current / Power
 *   EP2 (0x0402) msTemperatureMeasurement — Temperature
 *   EP3 (0x000F) genBinaryInput           — Relay State (충전/방전)
 *   EP4 (0x0702) seMetering               — Remain Ah
 *   EP5 (0x000C) genAnalogInput           — Elapsed Minutes
 *
 * ZCL 단위 변환:
 *   Voltage  : INT16, ÷100  → V   (acVoltageDivisor=100)
 *   Current  : INT16, ÷100  → A   (acCurrentDivisor=100, 방전시 음수)
 *   Power    : INT16, ÷1    → W   (acPowerDivisor=1)
 *   Temp     : INT16, ÷100  → °C
 *   RemainAh : UINT48 LSB   → mAh → ÷1000 = Ah
 *   ElapsMin : SINGLE float → 분
 *
 * 설치 방법:
 *   1. 이 파일을 Z2M data 디렉토리에 복사
 *      예) /opt/zigbee2mqtt/data/juntek_bms.js
 *   2. configuration.yaml 에 추가:
 *      external_converters:
 *        - juntek_bms.js
 *   3. Z2M 재시작
 */

'use strict';

const {
    electricityMeter,
    temperature,
} = require('zigbee-herdsman-converters/lib/modernExtend');

// ─────────────────────────────────────────────
// fromZigbee converters
// ─────────────────────────────────────────────

/** EP1: Electrical Measurement → voltage / current / power */
const fz_elec = {
    cluster: 'haElectricalMeasurement',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        const result = {};

        if (msg.data.hasOwnProperty('rmsVoltage')) {
            result.voltage = msg.data.rmsVoltage / 100.0;
        }
        if (msg.data.hasOwnProperty('rmsCurrent')) {
            let raw = msg.data.rmsCurrent;
            if (raw > 0x7FFF) raw -= 0x10000;
            result.current = raw / 100.0;
        }
        if (msg.data.hasOwnProperty('activePower')) {
            let raw = msg.data.activePower;
            if (raw > 0x7FFF) raw -= 0x10000;
            result.power = raw;
        }
        return Object.keys(result).length ? result : undefined;
    },
};

/** EP2: Temperature Measurement → temperature */
const fz_temp = {
    cluster: 'msTemperatureMeasurement',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        meta.logger.debug(`[JUNTEK] fz_temp EP=${msg.endpoint.ID} data=${JSON.stringify(msg.data)}`);
        if (msg.data.hasOwnProperty('measuredValue')) {
            return { temperature: msg.data.measuredValue / 100.0 };
        }
    },
};

/** EP3: Binary Input → relay_state */
const fz_relay = {
    cluster: 'genBinaryInput',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        meta.logger.debug(`[JUNTEK] fz_relay EP=${msg.endpoint.ID} data=${JSON.stringify(msg.data)}`);
        if (msg.data.hasOwnProperty('presentValue')) {
            const v = msg.data.presentValue;
            return {
                relay_state: v ? 'charging' : 'discharging',
                battery_charging: v ? true : false,
            };
        }
    },
};

/** EP4: Simple Metering → remain_ah */
const fz_metering = {
    cluster: 'seMetering',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        meta.logger.debug(`[JUNTEK] fz_metering EP=${msg.endpoint.ID} data=${JSON.stringify(msg.data)}`);
        if (msg.data.hasOwnProperty('currentSummDelivered')) {
            const raw = Number(msg.data.currentSummDelivered);
            return { remain_ah: raw / 1000.0 };
        }
    },
};

/** EP5: Analog Input → elapsed_min */
const fz_analog = {
    cluster: 'genAnalogInput',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        meta.logger.debug(`[JUNTEK] fz_analog EP=${msg.endpoint.ID} data=${JSON.stringify(msg.data)}`);
        if (msg.data.hasOwnProperty('presentValue')) {
            return { elapsed_min: Math.round(msg.data.presentValue) };
        }
    },
};

// ─────────────────────────────────────────────
// 필터 설정 상수
// ─────────────────────────────────────────────
const FILTER_CLUSTER = 0xFF00;
const FILTER_ATTRS = {
    volt_min:  { id: 0xFF00, name: 'filter_volt_min',  label: 'Filter voltage min',      unit: 'V',       min: 0,    max: 20,   step: 0.1 },
    volt_max:  { id: 0xFF01, name: 'filter_volt_max',  label: 'Filter voltage max',      unit: 'V',       min: 0,    max: 20,   step: 0.1 },
    curr_min:  { id: 0xFF02, name: 'filter_curr_min',  label: 'Filter current min',      unit: 'A',       min: -300, max: 0,    step: 1   },
    curr_max:  { id: 0xFF03, name: 'filter_curr_max',  label: 'Filter current max',      unit: 'A',       min: 0,    max: 300,  step: 1   },
    temp_min:  { id: 0xFF04, name: 'filter_temp_min',  label: 'Filter temperature min',  unit: '°C',      min: -40,  max: 0,    step: 1   },
    temp_max:  { id: 0xFF05, name: 'filter_temp_max',  label: 'Filter temperature max',  unit: '°C',      min: 0,    max: 100,  step: 1   },
    ah_max:    { id: 0xFF06, name: 'filter_ah_max',    label: 'Filter remain Ah max',    unit: 'Ah',      min: 0,    max: 1000, step: 1   },
    volt_rate: { id: 0xFF07, name: 'filter_volt_rate', label: 'Filter voltage rate',     unit: 'V/s',     min: 0.1,  max: 10,   step: 0.1 },
    curr_rate: { id: 0xFF08, name: 'filter_curr_rate', label: 'Filter current rate',     unit: 'A/s',     min: 1,    max: 200,  step: 1   },
    temp_rate: { id: 0xFF09, name: 'filter_temp_rate', label: 'Filter temperature rate', unit: '°C/s',    min: 0.1,  max: 20,   step: 0.1 },
};

// ─────────────────────────────────────────────
// fromZigbee: 필터 설정 read
// ─────────────────────────────────────────────
const fz_filter = {
    cluster: 'genBasic',   // 실제론 0xFF00이지만 Z2M은 cluster 이름으로 매칭
    type: ['readResponse'],
    convert(model, msg, publish, options, meta) {
        // 직접 raw cluster 0xFF00 readResponse 처리
        if (msg.endpoint.ID !== 1) return;
        const result = {};
        for (const [key, attr] of Object.entries(FILTER_ATTRS)) {
            if (msg.data.hasOwnProperty(attr.id)) {
                result[attr.name] = msg.data[attr.id];
            }
        }
        return Object.keys(result).length ? result : undefined;
    },
};

// ─────────────────────────────────────────────
// toZigbee: 필터 설정 write
// ─────────────────────────────────────────────
const tz_filter = {
    key: Object.values(FILTER_ATTRS).map(a => a.name),
    convertSet: async (entity, key, value, meta) => {
        const attr = Object.values(FILTER_ATTRS).find(a => a.name === key);
        if (!attr) return;
        // float를 IEEE 754 little-endian 4바이트로 변환
        const buf = Buffer.allocUnsafe(4);
        buf.writeFloatLE(parseFloat(value), 0);
        await entity.write(FILTER_CLUSTER, { [attr.id]: { value: buf, type: 0x39 } },
                           { disableDefaultResponse: true });
        return { state: { [key]: value } };
    },
    convertGet: async (entity, key, meta) => {
        const attr = Object.values(FILTER_ATTRS).find(a => a.name === key);
        if (!attr) return;
        await entity.read(FILTER_CLUSTER, [attr.id]);
    },
};

// ─────────────────────────────────────────────
// Device definition
// ─────────────────────────────────────────────

const definition = {
    zigbeeModel: ['JUNTEK-ZT3LB'],
    model: 'JUNTEK-ZT3LB',
    vendor: 'DODO',
    description: 'JUNTEK BMS Monitor via ZT3L (TLSR8258)',

    fromZigbee: [fz_elec, fz_temp, fz_relay, fz_metering, fz_analog, fz_filter],
    toZigbee: [tz_filter],

    exposes: [
        /* EP1: Electrical */
        { type:'numeric', name:'voltage',     label:'Voltage',     property:'voltage',     access:1, unit:'V',  description:'Battery voltage',                   device_class:'voltage',     state_class:'measurement' },
        { type:'numeric', name:'current',     label:'Current',     property:'current',     access:1, unit:'A',  description:'Charge(+) / Discharge(-) current',  device_class:'current',     state_class:'measurement' },
        { type:'numeric', name:'power',       label:'Power',       property:'power',       access:1, unit:'W',  description:'Instantaneous power',               device_class:'power',       state_class:'measurement' },
        /* EP2 */
        { type:'numeric', name:'temperature', label:'Temperature', property:'temperature', access:1, unit:'°C', description:'BMS temperature',                   device_class:'temperature', state_class:'measurement' },
        /* EP3 */
        { type:'enum',    name:'relay_state', label:'Relay state', property:'relay_state', access:1, values:['charging','discharging'], description:'BMS relay state' },
        { type:'binary',  name:'battery_charging', label:'Battery charging', property:'battery_charging', access:1, value_on:true, value_off:false, device_class:'battery_charging' },
        /* EP4 */
        { type:'numeric', name:'remain_ah',   label:'Remaining capacity', property:'remain_ah',   access:1, unit:'Ah',  state_class:'measurement' },
        /* EP5 */
        { type:'numeric', name:'elapsed_min', label:'Elapsed minutes',    property:'elapsed_min', access:1, unit:'min', state_class:'measurement' },
        /* 필터 설정 */
        ...Object.values(FILTER_ATTRS).map(a => ({
            type: 'numeric',
            name: a.name,
            label: a.label,
            property: a.name,
            access: 7,   /* STATE_SET */
            unit: a.unit,
            value_min: a.min,
            value_max: a.max,
            value_step: a.step,
            description: `BMS filter setting: ${a.label}`,
        })),
        /* Linkquality */
        { access:1, category:'diagnostic', description:'Link quality (signal strength)', label:'Linkquality', name:'linkquality', property:'linkquality', type:'numeric', unit:'lqi', value_max:255, value_min:0 },
    ],

    configure: async (device, coordinatorEndpoint, logger) => {
        /* EP1: Electrical Measurement — configReport */
        const ep1 = device.getEndpoint(1);
        await ep1.read('haElectricalMeasurement', [
            'acVoltageDivisor', 'acVoltageMultiplier',
            'acCurrentDivisor', 'acCurrentMultiplier',
            'acPowerDivisor',   'acPowerMultiplier',
        ]).catch(() => {});
        await ep1.configureReporting('haElectricalMeasurement', [
            { attribute: 'rmsVoltage',  min: 5,  max: 300, change: 1 },
            { attribute: 'rmsCurrent',  min: 5,  max: 300, change: 1 },
            { attribute: 'activePower', min: 5,  max: 300, change: 1 },
        ]).catch(() => {});

        /* EP2: Temperature */
        const ep2 = device.getEndpoint(2);
        await ep2.configureReporting('msTemperatureMeasurement', [
            { attribute: 'measuredValue', min: 10, max: 300, change: 10 },
        ]).catch(() => {});

        /* EP3: Binary Input */
        const ep3 = device.getEndpoint(3);
        await ep3.configureReporting('genBinaryInput', [
            { attribute: 'presentValue', min: 1, max: 300, change: 1 },
        ]).catch(() => {});

        /* EP4: Metering */
        const ep4 = device.getEndpoint(4);
        await ep4.configureReporting('seMetering', [
            { attribute: 'currentSummDelivered', min: 10, max: 600, change: 1 },
        ]).catch(() => {});

        /* EP5: Analog Input */
        const ep5 = device.getEndpoint(5);
        await ep5.configureReporting('genAnalogInput', [
            { attribute: 'presentValue', min: 30, max: 600, change: 1 },
        ]).catch(() => {});
    },
};

module.exports = definition;