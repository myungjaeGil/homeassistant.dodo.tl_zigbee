// Zigbee2MQTT External Converter — JUNTEK BMS ZT3L
// 파일 위치: /config/zigbee2mqtt/juntek_bms_z2m.js
//
// 소프트 리셋: HA UI 버튼 → ZCL Basic cluster 0xFF00 Write(1)
//   펌웨어가 수신 후 700ms 뒤 sys_reboot() 실행

const {Zcl} = require('zigbee-herdsman');
const ota = require('zigbee-herdsman-converters/lib/ota');
const fz = require('zigbee-herdsman-converters/lib/fromZigbee');
const tz = require('zigbee-herdsman-converters/lib/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const e = exposes.presets;
const ea = exposes.access;

// EP1 Basic 클러스터의 Manufacturer-Specific 속성 0xFF00
const SOFT_RESET_ATTR = 0xFF00;
const BASIC_CLUSTER   = 0x0000;

const definition = {
    zigbeeModel: ['JUNTEK-ZT3LB'],
    model:       'JUNTEK-ZT3LB',
    vendor:      'DODO',
    description: 'JUNTEK BMS Monitor (ZT3L)',
    fromZigbee:  [
        // EP1: Electrical Measurement
        {
            cluster: 'haElectricalMeasurement',
            type:    ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                const ep = msg.endpoint.ID;
                if (ep !== 1) return;
                const result = {};
                if (msg.data['rmsVoltage']  !== undefined)
                    result.voltage = (msg.data['rmsVoltage']  / 100).toFixed(2);
                if (msg.data['rmsCurrent']  !== undefined)
                    result.current = (msg.data['rmsCurrent']  / 100).toFixed(2);
                if (msg.data['activePower'] !== undefined)
                    result.power   =  msg.data['activePower'];
                return result;
            },
        },
        // EP2: Temperature
        {
            cluster: 'msTemperatureMeasurement',
            type:    ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                if (msg.endpoint.ID !== 2) return;
                if (msg.data['measuredValue'] !== undefined)
                    return { temperature: (msg.data['measuredValue'] / 100).toFixed(1) };
            },
        },
        // EP3: Binary Input (Relay — 충전/방전)
        {
            cluster: 'genBinaryInput',
            type:    ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                if (msg.endpoint.ID !== 3) return;
                if (msg.data['presentValue'] !== undefined)
                    return { relay_state: msg.data['presentValue'] ? 'charging' : 'discharging' };
            },
        },
        // EP4: Simple Metering (Remain Ah)
        {
            cluster: 'seMetering',
            type:    ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                if (msg.endpoint.ID !== 4) return;
                if (msg.data['currentSummDelivered'] !== undefined) {
                    const mah = parseInt(msg.data['currentSummDelivered']);
                    return { remain_ah: (mah / 1000).toFixed(3) };
                }
            },
        },
        // EP5: Analog Input (Elapsed Minutes)
        {
            cluster: 'genAnalogInput',
            type:    ['attributeReport', 'readResponse'],
            convert: (model, msg, publish, options, meta) => {
                if (msg.endpoint.ID !== 5) return;
                if (msg.data['presentValue'] !== undefined)
                    return { elapsed_min: Math.round(msg.data['presentValue']) };
            },
        },
        // EP6/EP7: OnOff (주행충전 / 충전전류) — 표준 컨버터, multiEndpoint로 자동 분기
        fz.on_off,
    ],
    toZigbee: [
        // EP6/EP7: OnOff (주행충전 / 충전전류)
        tz.on_off,
        // HA UI → 소프트 리셋 버튼
        {
            key: ['soft_reset'],
            convertSet: async (entity, key, value, meta) => {
                // EP1 Basic 클러스터 0xFF00 에 1 Write
                await entity.write(
                    BASIC_CLUSTER,
                    { [SOFT_RESET_ATTR]: { value: 1, type: Zcl.DataType.uint8 } },
                    { manufacturerCode: null, disableDefaultResponse: false }
                );
                return { state: { soft_reset: false } };   // 즉시 false 로 리셋 (버튼 UI)
            },
        },
    ],
    exposes: [
        // 측정값
        require('zigbee-herdsman-converters/lib/exposes')
            .numeric('voltage',     require('zigbee-herdsman-converters/lib/exposes').access.STATE)
            .withUnit('V').withDescription('배터리 전압'),
        require('zigbee-herdsman-converters/lib/exposes')
            .numeric('current',     require('zigbee-herdsman-converters/lib/exposes').access.STATE)
            .withUnit('A').withDescription('전류 (음수=방전)'),
        require('zigbee-herdsman-converters/lib/exposes')
            .numeric('power',       require('zigbee-herdsman-converters/lib/exposes').access.STATE)
            .withUnit('W').withDescription('전력'),
        require('zigbee-herdsman-converters/lib/exposes')
            .numeric('temperature', require('zigbee-herdsman-converters/lib/exposes').access.STATE)
            .withUnit('°C').withDescription('BMS 온도'),
        require('zigbee-herdsman-converters/lib/exposes')
            .text('relay_state',    require('zigbee-herdsman-converters/lib/exposes').access.STATE)
            .withDescription('충전/방전 상태 (charging / discharging)'),
        require('zigbee-herdsman-converters/lib/exposes')
            .numeric('remain_ah',   require('zigbee-herdsman-converters/lib/exposes').access.STATE)
            .withUnit('Ah').withDescription('잔여 용량'),
        require('zigbee-herdsman-converters/lib/exposes')
            .numeric('elapsed_min', require('zigbee-herdsman-converters/lib/exposes').access.STATE)
            .withUnit('min').withDescription('경과 시간'),
        // 신규 채널: 주행충전 ON/OFF, 충전전류 Full/Half
        e.switch().withEndpoint('ep6_drvchg').withDescription('주행충전 ON/OFF'),
        e.switch().withEndpoint('ep7_chgcur').withDescription('충전전류 Half(on)/Full(off)'),
        // 소프트 리셋 버튼
        require('zigbee-herdsman-converters/lib/exposes')
            .binary('soft_reset',   require('zigbee-herdsman-converters/lib/exposes').access.SET,
                    true, false)
            .withDescription('소프트 리셋 (펌웨어 재시작)'),
    ],
    endpoint: (device) => {
        return {
            default:      1,
            ep1_elec:     1,
            ep2_temp:     2,
            ep3_relay:    3,
            ep4_metering: 4,
            ep5_analog:   5,
            ep6_drvchg:   6,
            ep7_chgcur:   7,
        };
    },
    meta: { multiEndpoint: true },
    ota: ota.zigbeeOTA,
    configure: async (device, coordinatorEndpoint, logger) => {
        const ep1 = device.getEndpoint(1);
        const ep2 = device.getEndpoint(2);
        const ep3 = device.getEndpoint(3);
        const ep4 = device.getEndpoint(4);
        const ep5 = device.getEndpoint(5);
        const ep6 = device.getEndpoint(6);
        const ep7 = device.getEndpoint(7);

        // EP1 바인드
        await ep1.bind('haElectricalMeasurement', coordinatorEndpoint);
        await ep1.configureReporting('haElectricalMeasurement', [
            { attribute: 'rmsVoltage',  min: 5,  max: 300, change: 1 },
            { attribute: 'rmsCurrent',  min: 5,  max: 300, change: 1 },
            { attribute: 'activePower', min: 5,  max: 300, change: 1 },
        ]);
        // EP2 바인드
        await ep2.bind('msTemperatureMeasurement', coordinatorEndpoint);
        await ep2.configureReporting('msTemperatureMeasurement', [
            { attribute: 'measuredValue', min: 5, max: 300, change: 10 },
        ]);
        // EP3 바인드
        await ep3.bind('genBinaryInput', coordinatorEndpoint);
        await ep3.configureReporting('genBinaryInput', [
            { attribute: 'presentValue', min: 1, max: 300, change: 1 },
        ]);
        // EP4 바인드
        await ep4.bind('seMetering', coordinatorEndpoint);
        await ep4.configureReporting('seMetering', [
            { attribute: 'currentSummDelivered', min: 10, max: 300, change: 1 },
        ]);
        // EP5 바인드
        await ep5.bind('genAnalogInput', coordinatorEndpoint);
        await ep5.configureReporting('genAnalogInput', [
            { attribute: 'presentValue', min: 30, max: 600, change: 1 },
        ]);
        // EP6 바인드 — 주행충전
        await ep6.bind('genOnOff', coordinatorEndpoint);
        await ep6.configureReporting('genOnOff', [
            { attribute: 'onOff', min: 1, max: 300, change: 0 },
        ]);
        // EP7 바인드 — 충전전류
        await ep7.bind('genOnOff', coordinatorEndpoint);
        await ep7.configureReporting('genOnOff', [
            { attribute: 'onOff', min: 1, max: 300, change: 0 },
        ]);
    },
};

module.exports = definition;