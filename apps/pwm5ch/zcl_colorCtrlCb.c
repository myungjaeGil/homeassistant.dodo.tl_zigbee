/*********************************************************************
 * @file    zcl_colorCtrlCb.c
 * @brief   Color Control cluster stub — PWM 5ch 프로젝트 미사용
 *********************************************************************/
#include "tl_common.h"
#include "zcl_include.h"
#include "light_color_control/zcl_light_colorCtrl.h"

status_t zcl_lightColorCtrl_move2hue(u8 srcEp, epInfo_t *pDstEpInfo,
                                     u8 disableDefaultRsp, u8 seqNo,
                                     zcl_colorCtrlMoveToHueCmd_t *pMove2Hue)
{
    (void)srcEp; (void)pDstEpInfo;
    (void)disableDefaultRsp; (void)seqNo; (void)pMove2Hue;
    return ZCL_STA_UNSUP_CLUSTER_COMMAND;
}

status_t zcl_lightColorCtrl_move2saturation(u8 srcEp, epInfo_t *pDstEpInfo,
                                            u8 disableDefaultRsp, u8 seqNo,
                                            zcl_colorCtrlMoveToSaturationCmd_t *pMove2Sat)
{
    (void)srcEp; (void)pDstEpInfo;
    (void)disableDefaultRsp; (void)seqNo; (void)pMove2Sat;
    return ZCL_STA_UNSUP_CLUSTER_COMMAND;
}

status_t zcl_lightColorCtrl_move2color(u8 srcEp, epInfo_t *pDstEpInfo,
                                       u8 disableDefaultRsp, u8 seqNo,
                                       zcl_colorCtrlMoveToColorCmd_t *pMove2Color)
{
    (void)srcEp; (void)pDstEpInfo;
    (void)disableDefaultRsp; (void)seqNo; (void)pMove2Color;
    return ZCL_STA_UNSUP_CLUSTER_COMMAND;
}

status_t zcl_lightColorCtrl_move2colorTemperature(u8 srcEp, epInfo_t *pDstEpInfo,
                                                  u8 disableDefaultRsp, u8 seqNo,
                                                  zcl_colorCtrlMoveToColorTemperatureCmd_t *pMove2ColorTemp)
{
    (void)srcEp; (void)pDstEpInfo;
    (void)disableDefaultRsp; (void)seqNo; (void)pMove2ColorTemp;
    return ZCL_STA_UNSUP_CLUSTER_COMMAND;
}
