/*********************************************************************
 * @file    ir_recv_cmd.c
 * @brief   IR 수신 커맨드 → PWM 채널 제어
 *
 * NEC 커맨드 매핑 (리모컨에 맞게 수정):
 *   0x45 : ALL ON    — 전채널 ON
 *   0x46 : ALL OFF   — 전채널 OFF
 *   0x47 : MASTER+   — 마스터 밝기 +10%
 *   0x44 : MASTER-   — 마스터 밝기 -10%
 *   0x40 : CH0 토글
 *   0x19 : CH1 토글
 *   0x0D : CH2 토글
 *   0x16 : CH3 토글
 *   0x0C : CH4 토글
 *   0x18 : MASTER 토글
 *********************************************************************/

#include "tl_common.h"
#include "zb_api.h"
#include "ir_recv.h"
#include "pwm5ch_ep.h"
#include "pwm5ch_ctrl.h"
#include "pwm5ch_monitor.h"

extern void pwm_attrs_save(u8 ep_idx);

/*--------------------------------------------------------------------
 * 내부 헬퍼 — 채널 ON/OFF + PWM 적용 + NV 저장
 *------------------------------------------------------------------*/
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

static void ir_master_adjust(s8 delta_pct)
{
    s16 cur = (s16)g_pwmChAttrs[5].currentLevel;
    s16 step = (s16)(254 * delta_pct / 100);
    cur += step;
    if (cur < 1)   cur = 1;
    if (cur > 254) cur = 254;
    g_pwmChAttrs[5].currentLevel = (u8)cur;
    if (!g_pwmChAttrs[5].onOff) {
        g_pwmChAttrs[5].onOff = TRUE;
    }
    pwm_hw_apply(5);   /* MASTER idx=5 */
    pwm_attrs_save(5);
    printf("[IR] MASTER level=%d\r\n", (int)cur);
}

/*--------------------------------------------------------------------
 * ir_recv_on_cmd — ir_recv.c 에서 호출되는 콜백
 *------------------------------------------------------------------*/
void ir_recv_on_cmd(u8 addr, u8 cmd, bool repeat)
{
    printf("[IR] addr=0x%02X cmd=0x%02X repeat=%d\r\n",
           (int)addr, (int)cmd, (int)repeat);

    /* Repeat는 밝기 조절 커맨드에만 적용 */
    switch (cmd) {

    /* ── 전채널 ── */
    case 0x45:  /* ALL ON */
        ir_ch_set_onoff(0, TRUE);
        ir_ch_set_onoff(1, TRUE);
        ir_ch_set_onoff(2, TRUE);
        ir_ch_set_onoff(3, TRUE);
        ir_ch_set_onoff(4, TRUE);
        ir_ch_set_onoff(5, TRUE);
        printf("[IR] ALL ON\r\n");
        break;

    case 0x46:  /* ALL OFF */
        ir_ch_set_onoff(0, FALSE);
        ir_ch_set_onoff(1, FALSE);
        ir_ch_set_onoff(2, FALSE);
        ir_ch_set_onoff(3, FALSE);
        ir_ch_set_onoff(4, FALSE);
        ir_ch_set_onoff(5, FALSE);
        printf("[IR] ALL OFF\r\n");
        break;

    /* ── 마스터 밝기 ── */
    case 0x47:  /* MASTER + */
        ir_master_adjust(+10);
        break;

    case 0x44:  /* MASTER - */
        ir_master_adjust(-10);
        break;

    /* ── 채널 개별 토글 ── */
    case 0x40:  ir_ch_toggle(0); printf("[IR] CH0 toggle\r\n"); break;
    case 0x19:  ir_ch_toggle(1); printf("[IR] CH1 toggle\r\n"); break;
    case 0x0D:  ir_ch_toggle(2); printf("[IR] CH2 toggle\r\n"); break;
    case 0x16:  ir_ch_toggle(3); printf("[IR] CH3 toggle\r\n"); break;
    case 0x0C:  ir_ch_toggle(4); printf("[IR] CH4 toggle\r\n"); break;
    case 0x18:  ir_ch_toggle(5); printf("[IR] MASTER toggle\r\n"); break;

    default:
        printf("[IR] unknown cmd=0x%02X\r\n", (int)cmd);
        break;
    }
}
