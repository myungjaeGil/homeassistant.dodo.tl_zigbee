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
#include "zb_api.h"

/*--------------------------------------------------------------------
 * 갱신 정책
 *   [수정] 기존에는 메인 루프마다 512B 전체 프레임을 소프트 I2C 로 전송
 *          → 루프가 수십 ms 씩 멈춰 Zigbee 처리 지연.
 *   표시 내용이 바뀐 경우에만, 최소 OLED_MIN_INTERVAL_MS 간격으로 갱신.
 *------------------------------------------------------------------*/
#define OLED_MIN_INTERVAL_MS    200

static u8   s_zb_connected = 0;
static char s_ir_msg[22]   = "";   /* Page3 IR 로그 — 최대 21자 */
static bool s_dirty        = TRUE;
static u32  s_last_draw    = 0;

/* 화면에 표시되는 값의 스냅샷 — 바뀌었는지 비교용 */
typedef struct {
    u8 onOff[PWM_EP_COUNT];
    u8 level[PWM_EP_COUNT];
    u8 zb;
} oled_snapshot_t;
static oled_snapshot_t s_snap;

static void _take_snapshot(oled_snapshot_t *sn)
{
    u8 i;
    for (i = 0; i < PWM_EP_COUNT; i++) {
        sn->onOff[i] = g_pwmChAttrs[i].onOff ? 1 : 0;
        sn->level[i] = g_pwmChAttrs[i].currentLevel;
    }
    sn->zb = s_zb_connected;
}

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
    char num[2] = { (ch_idx == 5) ? 'M' : (char)('1' + ch_idx), '\0' };
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
    s_zb_connected = connected ? 1 : 0;
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
    s_dirty = TRUE;
}

void oled_ui_update(void)
{
    oled_snapshot_t now;

    if (!clock_time_exceed(s_last_draw, OLED_MIN_INTERVAL_MS * 1000)) return;

    /* 조인 상태는 직접 폴링 — 콜백 누락(steering 성공, leave 등)에도 반영 */
    s_zb_connected = zb_isDeviceJoinedNwk() ? 1 : 0;

    _take_snapshot(&now);
    if (!s_dirty && memcmp(&now, &s_snap, sizeof(now)) == 0) return;

    s_snap      = now;
    s_dirty     = FALSE;
    s_last_draw = clock_time();

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
