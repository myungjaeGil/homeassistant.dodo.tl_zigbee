/********************************************************************************************************
 * @file    zcl_colorCtrlCb.c
 *
 * @brief   Color Control cluster callback / stub
 *
 * Modified for ZT3L: Color Control 미사용 프로젝트
 *
 * [수정 내용]
 *  - zcl_lightColorCtrl_move2hue / move2saturation /
 *    move2color / move2colorTemperature 스텁 추가
 *    → zbhci_zclHandler.c 링커 에러 해소
 *    → Color Control 미사용이므로 ZCL_STA_UNSUP_CLUSTER_COMMAND 반환
 *
 *******************************************************************************************************/
#include "tl_common.h"
#include "zcl_include.h"
/* 헤더 경로: -I../../../zigbee/zcl 기준 서브디렉토리 */
#include "light_color_control/zcl_light_colorCtrl.h"

/*********************************************************************
 * Color Control 스텁 함수
 * zbhci_zclHandler.c 링커 에러 해소용 — 실제 동작 없음
 *********************************************************************/

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
