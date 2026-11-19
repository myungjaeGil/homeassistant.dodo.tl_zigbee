/**
 * @file  i2c_sw.h
 * @brief Software (bit-bang) I2C — TLSR8258 / ZT3L
 *
 * 핀은 board_8258_zt3l.h 에서 정의:
 *   I2C_SW_SDA_PIN  = GPIO_PC4  (Pin2)
 *   I2C_SW_SCL_PIN  = GPIO_PB7  (Pin15)
 *   I2C_SW_HALF_BIT_US = 2      (~250 kHz)
 */
#pragma once
#include "tl_common.h"

/* board 헤더에서 정의되지 않은 경우 기본값 */
#ifndef I2C_SW_SDA_PIN
#define I2C_SW_SDA_PIN          GPIO_PC4
#endif
#ifndef I2C_SW_SCL_PIN
#define I2C_SW_SCL_PIN          GPIO_PB7
#endif
#ifndef I2C_SW_HALF_BIT_US
#define I2C_SW_HALF_BIT_US      2
#endif

void i2c_sw_init(void);

/**
 * @return 0: OK,  -1: NACK
 */
int  i2c_sw_write(u8 addr7, const u8 *buf, u16 len);
