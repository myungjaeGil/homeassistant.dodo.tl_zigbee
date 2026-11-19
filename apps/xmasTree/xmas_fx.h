#pragma once
/*********************************************************************
 * @file    xmas_fx.h
 * @brief   크리스마스 트리 3CH 효과 엔진
 *
 *  효과 번호는 Zigbee 속성 EP4 OnOff 0xF000 값과 같음 (Z2M 컨버터와 일치시켜야 함)
 *
 *   0 STEADY     상시 점등 (효과 없음 — 채널별 밝기 그대로)
 *   1 WAVES      물결 — 세 채널이 120° 씩 어긋나게 밝아졌다 어두워짐
 *   2 SEQUENTIAL 순차 점등 — 1 → 1+2 → 1+2+3 → 전체 꺼짐 반복
 *   3 GLOW       숨쉬기 — 전 채널 동시에 천천히 밝아졌다 어두워짐
 *   4 CHASE      추적 — 한 채널씩 돌아가며 켜짐
 *   5 CROSSFADE  교차 페이드 — 한 채널이 꺼지면서 다음 채널이 켜짐
 *   6 TWINKLE    반짝임 — 채널마다 무작위로 반짝임
 *   7 FLASH      동시 점멸
 *   8 ALTERNATE  교대 점멸 — CH1·CH3 ↔ CH2
 *   9 AUTO       자동 순환 — 1~8 을 autoSec 마다 차례로 교체
 *
 *  동작 조건: EP4(전체) ON 이고 효과 != STEADY
 *  꺼진 채널(EP1~EP3 OFF)은 효과에서 빠짐. 채널 레벨 = 그 채널의 최대 밝기.
 *********************************************************************/
#include "tl_common.h"

enum {
    XMAS_FX_STEADY = 0,
    XMAS_FX_WAVES,
    XMAS_FX_SEQUENTIAL,
    XMAS_FX_GLOW,
    XMAS_FX_CHASE,
    XMAS_FX_CROSSFADE,
    XMAS_FX_TWINKLE,
    XMAS_FX_FLASH,
    XMAS_FX_ALTERNATE,
    XMAS_FX_AUTO,
    XMAS_FX_COUNT
};

#define XMAS_FX_FIRST_ANIM      XMAS_FX_WAVES       /* 자동 순환 범위 */
#define XMAS_FX_LAST_ANIM       XMAS_FX_ALTERNATE

#define XMAS_SPEED_MIN          1
#define XMAS_SPEED_MAX          10
#define XMAS_AUTO_SEC_MIN       5
#define XMAS_AUTO_SEC_MAX       3600

/* 부팅 시 1회 — NV 복원(xmas_fxCfg_init, pwm_attrs_init) 이후 호출 */
void xmas_fx_init(void);

/* 설정/전원 상태가 바뀌면 호출 — 엔진 시작·정지·효과 재시작 판단 */
void xmas_fx_refresh(void);

/* 로컬 조작 (버튼 / IR) — 속성 갱신 + NV 저장 + 엔진 반영 */
void xmas_fx_select(u8 effect);
void xmas_fx_next(void);
void xmas_fx_prev(void);
void xmas_fx_speed_step(s8 delta);

/* 표시용 */
const char *xmas_fx_name(u8 effect);    /* 9자 이하 */
bool        xmas_fx_is_active(void);
