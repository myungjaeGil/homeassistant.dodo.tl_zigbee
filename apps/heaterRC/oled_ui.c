/**
 * @file  oled_ui.c
 * @brief OLED UI 렌더링 — 무시동히터 리모컨
 */
#include "tl_common.h"
#include "zb_api.h"
#include "oled_ui.h"

#if HEATER_OLED_ENABLE

#include "i2c_sw.h"
#include "ssd1306.h"
#include "heater_main.h"

#define OLED_MIN_INTERVAL_MS    200

static u8   s_zb_connected = 0;
static char s_ir_msg[16]   = "";
static bool s_dirty        = TRUE;
static u32  s_last_draw    = 0;

typedef struct {
    u8 power;
    u8 level;
    u8 lastAct;
    u8 lastSrc;
    u8 pending;
    u8 zb;
} oled_snapshot_t;
static oled_snapshot_t s_snap;

static void _take_snapshot(oled_snapshot_t *sn)
{
    sn->power   = g_heaterOnOff[HEATER_IDX_POWER] ? 1 : 0;
    sn->level   = g_heaterLevel;
    sn->lastAct = (u8)heater_last_act();
    sn->lastSrc = g_heaterLastSource;
    sn->pending = heater_pending();
    sn->zb      = s_zb_connected;
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

    s_zb_connected = zb_isDeviceJoinedNwk() ? 1 : 0;

    _take_snapshot(&now);
    if (!s_dirty && memcmp(&now, &s_snap, sizeof(now)) == 0) return;

    s_snap      = now;
    s_dirty     = FALSE;
    s_last_draw = clock_time();

    ssd1306_clear();

    /* Page0: 장치명 + Zigbee */
    ssd1306_puts(0, 0,  "HEATER RC");
    ssd1306_puts(0, 90, "ZB:");
    ssd1306_putchar(0, 108, now.zb ? 'O' : 'X');

    /* Page1: 전원 + 레벨 */
    ssd1306_puts(1, 0,  "POWER:");
    ssd1306_puts(1, 42, now.power ? "ON" : "OFF");
    ssd1306_puts(1, 84, "LV:");
    ssd1306_putnum(1, 102, now.level, 1);

    /* Page2: 마지막 입력 (동작 + 출처) + 대기 */
    ssd1306_puts(2, 0,  "LAST:");
    ssd1306_puts(2, 36, heater_act_name((heater_act_t)now.lastAct));
    if (now.lastAct < HEATER_ACT_COUNT) {
        ssd1306_puts(2, 66, (now.lastSrc == HEATER_SRC_EXTERNAL) ? "EXT" : "RC");
    }
    ssd1306_puts(2, 90, "Q:");
    ssd1306_putnum(2, 102, now.pending, 1);

    /* Page3: IR */
    ssd1306_puts(3, 0,  "IR:");
    ssd1306_puts(3, 24, s_ir_msg[0] ? s_ir_msg : "-");

    ssd1306_flush();
}

#else  /* HEATER_OLED_ENABLE == 0 */

void oled_ui_init(void)                  { }
void oled_ui_update(void)                { }
void oled_ui_notify_zigbee(u8 connected) { (void)connected; }
void oled_ui_notify_ir(const char *msg)  { (void)msg; }

#endif
