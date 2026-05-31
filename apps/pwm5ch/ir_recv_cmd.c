/*********************************************************************
 * @file    ir_recv_cmd.c
 * @brief   IR 수신 커맨드 → PWM 채널 제어
 *
 * 리모컨 레이아웃:
 *   [0x45]              [0x47]    ALL ON        ALL OFF
 *   [0x44]  [0x40]  [0x43]       CH1 토글  CH1 UP   CH1 DN
 *   [0x07]  [0x15]  [0x09]       CH2 토글  CH2 UP   CH2 DN
 *   [0x16]  [0x19]  [0x0D]       CH3 토글  CH3 UP   CH3 DN
 *   [0x0C]  [0x18]  [0x5E]       CH4 토글  CH4 UP   CH4 DN
 *   [0x08]  [0x1C]  [0x5A]       CH5 토글  CH5 UP   CH5 DN
 *   [0x42]  [0x52]  [0x4A]       MASTER 토글  MASTER UP  MASTER DN
 *
 * 채터링 방지:
 *   토글/ON/OFF —  500ms debounce (중복 방지)
 *   다른 cmd    —  250ms debounce (이전 버튼 잔류 제거)
 *   UP/DN       —  100ms debounce (연속 조절 허용)
 *********************************************************************/

#include "tl_common.h"
#include "zb_api.h"
#include "ir_recv.h"
#include "pwm5ch_ep.h"
#include "pwm5ch_ctrl.h"
#include "pwm5ch_monitor.h"
#include "oled_ui.h"

extern void pwm_attrs_save(u8 ep_idx);

/*====================================================================
 * 내부 헬퍼
 *==================================================================*/
static void ir_ch_set_onoff(u8 ep_idx, bool on)
{
    g_pwmChAttrs[ep_idx].onOff = on;
    pwm_hw_apply(ep_idx);
    pwm_attrs_save(ep_idx);
}

static void ir_ch_toggle(u8 ep_idx)
{
    ir_ch_set_onoff(ep_idx, !g_pwmChAttrs[ep_idx].onOff);
}

static void ir_ch_adjust(u8 ep_idx, s8 delta_pct)
{
    s16 cur  = (s16)g_pwmChAttrs[ep_idx].currentLevel;
    s16 step = (s16)(254 * delta_pct / 100);
    cur += step;
    if (cur < 1)   cur = 1;
    if (cur > 254) cur = 254;
    g_pwmChAttrs[ep_idx].currentLevel = (u8)cur;
    g_pwmChAttrs[ep_idx].onOff = TRUE;
    pwm_hw_apply(ep_idx);
    pwm_attrs_save(ep_idx);
}

/* MASTER UP/DN — MASTER 레벨 조절 후 CH0~CH4 전체 재적용
 * 개별 채널 onOff 상태는 유지하되 MASTER 레벨만 변경 */
static void ir_master_adjust(s8 delta_pct)
{
    s16 cur  = (s16)g_pwmChAttrs[5].currentLevel;
    s16 step = (s16)(254 * delta_pct / 100);
    cur += step;
    if (cur < 1)   cur = 1;
    if (cur > 254) cur = 254;
    g_pwmChAttrs[5].currentLevel = (u8)cur;
    g_pwmChAttrs[5].onOff = TRUE;
    /* ep_idx=5 → pwm_hw_apply 내부에서 CH0~CH4 전체 combined 재계산
     * pwm5ch_ctrl.c: combined = ch_level * master_level / 254 */
    pwm_hw_apply(5);
    pwm_attrs_save(5);

    printf("[IR] MASTER level=%d pct\r\n", (int)(cur * 100 / 254));
}

/*====================================================================
 * UP/DN 커맨드 판별
 *==================================================================*/
static bool ir_is_updn(u8 cmd)
{
    switch (cmd) {
    case 0x40: case 0x43:
    case 0x15: case 0x09:
    case 0x19: case 0x0D:
    case 0x18: case 0x5E:
    case 0x1C: case 0x5A:
    case 0x52: case 0x4A:
        return TRUE;
    default:
        return FALSE;
    }
}

/*====================================================================
 * 채터링 방지
 *   UP/DN:      100ms debounce (연속 조절)
 *   동일 cmd:   500ms debounce
 *   다른 cmd:   250ms debounce
 *==================================================================*/
#define IR_DEBOUNCE_UPDN_MS    100
#define IR_DEBOUNCE_SAME_MS    500
#define IR_DEBOUNCE_DIFF_MS    250

static bool ir_debounce(u8 cmd)
{
    static u8  s_last_cmd  = 0xFF;
    static u32 s_last_time = 0;

    u32 now   = clock_time();
    u32 dt    = (now >= s_last_time) ? (now - s_last_time)
                                     : (0xFFFFFFFFu - s_last_time + now + 1);
    u32 dt_ms = dt / 16000u;

    u32 limit;
    if (ir_is_updn(cmd))        limit = IR_DEBOUNCE_UPDN_MS;
    else if (cmd == s_last_cmd) limit = IR_DEBOUNCE_SAME_MS;
    else                        limit = IR_DEBOUNCE_DIFF_MS;

    if (dt_ms < limit) return FALSE;

    s_last_cmd  = cmd;
    s_last_time = now;
    return TRUE;
}

/*====================================================================
 * ir_recv_on_cmd — ir_recv.c 에서 호출되는 콜백
 *==================================================================*/
void ir_recv_on_cmd(u8 addr, u8 cmd, bool repeat)
{
    if (repeat) {
        /* repeat — UP/DN 만 허용, 직전 cmd 일치 필수 */
        if (!ir_is_updn(cmd)) return;
        /* debounce 없이 바로 처리 (repeat 간격 ~110ms 가 자연 debounce) */
    } else {
        if (!ir_debounce(cmd)) return;
    }

    switch (cmd) {

    /* ── 전채널 ── */
    case 0x45:
        for (u8 i = 0; i <= 5; i++) ir_ch_set_onoff(i, TRUE);
        printf("[IR] ALL ON\r\n");
        oled_ui_notify_ir("ALL ON");
        break;

    case 0x47:
        for (u8 i = 0; i <= 5; i++) ir_ch_set_onoff(i, FALSE);
        printf("[IR] ALL OFF\r\n");
        oled_ui_notify_ir("ALL OFF");
        break;

    /* ── CH1 (ep_idx=0) ── */
    case 0x44: ir_ch_toggle(0);       printf("[IR] CH1 toggle\r\n"); oled_ui_notify_ir("CH1 toggle"); break;
    case 0x40: ir_ch_adjust(0, +5);  printf("[IR] CH1 UP\r\n");     oled_ui_notify_ir("CH1 UP");     break;
    case 0x43: ir_ch_adjust(0, -5);  printf("[IR] CH1 DN\r\n");     oled_ui_notify_ir("CH1 DN");     break;

    /* ── CH2 (ep_idx=1) ── */
    case 0x07: ir_ch_toggle(1);       printf("[IR] CH2 toggle\r\n"); oled_ui_notify_ir("CH2 toggle"); break;
    case 0x15: ir_ch_adjust(1, +5);  printf("[IR] CH2 UP\r\n");     oled_ui_notify_ir("CH2 UP");     break;
    case 0x09: ir_ch_adjust(1, -5);  printf("[IR] CH2 DN\r\n");     oled_ui_notify_ir("CH2 DN");     break;

    /* ── CH3 (ep_idx=2) ── */
    case 0x16: ir_ch_toggle(2);       printf("[IR] CH3 toggle\r\n"); oled_ui_notify_ir("CH3 toggle"); break;
    case 0x19: ir_ch_adjust(2, +5);  printf("[IR] CH3 UP\r\n");     oled_ui_notify_ir("CH3 UP");     break;
    case 0x0D: ir_ch_adjust(2, -5);  printf("[IR] CH3 DN\r\n");     oled_ui_notify_ir("CH3 DN");     break;

    /* ── CH4 (ep_idx=3) ── */
    case 0x0C: ir_ch_toggle(3);       printf("[IR] CH4 toggle\r\n"); oled_ui_notify_ir("CH4 toggle"); break;
    case 0x18: ir_ch_adjust(3, +5);  printf("[IR] CH4 UP\r\n");     oled_ui_notify_ir("CH4 UP");     break;
    case 0x5E: ir_ch_adjust(3, -5);  printf("[IR] CH4 DN\r\n");     oled_ui_notify_ir("CH4 DN");     break;

    /* ── CH5 (ep_idx=4) ── */
    case 0x08: ir_ch_toggle(4);       printf("[IR] CH5 toggle\r\n"); oled_ui_notify_ir("CH5 toggle"); break;
    case 0x1C: ir_ch_adjust(4, +5);  printf("[IR] CH5 UP\r\n");     oled_ui_notify_ir("CH5 UP");     break;
    case 0x5A: ir_ch_adjust(4, -5);  printf("[IR] CH5 DN\r\n");     oled_ui_notify_ir("CH5 DN");     break;

    /* ── MASTER (ep_idx=5) ── */
    case 0x42: ir_ch_toggle(5);         printf("[IR] MASTER toggle\r\n"); oled_ui_notify_ir("MASTER toggle"); break;
    case 0x52: ir_master_adjust(+5);   printf("[IR] MASTER UP\r\n");     oled_ui_notify_ir("MASTER UP");     break;
    case 0x4A: ir_master_adjust(-5);   printf("[IR] MASTER DN\r\n");     oled_ui_notify_ir("MASTER DN");     break;

    default:
        printf("[IR] unknown cmd=0x%02x\r\n", (int)cmd);
        break;
    }
}
