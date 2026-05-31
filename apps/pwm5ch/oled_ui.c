/**
 * @file  oled_ui.c
 * @brief OLED UI 렌더링 — ZT3L PWM 5CH
 *
 * 의존: i2c_sw, ssd1306, pwm5ch_ep.h (g_pwmChAttrs)
 */
#include "tl_common.h"
#include "oled_ui.h"
#include "i2c_sw.h"
#include "ssd1306.h"
#include "pwm5ch_ep.h"

static u8   s_zb_connected = 0;
static char s_ir_msg[22]   = "";   /* Page3 IR 로그 — 최대 21자 */

/* ON: 채워진 사각형, OFF: 빈 사각형 (5×6px) */
static void _draw_onoff_icon(u8 page, u8 col, u8 on)
{
    u8 y_base = page * 8;
    u8 i;
    if (on) {
        for (i = 0; i < 5 && col + i < SSD1306_WIDTH; i++) {
            for (u8 py = y_base + 1; py < y_base + 7; py++) {
                ssd1306_pixel(col + i, py, 1);
            }
        }
    } else {
        for (i = 0; i < 5 && col + i < SSD1306_WIDTH; i++) {
            ssd1306_pixel(col + i, y_base + 1, 1);
            ssd1306_pixel(col + i, y_base + 6, 1);
        }
        for (u8 py = y_base + 1; py <= y_base + 6; py++) {
            ssd1306_pixel(col,     py, 1);
            ssd1306_pixel(col + 4, py, 1);
        }
    }
}

/* 채널 1개: "N:[아이콘]PCT " — 36px */
static void _draw_channel(u8 page, u8 col, u8 ch_idx)
{
    char num[2] = { '1' + ch_idx, '\0' };
    ssd1306_puts(page, col, num);       col += 6;
    ssd1306_putchar(page, col, ':');    col += 6;

    u8  on  = g_pwmChAttrs[ch_idx].onOff;
    _draw_onoff_icon(page, col, on);    col += 6;

    u16 lv  = g_pwmChAttrs[ch_idx].currentLevel;
    u16 pct = on ? (u16)((lv * 100u + 127u) / 254u) : 0;
    ssd1306_putnum(page, col, pct, 3);
}

void oled_ui_init(void)
{
    i2c_sw_init();
    ssd1306_init();
}

void oled_ui_notify_zigbee(u8 connected)
{
    s_zb_connected = connected;
}

/*--------------------------------------------------------------------
 * oled_ui_notify_ir — IR 커맨드 수신 시 Page3 메시지 갱신
 * ir_recv_cmd.c 에서 호출
 *------------------------------------------------------------------*/
void oled_ui_notify_ir(const char *msg)
{
    u8 i;
    for (i = 0; i < sizeof(s_ir_msg) - 1 && msg[i]; i++) {
        s_ir_msg[i] = msg[i];
    }
    s_ir_msg[i] = '\0';
}

void oled_ui_update(void)
{
    ssd1306_clear();

    /* Page0: 장치명 + Zigbee 상태 */
    ssd1306_puts(0, 0,  "ZT3L-5CH");
    ssd1306_puts(0, 66, "ZB:");
    ssd1306_putchar(0, 84, s_zb_connected ? 'O' : 'X');

    /* Page1: CH1~CH3 */
    _draw_channel(1,  0, 0);
    _draw_channel(1, 37, 1);
    _draw_channel(1, 74, 2);

    /* Page2: CH4~CH5 + MASTER */
    _draw_channel(2,  0, 3);
    _draw_channel(2, 37, 4);
    _draw_channel(2, 74, 5);

    /* Page3: IR 커맨드 로그 */
    ssd1306_puts(3, 0, "IR:");
    ssd1306_puts(3, 18, s_ir_msg[0] ? s_ir_msg : "-");

    ssd1306_flush();
}
