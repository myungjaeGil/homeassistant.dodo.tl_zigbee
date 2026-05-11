#pragma once
/*********************************************************************
 * @file    epeverCtrl.h
 * @brief   ZT3L EPever MPPT 모니터 — LED/버튼 제어 인터페이스
 *********************************************************************/

#include "tl_common.h"

/*--------------------------------------------------------------------
 * LED_POWER 상태 정의
 *------------------------------------------------------------------*/
typedef enum {
    LED_PWR_STATE_BOOT        = 0,
    LED_PWR_STATE_JOINED      = 1,
    LED_PWR_STATE_NOT_JOINED  = 2,
    LED_PWR_STATE_RESET       = 3,
} led_pwr_state_t;

/*--------------------------------------------------------------------
 * 함수 선언
 *------------------------------------------------------------------*/
void epever_hw_init(void);

void led_power_init(void);
void led_power_set_state(led_pwr_state_t state);

void light_blink_start(u8 times, u16 ledOnTime, u16 ledOffTime);
void light_blink_stop(void);

/* SDK 내부 호환 */
void sampleLight_onOffInit(void);
void sampleLight_onOffUpdate(u8 cmd);

#define LIGHT_STA_ON_OFF    1
#define LIGHT_STA_LEVEL     2
void light_refresh(u8 sta);

void light_applyUpdate(u8 *curLevel, u16 *curLevel256, s32 *stepLevel256,
                       u16 *remainingTime, u8 minLevel, u8 maxLevel, bool wrap);
void light_applyUpdate_16(u16 *curLevel, u32 *curLevel256, s32 *stepLevel256,
                          u16 *remainingTime, u16 minLevel, u16 maxLevel, bool wrap);
