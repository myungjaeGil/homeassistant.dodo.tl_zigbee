/**
 * Zigbee2MQTT External Converter
 *
 * Device  : ZT3L-XMAS3CH  (TLSR8258 / Tuya ZT3L) — 크리스마스 트리 3채널 전등
 * Vendor  : DODO
 * Version : 1.0.0
 *
 * EP 구성:
 *   EP1 ~ EP3 : 트리 채널 CH1~CH3 (OnOff + LevelControl)
 *               효과 동작 중에는 ON/OFF = 효과 참여 여부, 밝기 = 그 채널의 최대 밝기
 *   EP4       : 전체 (OnOff + LevelControl) — 전원 + 마스터 밝기
 *
 * 펌웨어 전용 속성:
 *   EP4 genOnOff
 *     0xF000 effect    enum8   R/W  0 steady … 9 auto
 *     0xF001 speed     uint8   R/W  1(느림) ~ 10(빠름)
 *     0xF002 autoSec   uint16  R/W  자동 순환 효과 교체 간격(초) 5~3600
 *     0xF003 running   enum8   R    실제 동작 중인 효과 (auto 일 때 현재 효과)
 *   EP1~EP3 genLevelCtrl
 *     0xF000 dimMinPct uint8   0~99   슬라이더 1%   에 해당하는 실제 밝기(%)
 *     0xF001 dimMaxPct uint8   1~100  슬라이더 100% 에 해당하는 실제 밝기(%)
 *
 * 설치:
 *   data/external_converters/ 에 복사 → Z2M 재시작 → (이미 페어링했다면) Reconfigure
 */

'use strict';

const fz = require('zigbee-herdsman-converters/converters/fromZigbee');
const tz = require('zigbee-herdsman-converters/converters/toZigbee');
const exposes = require('zigbee-herdsman-converters/lib/exposes');
const reporting = require('zigbee-herdsman-converters/lib/reporting');
const utils = require('zigbee-herdsman-converters/lib/utils');
const e = exposes.presets;
const ea = exposes.access;

/* ─── 로거 — Z2M 2.x 는 lib/logger, 1.x 는 configure 인자 ─── */
let zhcLogger = null;
try {
    zhcLogger = require('zigbee-herdsman-converters/lib/logger').logger;
} catch (err) {
    zhcLogger = null;
}
const NS = 'zhc:ZT3L-XMAS3CH';
function makeLog(argLogger) {
    const l = (argLogger && typeof argLogger.info === 'function') ? argLogger : null;
    return {
        info: (msg) => (zhcLogger ? zhcLogger.info(msg, NS) : l ? l.info(msg) : console.log(msg)),
        warn: (msg) => (zhcLogger ? zhcLogger.warning(msg, NS) : l ? l.warn(msg) : console.warn(msg)),
    };
}

/* ─── 엔드포인트 ─────────────────────────────────────────── */
const ENDPOINTS = {
    ch1:    1,
    ch2:    2,
    ch3:    3,
    master: 4,
};
const EP_MASTER = ENDPOINTS.master;
const DIM_RANGE_EPS = ['ch1', 'ch2', 'ch3'];

/* ─── 효과 (펌웨어 xmas_fx.h 와 순서 동일) ─────────────────── */
const EFFECTS = [
    'steady',       // 0 상시 점등
    'waves',        // 1 물결
    'sequential',   // 2 순차 점등
    'glow',         // 3 숨쉬기
    'chase',        // 4 추적
    'crossfade',    // 5 교차 페이드
    'twinkle',      // 6 반짝임
    'flash',        // 7 동시 점멸
    'alternate',    // 8 교대 점멸
    'auto',         // 9 자동 순환
];

const ATTR_EFFECT   = 0xF000;
const ATTR_SPEED    = 0xF001;
const ATTR_AUTO_SEC = 0xF002;
const ATTR_RUNNING  = 0xF003;
const ATTR_DIM_MIN  = 0xF000;
const ATTR_DIM_MAX  = 0xF001;
const ZCL_UINT8  = 0x20;
const ZCL_UINT16 = 0x21;
const ZCL_ENUM8  = 0x30;

const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

/* ─── fromZigbee ─────────────────────────────────────────── */
const fzXmasFx = {
    cluster: 'genOnOff',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        if (msg.endpoint.ID !== EP_MASTER) return;
        const d = msg.data;
        const r = {};
        if (d[ATTR_EFFECT] !== undefined && EFFECTS[d[ATTR_EFFECT]]) r.effect = EFFECTS[d[ATTR_EFFECT]];
        if (d[ATTR_RUNNING] !== undefined && EFFECTS[d[ATTR_RUNNING]]) r.running_effect = EFFECTS[d[ATTR_RUNNING]];
        if (d[ATTR_SPEED] !== undefined) r.effect_speed = d[ATTR_SPEED];
        if (d[ATTR_AUTO_SEC] !== undefined) r.auto_interval = d[ATTR_AUTO_SEC];
        return Object.keys(r).length ? r : undefined;
    },
};

const fzDimRange = {
    cluster: 'genLevelCtrl',
    type: ['attributeReport', 'readResponse'],
    convert: (model, msg, publish, options, meta) => {
        if (msg.endpoint.ID === EP_MASTER) return;
        const result = {};
        const min = msg.data[ATTR_DIM_MIN];
        const max = msg.data[ATTR_DIM_MAX];
        if (min !== undefined) result[utils.postfixWithEndpointName('dim_min', msg, model, meta)] = min;
        if (max !== undefined) result[utils.postfixWithEndpointName('dim_max', msg, model, meta)] = max;
        return Object.keys(result).length ? result : undefined;
    },
};

/* ─── toZigbee ───────────────────────────────────────────── */
const tzXmasFx = {
    key: ['effect', 'effect_speed', 'auto_interval'],
    convertSet: async (entity, key, value, meta) => {
        const ep = meta.device.getEndpoint(EP_MASTER);
        let payload;
        let state;

        if (key === 'effect') {
            const idx = EFFECTS.indexOf(String(value).toLowerCase());
            if (idx < 0) throw new Error(`effect: ${EFFECTS.join(', ')} 중 하나 (${value})`);
            payload = {[ATTR_EFFECT]: {value: idx, type: ZCL_ENUM8}};
            state = {effect: EFFECTS[idx]};
        } else if (key === 'effect_speed') {
            const v = Math.min(10, Math.max(1, Math.round(Number(value))));
            if (!Number.isFinite(v)) throw new Error(`effect_speed: 숫자가 아님 (${value})`);
            payload = {[ATTR_SPEED]: {value: v, type: ZCL_UINT8}};
            state = {effect_speed: v};
        } else {
            const v = Math.min(3600, Math.max(5, Math.round(Number(value))));
            if (!Number.isFinite(v)) throw new Error(`auto_interval: 숫자가 아님 (${value})`);
            payload = {[ATTR_AUTO_SEC]: {value: v, type: ZCL_UINT16}};
            state = {auto_interval: v};
        }

        await ep.write('genOnOff', payload);
        if (key === 'effect') {
            /* 실제 동작 효과(running) 갱신 */
            await ep.read('genOnOff', [ATTR_RUNNING]).catch(() => {});
        }
        return {state};
    },
    convertGet: async (entity, key, meta) => {
        await meta.device.getEndpoint(EP_MASTER)
            .read('genOnOff', [ATTR_EFFECT, ATTR_SPEED, ATTR_AUTO_SEC, ATTR_RUNNING]);
    },
};

const tzDimRange = {
    key: ['dim_min', 'dim_max'],
    convertSet: async (entity, key, value, meta) => {
        const v = Math.round(Number(value));
        const attr = key === 'dim_min' ? ATTR_DIM_MIN : ATTR_DIM_MAX;
        await entity.write('genLevelCtrl', {[attr]: {value: v, type: ZCL_UINT8}});
        /* 펌웨어가 min < max 로 보정할 수 있으므로 반영 후 둘 다 다시 읽음 */
        await sleep(600);
        await entity.read('genLevelCtrl', [ATTR_DIM_MIN, ATTR_DIM_MAX]);
        return {state: {[key]: v}};
    },
    convertGet: async (entity, key, meta) => {
        await entity.read('genLevelCtrl', [ATTR_DIM_MIN, ATTR_DIM_MAX]);
    },
};

/* ─── exposes ────────────────────────────────────────────── */
const dimRangeExposes = DIM_RANGE_EPS.flatMap((name) => [
    e.numeric('dim_min', ea.ALL)
        .withValueMin(0).withValueMax(99).withValueStep(1).withUnit('%')
        .withDescription('슬라이더 1%에 해당하는 실제 밝기')
        .withEndpoint(name).withCategory('config'),
    e.numeric('dim_max', ea.ALL)
        .withValueMin(1).withValueMax(100).withValueStep(1).withUnit('%')
        .withDescription('슬라이더 100%에 해당하는 실제 밝기')
        .withEndpoint(name).withCategory('config'),
]);

const definition = {
    zigbeeModel: ['ZT3L-XMAS3CH'],
    model:       'ZT3L-XMAS3CH',
    vendor:      'DODO',
    description: 'ZT3L 크리스마스 트리 3채널 전등 (CH1~CH3 + 전체, 효과 10종)',

    fromZigbee: [fz.on_off, fz.brightness, fzXmasFx, fzDimRange],
    toZigbee:   [
        tz.light_onoff_brightness,
        tz.light_brightness_move,
        tz.light_brightness_step,
        tzXmasFx,
        tzDimRange,
    ],

    exposes: [
        ...Object.keys(ENDPOINTS).map((name) => e.light().withBrightness().withEndpoint(name)),
        e.enum('effect', ea.ALL, EFFECTS)
            .withLabel('Effect')
            .withDescription('트리 효과 (steady = 효과 없이 상시 점등, auto = 1~8 자동 순환)'),
        e.enum('running_effect', ea.STATE_GET, EFFECTS)
            .withLabel('Running effect')
            .withDescription('지금 동작 중인 효과 (auto 일 때 현재 효과)'),
        e.numeric('effect_speed', ea.ALL)
            .withValueMin(1).withValueMax(10).withValueStep(1)
            .withLabel('Effect speed').withDescription('1 = 느림, 10 = 빠름'),
        e.numeric('auto_interval', ea.ALL)
            .withValueMin(5).withValueMax(3600).withValueStep(5).withUnit('s')
            .withLabel('Auto interval').withDescription('auto 효과에서 다음 효과로 넘어가는 간격')
            .withCategory('config'),
        ...dimRangeExposes,
    ],

    endpoint: (device) => ENDPOINTS,
    meta: {multiEndpoint: true},

    configure: async (device, coordinatorEndpoint, loggerOrDefinition) => {
        const log = makeLog(loggerOrDefinition);

        for (const [name, epNum] of Object.entries(ENDPOINTS)) {
            const ep = device.getEndpoint(epNum);
            if (!ep) {
                log.warn(`[ZT3L-XMAS3CH] EP${epNum}(${name}) not found`);
                continue;
            }
            await reporting.bind(ep, coordinatorEndpoint, ['genOnOff', 'genLevelCtrl']);
            await reporting.onOff(ep, {min: 1, max: 600, change: 1});
            await reporting.brightness(ep, {min: 1, max: 600, change: 1});

            if (DIM_RANGE_EPS.includes(name)) {
                await ep.read('genLevelCtrl', [ATTR_DIM_MIN, ATTR_DIM_MAX]).catch((err) =>
                    log.warn(`[ZT3L-XMAS3CH] EP${epNum} dim range read failed: ${err.message}`));
            }
        }

        /* 효과 속성 리포팅 (IR·버튼으로 바꾼 효과도 HA 에 반영) + 초기값 읽기 */
        const m = device.getEndpoint(EP_MASTER);
        try {
            await m.configureReporting('genOnOff', [
                {attribute: {ID: ATTR_EFFECT,  type: ZCL_ENUM8}, minimumReportInterval: 1, maximumReportInterval: 3600, reportableChange: 0},
                {attribute: {ID: ATTR_RUNNING, type: ZCL_ENUM8}, minimumReportInterval: 1, maximumReportInterval: 3600, reportableChange: 0},
                {attribute: {ID: ATTR_SPEED,   type: ZCL_UINT8}, minimumReportInterval: 1, maximumReportInterval: 3600, reportableChange: 1},
            ]);
        } catch (err) {
            log.warn(`[ZT3L-XMAS3CH] effect reporting failed: ${err.message}`);
        }
        await m.read('genOnOff', [ATTR_EFFECT, ATTR_SPEED, ATTR_AUTO_SEC, ATTR_RUNNING]).catch((err) =>
            log.warn(`[ZT3L-XMAS3CH] effect read failed: ${err.message}`));

        log.info('[ZT3L-XMAS3CH] configured');
    },
};

/* ─── 로드 시 검사 — zhc 버전 차이로 없는 컨버터가 있으면 이름을 알려줌 ─── */
['fromZigbee', 'toZigbee'].forEach((k) => {
    definition[k].forEach((c, i) => {
        if (!c) throw new Error(`[ZT3L-XMAS3CH] ${k}[${i}] 컨버터가 이 Z2M 버전에 없습니다`);
    });
});

module.exports = definition;
