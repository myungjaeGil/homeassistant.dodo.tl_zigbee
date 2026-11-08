/*********************************************************************
 * @file    ir_recv_cmd.c
 * @brief   IR 수신 커맨드 → 히터 버튼
 *
 * 리모컨 키 (PWM5CH 와 같은 NEC 리모컨 기준, 필요하면 아래 코드만 바꾸면 됨):
 *   [0x45] ON      [0x47] OFF
 *   [0x40] UP      [0x43] DOWN
 *
 * 채터링 / 반복:
 *   ON / OFF  — 같은 키 1초 안 재입력 무시, 반복 프레임 무시
 *   UP / DOWN — 150ms 디바운스, 길게 누름(반복 프레임)은 대기열이 비었을 때만
 *               한 단계씩 추가 → 손을 떼면 바로 멈춤 (밀린 펄스가 남지 않음)
 *********************************************************************/

#include "tl_common.h"
#include "ir_recv.h"
#include "heater_main.h"
#include "oled_ui.h"

#if IR_RECV_ENABLE

#define IR_KEY_ON           0x45
#define IR_KEY_OFF          0x47
#define IR_KEY_UP           0x40
#define IR_KEY_DOWN         0x43

#define IR_DEBOUNCE_POWER_MS   1000
#define IR_DEBOUNCE_UPDN_MS    150

static u32 ms_since(u32 t)
{
    return (u32)((clock_time() - t) / 16000u);
}

static bool ir_debounce(u8 cmd, u32 limit_ms)
{
    static u8  s_last_cmd  = 0xFF;
    static u32 s_last_time = 0;

    if (cmd == s_last_cmd && ms_since(s_last_time) < limit_ms) return FALSE;
    s_last_cmd  = cmd;
    s_last_time = clock_time();
    return TRUE;
}

void ir_recv_on_cmd(u8 addr, u8 cmd, bool repeat)
{
    bool ok;
    (void)addr;

    switch (cmd) {
    case IR_KEY_ON:
    case IR_KEY_OFF:
        if (repeat) return;
        if (!ir_debounce(cmd, IR_DEBOUNCE_POWER_MS)) return;
        ok = heater_power_set(cmd == IR_KEY_ON);
        DBG_LOG("[IR] %s %s\r\n", (cmd == IR_KEY_ON) ? "ON" : "OFF", ok ? "" : "(rejected)");
        oled_ui_notify_ir((cmd == IR_KEY_ON) ? "ON" : "OFF");
        break;

    case IR_KEY_UP:
    case IR_KEY_DOWN:
        if (repeat) {
            /* 길게 누름 — 이전 펄스가 끝났을 때만 한 단계 추가 */
            if (heater_is_busy()) return;
        } else {
            if (!ir_debounce(cmd, IR_DEBOUNCE_UPDN_MS)) return;
        }
        ok = heater_step(cmd == IR_KEY_UP);
        DBG_LOG("[IR] %s %s\r\n", (cmd == IR_KEY_UP) ? "UP" : "DOWN", ok ? "" : "(rejected)");
        oled_ui_notify_ir((cmd == IR_KEY_UP) ? "UP" : "DOWN");
        break;

    default:
        DBG_LOG("[IR] unknown cmd=0x%02x addr=0x%02x\r\n", (int)cmd, (int)addr);
        break;
    }
}

#endif /* IR_RECV_ENABLE */
