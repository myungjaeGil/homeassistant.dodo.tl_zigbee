/*********************************************************************
 * @file    ir_recv_cmd.c
 * @brief   IR 수신 커맨드 → 트리 전등 제어 (PWM5CH 리모컨 배치 그대로 사용)
 *
 * 리모컨 레이아웃:
 *   [0x45]              [0x47]    전체 ON       전체 OFF
 *   [0x44]  [0x40]  [0x43]       CH1 토글  CH1 UP     CH1 DN
 *   [0x07]  [0x15]  [0x09]       CH2 토글  CH2 UP     CH2 DN
 *   [0x16]  [0x19]  [0x0D]       CH3 토글  CH3 UP     CH3 DN
 *   [0x0C]  [0x18]  [0x5E]       다음 효과 속도 UP    속도 DN
 *   [0x08]  [0x1C]  [0x5A]       이전 효과 상시 점등  자동 순환
 *   [0x42]  [0x52]  [0x4A]       전체 토글 전체 밝기 UP 전체 밝기 DN
 *
 * 채터링 방지:
 *   토글/ON/OFF —  500ms debounce (중복 방지)
 *   다른 cmd    —  250ms debounce (이전 버튼 잔류 제거)
 *   UP/DN       —  100ms debounce (연속 조절 허용) — 효과 속도는 제외(한 번에 1단계)
 *********************************************************************/

#include "tl_common.h"
#include "zb_api.h"
#include "ir_recv.h"
#include "xmas_ep.h"
#include "xmas_ctrl.h"
#include "xmas_main.h"
#include "oled_ui.h"
#include "xmas_fx.h"


/*====================================================================
 * 내부 헬퍼
 *==================================================================*/
/* [수정] startUpOnOff / startUpLevel 을 덮어쓰지 않음.
 *   기존에는 IR 조작 값이 startUp 값으로 고정 저장되어,
 *   이후 Zigbee 로 끈 채널이 전원 재투입 시 다시 켜지는 문제가 있었음.
 *   startUp = 0xFF(이전 상태 복원) 이므로 현재 상태만 저장하면 됨. */
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
    if (cur < 13)  cur = 13;   /* 최소 5% */
    if (cur > 254) cur = 254;
    g_pwmChAttrs[ep_idx].currentLevel = (u8)cur;
    g_pwmChAttrs[ep_idx].onOff = TRUE;
    pwm_hw_apply(ep_idx);
    pwm_attrs_save(ep_idx);
}

/* MASTER UP/DN — MASTER 레벨 조절 후 CH1~CH3 전체 재적용
 * 개별 채널 onOff 상태는 유지하되 MASTER 레벨만 변경 */
static void ir_master_adjust(s8 delta_pct)
{
    s16 cur  = (s16)g_pwmChAttrs[PWM_MASTER_IDX].currentLevel;
    s16 step = (s16)(254 * delta_pct / 100);
    cur += step;
    if (cur < 13)  cur = 13;   /* 최소 5% */
    if (cur > 254) cur = 254;
    g_pwmChAttrs[PWM_MASTER_IDX].currentLevel = (u8)cur;
    g_pwmChAttrs[PWM_MASTER_IDX].onOff = TRUE;
    /* 마스터 → pwm_hw_apply 내부에서 CH1~CH3 전체 재계산 */
    pwm_hw_apply(PWM_MASTER_IDX);
    pwm_attrs_save(PWM_MASTER_IDX);

    DBG_LOG("[IR] MASTER level=%d pct\r\n", (int)(cur * 100 / 254));
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

    /* ── 전체 ── */
    case 0x45:
        for (u8 i = 0; i < PWM_EP_COUNT; i++) ir_ch_set_onoff(i, TRUE);
        DBG_LOG("[IR] ALL ON\r\n");
        oled_ui_notify_ir("ALL ON");
        break;

    case 0x47:
        ir_ch_set_onoff(PWM_MASTER_IDX, FALSE);   /* 전체 전원만 끔 — 채널 선택 상태는 유지 */
        DBG_LOG("[IR] ALL OFF\r\n");
        oled_ui_notify_ir("ALL OFF");
        break;

    /* ── CH1 (ep_idx=0) ── */
    case 0x44: ir_ch_toggle(0);       DBG_LOG("[IR] CH1 toggle\r\n"); oled_ui_notify_ir("CH1 toggle"); break;
    case 0x40: ir_ch_adjust(0, +5);  DBG_LOG("[IR] CH1 UP\r\n");     oled_ui_notify_ir("CH1 UP");     break;
    case 0x43: ir_ch_adjust(0, -5);  DBG_LOG("[IR] CH1 DN\r\n");     oled_ui_notify_ir("CH1 DN");     break;

    /* ── CH2 (ep_idx=1) ── */
    case 0x07: ir_ch_toggle(1);       DBG_LOG("[IR] CH2 toggle\r\n"); oled_ui_notify_ir("CH2 toggle"); break;
    case 0x15: ir_ch_adjust(1, +5);  DBG_LOG("[IR] CH2 UP\r\n");     oled_ui_notify_ir("CH2 UP");     break;
    case 0x09: ir_ch_adjust(1, -5);  DBG_LOG("[IR] CH2 DN\r\n");     oled_ui_notify_ir("CH2 DN");     break;

    /* ── CH3 (ep_idx=2) ── */
    case 0x16: ir_ch_toggle(2);       DBG_LOG("[IR] CH3 toggle\r\n"); oled_ui_notify_ir("CH3 toggle"); break;
    case 0x19: ir_ch_adjust(2, +5);  DBG_LOG("[IR] CH3 UP\r\n");     oled_ui_notify_ir("CH3 UP");     break;
    case 0x0D: ir_ch_adjust(2, -5);  DBG_LOG("[IR] CH3 DN\r\n");     oled_ui_notify_ir("CH3 DN");     break;

    /* ── 효과 ── */
    case 0x0C: xmas_fx_next();          oled_ui_notify_ir(xmas_fx_name(g_xmasFxCfg.effect)); break;
    case 0x08: xmas_fx_prev();          oled_ui_notify_ir(xmas_fx_name(g_xmasFxCfg.effect)); break;
    case 0x1C: xmas_fx_select(XMAS_FX_STEADY); oled_ui_notify_ir("STEADY");                  break;
    case 0x5A: xmas_fx_select(XMAS_FX_AUTO);   oled_ui_notify_ir("AUTO");                    break;
    case 0x18: xmas_fx_speed_step(+1);  oled_ui_notify_ir("SPEED UP");                       break;
    case 0x5E: xmas_fx_speed_step(-1);  oled_ui_notify_ir("SPEED DN");                       break;

    /* ── 전체 (ep_idx=3) ── */
    case 0x42: ir_ch_toggle(PWM_MASTER_IDX); DBG_LOG("[IR] MASTER toggle\r\n"); oled_ui_notify_ir("ALL toggle"); break;
    case 0x52: ir_master_adjust(+5);         DBG_LOG("[IR] MASTER UP\r\n");     oled_ui_notify_ir("ALL UP");     break;
    case 0x4A: ir_master_adjust(-5);         DBG_LOG("[IR] MASTER DN\r\n");     oled_ui_notify_ir("ALL DN");     break;

    default:
        DBG_LOG("[IR] unknown cmd=0x%02x\r\n", (int)cmd);
        break;
    }
}
