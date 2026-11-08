/**
 * Zigbee2MQTT External Converter
 *
 * Device  : ZT3L-HEATER  (TLSR8258 / Tuya ZT3L) — 무시동히터 리모컨
 * Vendor  : DODO
 * Version : 1.1.0
 *
 * EP 구성:
 *   EP1 : 히터 전원  OnOff  — On = ON 버튼, Off = OFF 버튼 (상태는 이 장치 명령 + 외부 버튼 입력 반영)
 *   EP2 : UP 버튼    OnOff  — On 명령 → UP 1회 누름 (끝나면 자동 Off)
 *   EP3 : DOWN 버튼  OnOff  — On 명령 → DOWN 1회 누름
 *
 * 1.1.0:
 *   - 버튼 라인을 평소 입력으로 감시 → 원래 리모컨/히터 본체 조작도 반영
 *   - level(0~9), last_action, last_source 추가 (펌웨어가 변경 시 리포트)
 *
 * 노출 항목 (HA):
 *   state             스위치  히터 전원
 *   level             센서    현재 레벨 0~9
 *   last_action       센서    마지막 입력 (on / off / up / down / none)
 *   last_source       센서    마지막 입력 출처 (remote = 이 장치 / external = 외부 버튼)
 *   heater_up         버튼    UP 1단계
 *   heater_down       버튼    DOWN 1단계
 *   power_pulse_ms    숫자    ON/OFF 버튼 누름 시간 (50~5000ms)
 *   button_pulse_ms   숫자    UP/DOWN 버튼 누름 시간 (50~5000ms)
 *   button_gap_ms     숫자    연속 명령 사이 간격 (100~3000ms)
 *
 * 펌웨어 전용 속성 (EP1 genOnOff):
 *   0xF000 powerPulseMs / 0xF001 btnPulseMs / 0xF002 gapMs   uint16 R/W
 *   0xF003 level (uint8) / 0xF004 lastAction (enum8) / 0xF005 lastSource (enum8)   R, 리포트
 *
 * 설치 (Z2M 2.x):
 *   data/external_converters/ 폴더에 이 파일을 넣고 Z2M 재시작
 */

'use strict';

const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const e = exposes.presets;
const ea = exposes.access;

/* ─── 로거 — Z2M 2.x 는 lib/logger, 1.x 는 configure 인자 ─── */
let zhcLogger = null;
try {
    zhcLogger = require('zigbee-herdsman-converters/lib/logger').logger;
} catch (err) {
    zhcLogger = null;
}
const NS = 'zhc:ZT3L-HEATER';
function makeLog(argLogger) {
    const l = (argLogger && typeof argLogger.info === 'function') ? argLogger : null;
    return {
        info: (msg) => (zhcLogger ? zhcLogger.info(msg, NS) : l ? l.info(msg) : console.log(msg)),
        warn: (msg) => (zhcLogger ? zhcLogger.warning(msg, NS) : l ? l.warn(msg) : console.warn(msg)),
    };
}

const EP_POWER = 1;
const EP_UP = 2;
const EP_DOWN = 3;

const ZCL_UINT16 = 0x21;
const CFG_ATTRS = {
    power_pulse_ms:  0xF000,
    button_pulse_ms: 0xF001,
    button_gap_ms:   0xF002,
};
const CFG_IDS = Object.values(CFG_ATTRS);

const ATTR_LEVEL = 0xF003;
const ATTR_LAST_ACTION = 0xF004;
const ATTR_LAST_SOURCE = 0xF005;
const STATUS_IDS = [ATTR_LEVEL, ATTR_LAST_ACTION, ATTR_LAST_SOURCE];
const ZCL_UINT8 = 0x20;
const ZCL_ENUM8 = 0x30;

const LAST_ACTION = {0: 'on', 1: 'off', 2: 'up', 3: 'down', 255: 'none'};
const LAST_SOURCE = {0: 'remote', 1: 'external'};

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

/* ─── fromZigbee ────────────────────────────────────────── */

/* EP1 onOff → state (EP2/EP3 순간 버튼 보고는 무시) */
const fzPower = {
    cluster: 'genOnOff',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        if (msg.endpoint.ID !== EP_POWER) return;
        const result = {};
        if (msg.data.onOff !== undefined) {
            result.state = msg.data.onOff ? 'ON' : 'OFF';
        }
        for (const [key, id] of Object.entries(CFG_ATTRS)) {
            if (msg.data[id] !== undefined) result[key] = msg.data[id];
        }
        if (msg.data[ATTR_LEVEL] !== undefined) {
            result.level = msg.data[ATTR_LEVEL];
        }
        if (msg.data[ATTR_LAST_ACTION] !== undefined) {
            result.last_action = LAST_ACTION[msg.data[ATTR_LAST_ACTION]] ?? 'none';
        }
        if (msg.data[ATTR_LAST_SOURCE] !== undefined) {
            result.last_source = LAST_SOURCE[msg.data[ATTR_LAST_SOURCE]] ?? 'remote';
        }
        return result;
    },
};

/* ─── toZigbee ──────────────────────────────────────────── */

const tzPower = {
    key: ['state'],
    convertSet: async (entity, key, value, meta) => {
        const v = String(value).toLowerCase();
        if (!['on', 'off', 'toggle'].includes(v)) {
            throw new Error(`state 는 ON / OFF / TOGGLE 만 가능합니다 (입력: ${value})`);
        }
        const ep = meta.device.getEndpoint(EP_POWER);
        await ep.command('genOnOff', v, {}, {disableDefaultResponse: false});
        if (v === 'toggle') {
            await ep.read('genOnOff', ['onOff']);
            return;
        }
        return {state: {state: v.toUpperCase()}};
    },
    convertGet: async (entity, key, meta) => {
        await meta.device.getEndpoint(EP_POWER).read('genOnOff', ['onOff']);
    },
};

/* UP / DOWN 버튼 — HA 에서 버튼 엔티티로 표시 (값 하나짜리 SET 전용 enum) */
const tzStep = {
    key: ['heater_up', 'heater_down'],
    convertSet: async (entity, key, value, meta) => {
        const epId = key === 'heater_up' ? EP_UP : EP_DOWN;
        /* 펌웨어가 거부하면(히터 꺼짐, 대기열 가득) 실패 응답 → Z2M 에 오류로 표시 */
        await meta.device.getEndpoint(epId).command('genOnOff', 'on', {}, {disableDefaultResponse: false});
    },
};

const tzConfig = {
    key: Object.keys(CFG_ATTRS),
    convertSet: async (entity, key, value, meta) => {
        const v = Math.round(Number(value));
        const ep = meta.device.getEndpoint(EP_POWER);
        await ep.write('genOnOff', {[CFG_ATTRS[key]]: {value: v, type: ZCL_UINT16}});
        /* 펌웨어가 범위를 보정할 수 있으므로 반영 후 다시 읽음 */
        await sleep(600);
        await ep.read('genOnOff', CFG_IDS);
        return {state: {[key]: v}};
    },
    convertGet: async (entity, key, meta) => {
        await meta.device.getEndpoint(EP_POWER).read('genOnOff', CFG_IDS);
    },
};

/* 상태 속성 — 읽기 전용 (새로고침 버튼으로 다시 읽기) */
const tzStatus = {
    key: ['level', 'last_action', 'last_source'],
    convertGet: async (entity, key, meta) => {
        await meta.device.getEndpoint(EP_POWER).read('genOnOff', STATUS_IDS);
    },
};

/* ─── 정의 ──────────────────────────────────────────────── */

const definition = {
    zigbeeModel: ['ZT3L-HEATER'],
    model:       'ZT3L-HEATER',
    vendor:      'DODO',
    description: '무시동히터 리모컨 (ON / OFF / UP / DOWN 버튼 출력)',

    fromZigbee: [fzPower],
    toZigbee:   [tzPower, tzStep, tzConfig, tzStatus],

    exposes: [
        e.switch().withDescription('히터 전원 (이 장치 명령 + 외부 버튼 입력 기준)'),
        e.numeric('level', ea.STATE_GET)
            .withValueMin(0).withValueMax(9)
            .withDescription('현재 레벨 0~9 (UP +1, DOWN -1)'),
        e.enum('last_action', ea.STATE_GET, ['on', 'off', 'up', 'down', 'none'])
            .withDescription('마지막 입력'),
        e.enum('last_source', ea.STATE_GET, ['remote', 'external'])
            .withDescription('마지막 입력 출처 (remote = 이 장치가 누름, external = 원래 리모컨/히터 버튼)'),
        e.enum('heater_up', ea.SET, ['up']).withDescription('온도/단계 올림 1회'),
        e.enum('heater_down', ea.SET, ['down']).withDescription('온도/단계 내림 1회'),
        e.numeric('power_pulse_ms', ea.ALL)
            .withValueMin(50).withValueMax(5000).withValueStep(50).withUnit('ms')
            .withDescription('ON/OFF 버튼 누름 시간 (길게 눌러야 켜지는 히터는 1000~3000)')
            .withCategory('config'),
        e.numeric('button_pulse_ms', ea.ALL)
            .withValueMin(50).withValueMax(5000).withValueStep(50).withUnit('ms')
            .withDescription('UP/DOWN 버튼 누름 시간')
            .withCategory('config'),
        e.numeric('button_gap_ms', ea.ALL)
            .withValueMin(100).withValueMax(3000).withValueStep(50).withUnit('ms')
            .withDescription('연속 명령 사이 간격')
            .withCategory('config'),
    ],

    configure: async (device, coordinatorEndpoint, loggerOrDefinition) => {
        const log = makeLog(loggerOrDefinition);
        const ep = device.getEndpoint(EP_POWER);
        if (!ep) {
            log.warn('[ZT3L-HEATER] EP1 not found');
            return;
        }
        await reporting.bind(ep, coordinatorEndpoint, ['genOnOff']);
        await reporting.onOff(ep, {min: 1, max: 600, change: 1});
        /* 레벨 / 마지막 입력 — 바뀌면 바로(1초 이내) 리포트, 10분마다 주기 리포트 */
        try {
            await ep.configureReporting('genOnOff', [
                {attribute: {ID: ATTR_LEVEL, type: ZCL_UINT8}, minimumReportInterval: 1, maximumReportInterval: 600, reportableChange: 1},
                {attribute: {ID: ATTR_LAST_ACTION, type: ZCL_ENUM8}, minimumReportInterval: 0, maximumReportInterval: 600, reportableChange: 0},
                {attribute: {ID: ATTR_LAST_SOURCE, type: ZCL_ENUM8}, minimumReportInterval: 0, maximumReportInterval: 600, reportableChange: 0},
            ]);
        } catch (err) {
            log.warn(`[ZT3L-HEATER] status reporting setup failed (old firmware?): ${err.message}`);
        }
        try {
            await ep.read('genOnOff', ['onOff', ...CFG_IDS, ...STATUS_IDS]);
        } catch (err) {
            log.warn(`[ZT3L-HEATER] initial read failed: ${err.message}`);
        }
        log.info('[ZT3L-HEATER] configured');
    },
};

/* ─── 로드 시 검사 — zhc 버전 차이로 없는 컨버터가 있으면 이름을 알려줌 ─── */
['fromZigbee', 'toZigbee'].forEach((k) => {
    definition[k].forEach((c, i) => {
        if (!c) {
            throw new Error(`[ZT3L-HEATER] ${k}[${i}] 컨버터가 이 Z2M 버전에 없습니다`);
        }
    });
});

module.exports = definition;
