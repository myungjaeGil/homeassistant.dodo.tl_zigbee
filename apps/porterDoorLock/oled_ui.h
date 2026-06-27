/**
 * @file  oled_ui.h
 * @brief OLED UI — SSD1306 128x32  (Porter Door Lock 전용)
 *
 * 화면 레이아웃 (4 pages × 8px):
 *   Page0: "PORTER-DLK  ZB:O/X"    장치명 + Zigbee 연결 상태
 *   Page1: "ACC:[ON/OF] DR:[LK/UN]" ACC 상태 + Door 잠금 상태
 *   Page2: "LCK:[■/□]  ULK:[■/□]"  릴레이 펄스 상태 (펄스 중 ■)
 *   Page3: 최근 이벤트 로그 (예: "LOCK fired", "ACC ON")
 */
#pragma once
#include "tl_common.h"

void oled_ui_init(void);
void oled_ui_update(void);
void oled_ui_notify_zigbee(u8 connected);

/* 이벤트 로그 갱신 — 호출 측에서 문자열 직접 전달 */
void oled_ui_notify_event(const char *msg);
