/**
 * @file  ssd1306.h
 * @brief SSD1306 OLED driver — 128x32, I2C
 *
 * 의존: i2c_sw.h
 */
#pragma once
#include "tl_common.h"

#define SSD1306_I2C_ADDR   0x3C   /* SA0=GND → 0x3C, SA0=VCC → 0x3D */
#define SSD1306_WIDTH      128
#define SSD1306_HEIGHT     32
#define SSD1306_PAGES      (SSD1306_HEIGHT / 8)   /* 4 pages */

/* ── API ────────────────────────────────────────────────── */

/** 초기화 (i2c_sw_init() 호출 후 사용) */
void ssd1306_init(void);

/** 내부 프레임버퍼 전체를 OLED로 전송 */
void ssd1306_flush(void);

/** 프레임버퍼 지우기 */
void ssd1306_clear(void);

/**
 * @brief 5x7 폰트로 문자 1개 출력
 * @param page  0~3  (8px 단위 행)
 * @param col   0~127
 */
void ssd1306_putchar(u8 page, u8 col, char c);

/**
 * @brief 문자열 출력
 * @param page  0~3
 * @param col   시작 열
 */
void ssd1306_puts(u8 page, u8 col, const char *s);

/**
 * @brief 숫자(정수) 출력 — 최대 4자리, 우측 정렬
 */
void ssd1306_putnum(u8 page, u8 col, u16 n, u8 digits);

/** 단일 픽셀 set/clear (직접 프레임버퍼 조작) */
void ssd1306_pixel(u8 x, u8 y, u8 on);
