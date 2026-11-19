/**
 * Zigbee2MQTT External Converter  v1.3.0
 * Device  : JUNTEK BMS Monitor (ZT3L / TLSR8258)
 * Model   : JUNTEK-ZT3LB
 * Vendor  : DODO
 *
 * EP 구성:
 *   EP1 (0x0B04) haElectricalMeasurement — Voltage / Current / Power
 *   EP1 (0xFF00) 필터 설정 (float 10개, R/W, 장치 NV 에 저장)
 *   EP1 (0x0000) genBasic 속성 0xFF00 — 소프트 리셋 (UINT8, write 1 → sys_reboot)
 *   EP1 (0xFF00) RS485 통신 상태 (읽기/리포트 전용, 펌웨어 v1.3 이상)
 *       0xFF10 rs485_status  ENUM8  : 0=정상 / 1=수신 없음(타임아웃) / 2=파싱 오류 반복
 *       0xFF11 rs485_rx_age  UINT16 : 마지막 정상 프레임 이후 경과 초 (최대 65535)
 *       0xFF12 rs485_err_cnt UINT16 : 부팅 후 누적 파싱 오류 수
 *   EP2 (0x0402) msTemperatureMeasurement — Temperature
 *   EP3 (0x000F) genBinaryInput           — Relay State (충전/방전, BMS 파싱 데이터 보고용)
 *   EP4 (0x0702) seMetering               — Remain Ah
 *   EP5 (0x000C) genAnalogInput           — Elapsed Minutes
 *   EP6 (0x0006) genOnOff                 — 주행충전 ON/OFF (GPIO_PD2)
 *   EP7 (0x0006) genOnOff                 — 충전전류 Full/Half (GPIO_PC3, on=Half/off=Full)
 *
 * ZCL 단위 변환:
 *   Voltage  : UINT16, ÷100 → V
 *   Current  : dcCurrent INT16 ÷100 → A (부호 있음, 새 펌웨어)
 *              없으면 rmsCurrent UINT16 절댓값 + Power 부호/Relay 상태로 복원 (구 펌웨어)
 *   Power    : INT16,  ÷1   → W  (방전 시 음수)
 *   Temp     : INT16,  ÷100 → °C
 *   RemainAh : UINT48 LSB   → mAh → ÷1000 = Ah
 *   ElapsMin : SINGLE float → 분
 *
 * v1.1.0 변경 (Z2M 2.x 대응)
 *   - meta.logger 제거 (2.x 에서 없음 → 온도/릴레이/잔량/경과/EP6·EP7 변환이 모두 예외로 실패하던 문제)
 *   - 필터 설정 write: Buffer 대신 숫자 전달 (Buffer 를 float 로 보내면 NaN 이 써졌음)
 *   - 필터 설정 read 응답: 클러스터 이름 '65280' 으로 매칭 (기존 'genBasic' 으로는 수신 불가)
 *   - EP6/EP7 명령을 엔드포인트 6/7 로 직접 전송 (기존: 기본 EP1 로 갈 수 있었음)
 *   - 전류 부호 복원 (Charge + / Discharge −)
 *   - exposes 를 표준 빌더(e.*)로 작성, configure 에서 필터 설정값 읽기
 *
 * v1.2.0 변경
 *   - 소프트 리셋 버튼 추가 (soft_reset → HA button 엔티티)
 *     EP1 genBasic 0xFF00 에 1 쓰기 → 펌웨어가 LED 3회 깜빡인 뒤 0.7초 후 재부팅
 *
 * v1.3.0 변경
 *   - RS485 통신 상태 추가 (rs485_status / rs485_rx_age / rs485_err_cnt)
 *     구 펌웨어(속성 없음)에서는 읽기만 조용히 실패하고 나머지 기능은 그대로 동작
 *
 * 설치 방법:
 *   1. 이 파일을 Z2M data/external_converters/ 에 복사
 *   2. Z2M 재시작 → 장치 페이지에서 Reconfigure 1회
 */

'use strict';

const exposes = require('zigbee-herdsman-converters/lib/exposes');
const {logger} = require('zigbee-herdsman-converters/lib/logger');

const e = exposes.presets;
const ea = exposes.access;
const NS = 'zhc:juntek';

// ─────────────────────────────────────────────
// 필터 설정 상수
// ─────────────────────────────────────────────
const FILTER_CLUSTER = 0xFF00;
const FILTER_CLUSTER_NAME = String(FILTER_CLUSTER);   // herdsman 은 미정의 클러스터를 '65280' 으로 표시
const ZCL_TYPE_SINGLE = 0x39;
const FILTER_ATTRS = [
    { id: 0xFF00, name: 'filter_volt_min',  label: 'Filter voltage min',      unit: 'V',    min: 0,    max: 20,   step: 0.1 },
    { id: 0xFF01, name: 'filter_volt_max',  label: 'Filter voltage max',      unit: 'V',    min: 0,    max: 20,   step: 0.1 },
    { id: 0xFF02, name: 'filter_curr_min',  label: 'Filter current min',      unit: 'A',    min: -300, max: 0,    step: 1   },
    { id: 0xFF03, name: 'filter_curr_max',  label: 'Filter current max',      unit: 'A',    min: 0,    max: 300,  step: 1   },
    { id: 0xFF04, name: 'filter_temp_min',  label: 'Filter temperature min',  unit: '°C',   min: -40,  max: 0,    step: 1   },
    { id: 0xFF05, name: 'filter_temp_max',  label: 'Filter temperature max',  unit: '°C',   min: 0,    max: 100,  step: 1   },
    { id: 0xFF06, name: 'filter_ah_max',    label: 'Filter remain Ah max',    unit: 'Ah',   min: 0,    max: 1000, step: 1   },
    { id: 0xFF07, name: 'filter_volt_rate', label: 'Filter voltage rate',     unit: 'V/s',  min: 0.1,  max: 10,   step: 0.1 },
    { id: 0xFF08, name: 'filter_curr_rate', label: 'Filter current rate',     unit: 'A/s',  min: 1,    max: 200,  step: 1   },
    { id: 0xFF09, name: 'filter_temp_rate', label: 'Filter temperature rate', unit: '°C/s', min: 0.1,  max: 20,   step: 0.1 },
];

const RELAY_EP = { drvchg_state: 6, chgcur_state: 7 };

// RS485 통신 상태 (EP1 0xFF00 클러스터, 읽기/리포트 전용)
const RS485_ATTR = { status: 0xFF10, age: 0xFF11, err: 0xFF12 };
const RS485_STATUS = ['ok', 'timeout', 'parse_error'];   // 펌웨어 ENUM8 값 0/1/2

// 소프트 리셋: genBasic 클러스터의 제조사 속성 (필터 클러스터 0xFF00 과는 다른 클러스터)
const SOFT_RESET_ATTR = 0xFF00;
const ZCL_TYPE_UINT8 = 0x20;

const round = (v, d) => Math.round(v * 10 ** d) / 10 ** d;

// ─────────────────────────────────────────────
// fromZigbee converters
// ─────────────────────────────────────────────

/** EP1: Electrical Measurement → voltage / current / power */
// dcCurrent 를 보낸 적 있는 장치 (이후 rmsCurrent 절댓값으로 덮어쓰지 않음)
const dcCurrentDevices = new Set();

const fz_elec = {
    cluster: 'haElectricalMeasurement',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        const result = {};
        const d = msg.data;

        if (d.rmsVoltage !== undefined) {
            result.voltage = d.rmsVoltage / 100.0;
        }
        if (d.activePower !== undefined) {
            let raw = d.activePower;
            if (raw > 0x7FFF) raw -= 0x10000;
            result.power = raw;
        }
        // [추가] 새 펌웨어: dcCurrent(INT16) 에 부호가 들어 있음 → 그대로 사용 (가장 정확)
        if (d.dcCurrent !== undefined) {
            let raw = d.dcCurrent;
            if (raw > 0x7FFF) raw -= 0x10000;
            result.current = raw / 100.0;
            if (meta.device?.ieeeAddr) dcCurrentDevices.add(meta.device.ieeeAddr);
        } else if (d.rmsCurrent !== undefined && !dcCurrentDevices.has(meta.device?.ieeeAddr)) {
            // 구 펌웨어(dcCurrent 없음)일 때만 rmsCurrent 로 전류 계산
            let raw = d.rmsCurrent;
            if (raw > 0x7FFF) raw -= 0x10000;      // 구 펌웨어(INT16 음수) 호환
            let cur = Math.abs(raw) / 100.0;
            // 펌웨어는 절댓값 전송 → 같은 프레임의 power 부호, 없으면 마지막 충/방전 상태로 부호 결정
            const power = result.power !== undefined ? result.power : meta.state?.power;
            const discharging = (power !== undefined && power !== 0)
                ? power < 0
                : meta.state?.relay_state === 'discharging';
            if (raw < 0 || discharging) cur = -cur;
            result.current = cur;
        }
        return Object.keys(result).length ? result : undefined;
    },
};

/** EP2: Temperature Measurement → temperature */
const fz_temp = {
    cluster: 'msTemperatureMeasurement',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        logger.debug(`fz_temp EP=${msg.endpoint.ID} data=${JSON.stringify(msg.data)}`, NS);
        if (msg.data.measuredValue !== undefined) {
            let raw = msg.data.measuredValue;
            if (raw > 0x7FFF) raw -= 0x10000;
            return { temperature: raw / 100.0 };
        }
    },
};

/** EP3: Binary Input → relay_state */
const fz_relay = {
    cluster: 'genBinaryInput',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        if (msg.data.presentValue !== undefined) {
            const v = !!msg.data.presentValue;
            return {
                relay_state: v ? 'charging' : 'discharging',
                battery_charging: v,
            };
        }
    },
};

/** EP4: Simple Metering → remain_ah */
const fz_metering = {
    cluster: 'seMetering',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        const v = msg.data.currentSummDelivered;
        if (v === undefined) return;
        // herdsman 버전에 따라 UINT48 이 숫자 또는 [high32, low32] 배열로 옴
        const raw = Array.isArray(v) ? v[0] * 0x100000000 + v[1] : Number(v);
        return { remain_ah: round(raw / 1000.0, 3) };
    },
};

/** EP5: Analog Input → elapsed_min */
const fz_analog = {
    cluster: 'genAnalogInput',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        if (msg.data.presentValue !== undefined) {
            return { elapsed_min: Math.round(msg.data.presentValue) };
        }
    },
};

/** EP6: 주행충전 / EP7: 충전전류 — genOnOff → drvchg_state / chgcur_state */
const fz_relay_onoff = {
    cluster: 'genOnOff',
    type: ['attributeReport', 'readResponse'],
    convert(model, msg, publish, options, meta) {
        if (msg.data.onOff === undefined) return;
        const v = msg.data.onOff ? 'ON' : 'OFF';
        if (msg.endpoint.ID === 6) return { drvchg_state: v };
        if (msg.endpoint.ID === 7) return { chgcur_state: v };
    },
};

/** EP1 0xFF00: 필터 설정 read 응답 / 리포트 */
const fz_filter = {
    cluster: FILTER_CLUSTER_NAME,
    type: ['readResponse', 'attributeReport'],
    convert(model, msg, publish, options, meta) {
        const result = {};
        for (const a of FILTER_ATTRS) {
            const v = msg.data[a.id];
            if (typeof v === 'number' && Number.isFinite(v)) {
                result[a.name] = round(v, 2);
            }
        }
        return Object.keys(result).length ? result : undefined;
    },
};

/** EP1 0xFF00: RS485 통신 상태 read 응답 / 리포트 */
const fz_rs485 = {
    cluster: FILTER_CLUSTER_NAME,
    type: ['readResponse', 'attributeReport'],
    convert(model, msg, publish, options, meta) {
        const d = msg.data;
        const result = {};
        const st = d[RS485_ATTR.status];
        if (typeof st === 'number') result.rs485_status = RS485_STATUS[st] ?? `unknown_${st}`;
        const age = d[RS485_ATTR.age];
        if (typeof age === 'number') result.rs485_rx_age = age;
        const err = d[RS485_ATTR.err];
        if (typeof err === 'number') result.rs485_err_cnt = err;
        return Object.keys(result).length ? result : undefined;
    },
};

// ─────────────────────────────────────────────
// toZigbee converters
// ─────────────────────────────────────────────

/** 필터 설정 write/read (EP1) */
const tz_filter = {
    key: FILTER_ATTRS.map((a) => a.name),
    convertSet: async (entity, key, value, meta) => {
        const a = FILTER_ATTRS.find((x) => x.name === key);
        const v = Number(value);
        if (!Number.isFinite(v)) throw new Error(`${key}: 숫자가 아님 (${value})`);
        if (v < a.min || v > a.max) throw new Error(`${key}: ${a.min}~${a.max} 범위를 벗어남 (${v})`);
        const ep = meta.device.getEndpoint(1);
        // herdsman 이 type 0x39 를 IEEE754 float 로 직렬화하므로 숫자를 그대로 전달
        await ep.write(FILTER_CLUSTER, { [a.id]: { value: v, type: ZCL_TYPE_SINGLE } });
        // 펌웨어가 잘못된 조합(min>=max 등)을 되돌릴 수 있으므로 다시 읽어 실제 값 반영
        await ep.read(FILTER_CLUSTER, [a.id]).catch(() => {});
        return { state: { [key]: v } };
    },
    convertGet: async (entity, key, meta) => {
        const a = FILTER_ATTRS.find((x) => x.name === key);
        await meta.device.getEndpoint(1).read(FILTER_CLUSTER, [a.id]);
    },
};

/** EP6(주행충전) / EP7(충전전류) ON/OFF — key 로 엔드포인트를 직접 선택 */
const tz_relay_onoff = {
    key: Object.keys(RELAY_EP),
    convertSet: async (entity, key, value, meta) => {
        const ep = meta.device.getEndpoint(RELAY_EP[key]);
        const v = String(value).toUpperCase();
        if (!['ON', 'OFF', 'TOGGLE'].includes(v)) throw new Error(`${key}: ON/OFF/TOGGLE 만 가능 (${value})`);
        await ep.command('genOnOff', v.toLowerCase(), {}, { disableDefaultResponse: true });
        if (v === 'TOGGLE') {
            const cur = meta.state?.[key];
            return cur ? { state: { [key]: cur === 'ON' ? 'OFF' : 'ON' } } : undefined;
        }
        return { state: { [key]: v } };
    },
    convertGet: async (entity, key, meta) => {
        await meta.device.getEndpoint(RELAY_EP[key]).read('genOnOff', ['onOff']);
    },
};

/** RS485 상태 읽기 (HA/Z2M 에서 새로고침) */
const tz_rs485 = {
    key: ['rs485_status', 'rs485_rx_age', 'rs485_err_cnt'],
    convertGet: async (entity, key, meta) => {
        await meta.device.getEndpoint(1).read(FILTER_CLUSTER, Object.values(RS485_ATTR));
    },
};

/** 소프트 리셋 (EP1 genBasic 0xFF00 = 1) */
const tz_soft_reset = {
    key: ['soft_reset'],
    convertSet: async (entity, key, value, meta) => {
        const ep = meta.device.getEndpoint(1);
        // 펌웨어가 0.7초 뒤 재부팅하므로 응답이 오기 전에 끊길 수 있음 → 기본 응답 생략, 타임아웃은 무시
        await ep.write('genBasic', { [SOFT_RESET_ATTR]: { value: 1, type: ZCL_TYPE_UINT8 } },
            { disableDefaultResponse: true }).catch((err) => {
            logger.warning(`soft_reset: ${err.message} (재부팅 중이면 정상)`, NS);
        });
        logger.info('soft_reset: 재부팅 명령 전송', NS);
        return {};
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
    ota: true,   // Z2M 장치 화면 OTA 탭에서 업데이트 확인/설치

    fromZigbee: [fz_elec, fz_temp, fz_relay, fz_metering, fz_analog, fz_relay_onoff, fz_filter, fz_rs485],
    toZigbee: [tz_filter, tz_relay_onoff, tz_soft_reset, tz_rs485],

    exposes: [
        e.numeric('voltage', ea.STATE).withUnit('V').withDescription('Battery voltage'),
        e.numeric('current', ea.STATE).withUnit('A').withDescription('Charge(+) / Discharge(-) current'),
        e.numeric('power', ea.STATE).withUnit('W').withDescription('Instantaneous power (discharge negative)'),
        e.numeric('temperature', ea.STATE).withUnit('°C').withDescription('BMS temperature'),
        e.enum('relay_state', ea.STATE, ['charging', 'discharging']).withDescription('BMS relay state'),
        e.binary('battery_charging', ea.STATE, true, false).withDescription('Battery is charging'),
        e.numeric('remain_ah', ea.STATE).withUnit('Ah').withLabel('Remaining capacity'),
        e.numeric('elapsed_min', ea.STATE).withUnit('min').withLabel('Elapsed minutes'),
        e.binary('drvchg_state', ea.ALL, 'ON', 'OFF')
            .withLabel('Driving charge').withDescription('주행충전 ON/OFF (EP6, GPIO_PD2)'),
        e.binary('chgcur_state', ea.ALL, 'ON', 'OFF')
            .withLabel('Charge current (half)').withDescription('충전전류 Half(ON)/Full(OFF) (EP7, GPIO_PC3)'),
        // RS485 통신 상태 (BMS ↔ ZT3L)
        e.enum('rs485_status', ea.STATE_GET, RS485_STATUS)
            .withLabel('RS485 status').withDescription('BMS RS485 통신 상태: ok / timeout(수신 없음) / parse_error(데이터 오류 반복)'),
        e.numeric('rs485_rx_age', ea.STATE_GET).withUnit('s')
            .withLabel('RS485 last frame age').withDescription('마지막 정상 BMS 프레임 이후 경과 시간')
            .withCategory('diagnostic'),
        e.numeric('rs485_err_cnt', ea.STATE_GET)
            .withLabel('RS485 parse errors').withDescription('부팅 후 누적 파싱 오류 수')
            .withCategory('diagnostic'),
        // 값 1개 + SET 전용 enum → Z2M 이 HA 에 button 엔티티로 생성 (button.bmsz_soft_reset)
        e.enum('soft_reset', ea.SET, ['reset'])
            .withLabel('Soft reset').withDescription('BMSZ 재부팅 (소프트 리셋)')
            .withCategory('config'),
        ...FILTER_ATTRS.map((a) =>
            e.numeric(a.name, ea.ALL)
                .withLabel(a.label).withUnit(a.unit)
                .withValueMin(a.min).withValueMax(a.max).withValueStep(a.step)
                .withDescription(`BMS filter setting: ${a.label}`)
                .withCategory('config')),
    ],

    configure: async (device, coordinatorEndpoint, definition) => {
        const ep1 = device.getEndpoint(1);
        await ep1.read('haElectricalMeasurement', [
            'acVoltageDivisor', 'acVoltageMultiplier',
            'acCurrentDivisor', 'acCurrentMultiplier',
            'acPowerDivisor',   'acPowerMultiplier',
        ]).catch(() => {});

        const binds = [
            [1, 'haElectricalMeasurement', [
                { attribute: 'rmsVoltage',  minimumReportInterval: 5, maximumReportInterval: 300, reportableChange: 1 },
                { attribute: 'rmsCurrent',  minimumReportInterval: 5, maximumReportInterval: 300, reportableChange: 1 },
                { attribute: 'activePower', minimumReportInterval: 5, maximumReportInterval: 300, reportableChange: 1 },
            ]],
            // [추가] 부호 있는 DC 전류 (새 펌웨어). 따로 설정 → 구 펌웨어에서 실패해도 위 항목에 영향 없음
            [1, 'haElectricalMeasurement', [
                { attribute: 'dcCurrent',   minimumReportInterval: 5, maximumReportInterval: 300, reportableChange: 1 },
            ]],
            [2, 'msTemperatureMeasurement', [
                { attribute: 'measuredValue', minimumReportInterval: 10, maximumReportInterval: 300, reportableChange: 10 },
            ]],
            [3, 'genBinaryInput', [
                { attribute: 'presentValue', minimumReportInterval: 1, maximumReportInterval: 300, reportableChange: 0 },
            ]],
            [4, 'seMetering', [
                { attribute: 'currentSummDelivered', minimumReportInterval: 10, maximumReportInterval: 600, reportableChange: 1 },
            ]],
            [5, 'genAnalogInput', [
                { attribute: 'presentValue', minimumReportInterval: 30, maximumReportInterval: 600, reportableChange: 1 },
            ]],
            [6, 'genOnOff', [
                { attribute: 'onOff', minimumReportInterval: 1, maximumReportInterval: 300, reportableChange: 0 },
            ]],
            [7, 'genOnOff', [
                { attribute: 'onOff', minimumReportInterval: 1, maximumReportInterval: 300, reportableChange: 0 },
            ]],
        ];
        for (const [id, cluster, items] of binds) {
            const ep = device.getEndpoint(id);
            if (!ep) continue;
            try {
                await ep.bind(cluster, coordinatorEndpoint);
                await ep.configureReporting(cluster, items);
            } catch (err) {
                logger.warning(`configure EP${id} ${cluster}: ${err.message}`, NS);
            }
        }

        // 현재 필터 설정값 / 릴레이 상태 읽기 (HA 에 바로 표시)
        await ep1.read(FILTER_CLUSTER, FILTER_ATTRS.slice(0, 5).map((a) => a.id)).catch(() => {});
        await ep1.read(FILTER_CLUSTER, FILTER_ATTRS.slice(5).map((a) => a.id)).catch(() => {});
        for (const id of [6, 7]) {
            await device.getEndpoint(id)?.read('genOnOff', ['onOff']).catch(() => {});
        }
        // RS485 상태 (구 펌웨어는 UNSUPPORTED_ATTRIBUTE → 무시)
        await ep1.read(FILTER_CLUSTER, Object.values(RS485_ATTR)).catch(() => {});
    },
};

module.exports = definition;