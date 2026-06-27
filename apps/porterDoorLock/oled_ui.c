/**
 * @file  oled_ui.c
 * @brief OLED UI 렌더링 — Porter Door Lock (SSD1306 128x32)
 *
 * 레이아웃:
 *   Page0  "PORTER-DLK  ZB:O"   장치명 + Zigbee 상태
 *   Page1  "ACC:ON  DOOR:LK"    입력 상태 (ACC / Door)
 *   Page2  "LCK:[■]  ULK:[□]"   릴레이 펄스 상태
 *   Page3  "EVT: LOCK fired"    최근 이벤트 로그
 *
 * 의존: i2c_sw, ssd1306, doorLock_ep.h
 */
#include "tl_common.h"
#include "oled_ui.h"
#include "i2c_sw.h"
#include "ssd1306.h"
#include "doorLock_ep.h"

static u8   s_zb_connected = 0;
static char s_event_msg[22] = "";   /* 최근 이벤트 — 최대 21자 */

/*--------------------------------------------------------------------
 * 채워진 사각형(■) / 빈 사각형(□) 아이콘 — 5×6px
 *------------------------------------------------------------------*/
static void _draw_box(u8 page, u8 col, u8 filled)
{
    u8 y_base = page * 8;
    u8 i;
    if (filled) {
        for (i = 0; i < 5 && col + i < SSD1306_WIDTH; i++) {
            u8 py;
            for (py = y_base + 1; py < y_base + 7; py++) {
                ssd1306_pixel(col + i, py, 1);
            }
        }
    } else {
        /* 테두리만 */
        for (i = 0; i < 5 && col + i < SSD1306_WIDTH; i++) {
            ssd1306_pixel(col + i, y_base + 1, 1);
            ssd1306_pixel(col + i, y_base + 6, 1);
        }
        u8 py;
        for (py = y_base + 1; py <= y_base + 6; py++) {
            ssd1306_pixel(col,     py, 1);
            ssd1306_pixel(col + 4, py, 1);
        }
    }
}

/*--------------------------------------------------------------------
 * oled_ui_init
 *------------------------------------------------------------------*/
void oled_ui_init(void)
{
    i2c_sw_init();
    ssd1306_init();
}

void oled_ui_notify_zigbee(u8 connected)
{
    s_zb_connected = connected;
}

void oled_ui_notify_event(const char *msg)
{
    u8 i;
    for (i = 0; i < sizeof(s_event_msg) - 1 && msg[i]; i++) {
        s_event_msg[i] = msg[i];
    }
    s_event_msg[i] = '\0';
}

/*--------------------------------------------------------------------
 * oled_ui_update — 매 폴링 주기(100ms)에서 호출
 *------------------------------------------------------------------*/
void oled_ui_update(void)
{
    ssd1306_clear();

    /* -------------------------------------------------------
     * Page0: "PORTER-DLK  ZB:O/X"
     * ------------------------------------------------------- */
    ssd1306_puts(0, 0,  "PORTER-DLK");
    ssd1306_puts(0, 66, "ZB:");
    ssd1306_putchar(0, 84, s_zb_connected ? 'O' : 'X');

    /* -------------------------------------------------------
     * Page1: 입력 상태
     *   ACC:ON/OF   DOOR:LK/UN
     *   col0        col64
     * ------------------------------------------------------- */
    {
        u8 acc_on  = (g_dlEpAttrs[0].onOff) ? 1 : 0;
        u8 door_lk = (g_dlEpAttrs[1].onOff) ? 1 : 0;

        ssd1306_puts(1,  0, "ACC:");
        ssd1306_puts(1, 24, acc_on ? "ON " : "OF ");

        ssd1306_puts(1, 64, "DR:");
        ssd1306_puts(1, 82, door_lk ? "LK" : "UN");
    }

    /* -------------------------------------------------------
     * Page2: 릴레이 펄스 상태
     *   LCK:[■/□]  ULK:[■/□]
     *   col0              col64
     * ------------------------------------------------------- */
    {
        u8 lck_on = g_dlEpAttrs[2].onOff ? 1 : 0;
        u8 ulk_on = g_dlEpAttrs[3].onOff ? 1 : 0;

        ssd1306_puts(2,  0, "LCK:");
        _draw_box(2, 24, lck_on);

        ssd1306_puts(2, 64, "ULK:");
        _draw_box(2, 88, ulk_on);
    }

    /* -------------------------------------------------------
     * Page3: 최근 이벤트 로그
     *   "EVT:LOCK fired"
     * ------------------------------------------------------- */
    ssd1306_puts(3, 0, "EVT:");
    ssd1306_puts(3, 24, s_event_msg[0] ? s_event_msg : "-");

    ssd1306_flush();
}
