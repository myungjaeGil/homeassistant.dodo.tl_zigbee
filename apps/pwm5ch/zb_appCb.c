/*********************************************************************
 * @file    zb_appCb.c
 * @brief   Zigbee 스택 이벤트 콜백 — PWM 5CH 버전
 *********************************************************************/
#include "tl_common.h"
#include "zb_api.h"
#include "zcl_include.h"
#include "bdb.h"
#include "ota.h"
#include "pwm5ch_monitor.h"
#include "pwm5ch_ctrl.h"
#include "pwm5ch_ep.h"
#include "factory_reset.h"

void zbdemo_factoryResetCb(void)
{
    led_power_set_state(LED_PWR_STATE_RESET);
}
