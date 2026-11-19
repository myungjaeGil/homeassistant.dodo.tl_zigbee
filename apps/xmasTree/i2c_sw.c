/**
 * @file  i2c_sw.c
 * @brief Software (bit-bang) I2C — TLSR8258 / ZT3L
 */
#include "tl_common.h"
#include "i2c_sw.h"

/* ── 내부 매크로 ─────────────────────────────────────────── */
#define SDA_HI()   gpio_set_output_en(I2C_SW_SDA_PIN, 0)  /* 오픈드레인: 출력 disable = Hi-Z (풀업) */
#define SDA_LO()   do { gpio_write(I2C_SW_SDA_PIN, 0); gpio_set_output_en(I2C_SW_SDA_PIN, 1); } while(0)
#define SCL_HI()   gpio_set_output_en(I2C_SW_SCL_PIN, 0)
#define SCL_LO()   do { gpio_write(I2C_SW_SCL_PIN, 0); gpio_set_output_en(I2C_SW_SCL_PIN, 1); } while(0)
#define SDA_READ() gpio_read(I2C_SW_SDA_PIN)
#define DLY()      sleep_us(I2C_SW_HALF_BIT_US)

/* ── 초기화 ─────────────────────────────────────────────── */
void i2c_sw_init(void)
{
    /* SDA: PC4 — 오픈드레인 모드 (출력 disable 시 Hi-Z, 외부 풀업 필요) */
    gpio_set_func(I2C_SW_SDA_PIN, AS_GPIO);
    gpio_setup_up_down_resistor(I2C_SW_SDA_PIN, PM_PIN_PULLUP_10K);
    gpio_write(I2C_SW_SDA_PIN, 0);
    SDA_HI();

    /* SCL: PB7 */
    gpio_set_func(I2C_SW_SCL_PIN, AS_GPIO);
    gpio_setup_up_down_resistor(I2C_SW_SCL_PIN, PM_PIN_PULLUP_10K);
    gpio_write(I2C_SW_SCL_PIN, 0);
    SCL_HI();

    DLY(); DLY();
}

/* ── 내부: START / STOP ──────────────────────────────────── */
static void _start(void)
{
    SDA_HI(); DLY();
    SCL_HI(); DLY();
    SDA_LO(); DLY();
    SCL_LO(); DLY();
}

static void _stop(void)
{
    SDA_LO(); DLY();
    SCL_HI(); DLY();
    SDA_HI(); DLY();
}

/* ── 내부: 바이트 전송 → ACK 수신 ───────────────────────── */
static int _write_byte(u8 b)
{
    for (int i = 7; i >= 0; i--) {
        if (b & (1 << i)) { SDA_HI(); } else { SDA_LO(); }
        DLY();
        SCL_HI(); DLY();
        SCL_LO(); DLY();
    }
    /* ACK 수신 */
    SDA_HI();          /* SDA 릴리즈 */
    DLY();
    SCL_HI(); DLY();
    int ack = !SDA_READ();   /* ACK = LOW */
    SCL_LO(); DLY();
    return ack ? 0 : -1;
}

/* ── 공개 API ────────────────────────────────────────────── */
int i2c_sw_write(u8 addr7, const u8 *buf, u16 len)
{
    _start();
    if (_write_byte((addr7 << 1) | 0) != 0) { _stop(); return -1; }  /* addr+W */
    for (u16 i = 0; i < len; i++) {
        if (_write_byte(buf[i]) != 0) { _stop(); return -1; }
    }
    _stop();
    return 0;
}
