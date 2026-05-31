/**
 * @file  oled_ui.h
 * @brief OLED UI — SSD1306 128x32
 *
 * 화면 레이아웃 (4 pages × 8px):
 *   Page0: "ZT3L-5CH    ZB:O/X"   장치명 + Zigbee 연결 상태
 *   Page1: "1:[■]100 2:[■] 75 3:[□]  0"
 *   Page2: "4:[■] 50 5:[□]  0 M:[■] 80"
 *   Page3: IR 커맨드 로그 (최근 수신 커맨드)
 */
#pragma once
#include "tl_common.h"

void oled_ui_init(void);
void oled_ui_update(void);
void oled_ui_notify_zigbee(u8 connected);
void oled_ui_notify_ir(const char *msg);
