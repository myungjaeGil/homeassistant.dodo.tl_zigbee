/**
 * @file  oled_ui.h
 * @brief OLED UI — SSD1306 128x32, 무시동히터 리모컨
 *
 * 화면 레이아웃 (4 pages × 8px):
 *   Page0: "HEATER RC      ZB:O"   장치명 + Zigbee 연결 상태
 *   Page1: "POWER: ON     LV:3"    히터 전원 + 레벨(0~9)
 *   Page2: "LAST: UP  EXT  Q:2"    마지막 입력 + 출처(RC=이 장치, EXT=외부) + 대기 수
 *   Page3: "IR: DOWN"              최근 IR 수신
 *
 * oled_ui_update() 는 매 루프 호출해도 됨 — 내용 변경 시에만 200ms 간격으로 전송
 * HEATER_OLED_ENABLE = 0 이면 모든 함수가 빈 동작
 */
#pragma once
#include "tl_common.h"

void oled_ui_init(void);
void oled_ui_update(void);
void oled_ui_notify_zigbee(u8 connected);
void oled_ui_notify_ir(const char *msg);
