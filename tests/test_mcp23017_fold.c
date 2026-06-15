/* Host logic-level test for the MCP23017 button-expander driver (Slice 2 of
 * MCP23017_PLAN.md). Compiles application/Src/mcp23017.c on a desktop toolchain
 * against stub i2c.h / analog.h (tests/stubs) and fakes the I2C bus, so the pure
 * bit-fold, the Init/Process/Get path, and the bus-idle gate are exercised
 * without hardware.
 *
 * Build + run (from the FreeJoyX repo root), e.g. with the Qt MinGW gcc:
 *   gcc -std=c11 -I tests/stubs -I application/Inc \
 *       tests/test_mcp23017_fold.c application/Src/mcp23017.c -o build/test_mcp23017_fold
 *   ./build/test_mcp23017_fold        # exit 0 = pass
 */
#include <stdio.h>
#include <string.h>

#include "common_types.h"
#include "common_defines.h"
#include "mcp23017.h"

/* ---- faked bus + sensor globals the driver links against ---- */
sensor_t sensors[MAX_AXIS_NUM];
static uint16_t g_fake_gpio = 0;

int I2C_WriteBlocking(uint8_t a, uint8_t r, uint8_t *d, uint16_t l)
{ (void)a; (void)r; (void)d; (void)l; return 0; /* ACK */ }

int I2C_ReadBlocking(uint8_t a, uint8_t r, uint8_t *d, uint16_t l, uint8_t n)
{
    (void)a; (void)r; (void)n;
    if (l >= 2) { d[0] = (uint8_t)(g_fake_gpio & 0xFF); d[1] = (uint8_t)(g_fake_gpio >> 8); }
    return 0;
}

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void busIdle(void)
{ for (int i = 0; i < MAX_AXIS_NUM; i++) { sensors[i].source = -1; sensors[i].rx_complete = 1; sensors[i].tx_complete = 1; } }

int main(void)
{
    busIdle();
    uint8_t buf[MAX_BUTTONS_NUM];
    uint8_t pos;

    /* --- pure fold: low button_cnt bits, LSB first --- */
    memset(buf, 0xAA, sizeof(buf)); pos = 0;
    I2cGpio_FoldBits(0x0005, 4, buf, &pos, MAX_BUTTONS_NUM);   /* bits 0 and 2 */
    CHECK(pos == 4);
    CHECK(buf[0] == 1 && buf[1] == 0 && buf[2] == 1 && buf[3] == 0);

    /* button_cnt clamps to 16 */
    pos = 0;
    I2cGpio_FoldBits(0xFFFF, 200, buf, &pos, MAX_BUTTONS_NUM);
    CHECK(pos == 16);

    /* never writes past max */
    pos = MAX_BUTTONS_NUM - 2;
    I2cGpio_FoldBits(0xFFFF, 8, buf, &pos, MAX_BUTTONS_NUM);
    CHECK(pos == MAX_BUTTONS_NUM);

    /* --- end-to-end Init -> Process -> Get with a faked chip --- */
    dev_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    cfg.gpio_expanders[0].address    = 0x20;
    cfg.gpio_expanders[0].button_cnt = 12;
    I2cGpioInit(&cfg);                 /* WriteBlocking ACKs -> slot present */
    g_fake_gpio = 0x0A03;              /* GPIOA=0x03 (b0,b1), GPIOB=0x0A (b9,b11) */
    I2cGpioProcess(&cfg);
    memset(buf, 0, sizeof(buf)); pos = 0;
    I2cGpioGet(buf, &cfg, &pos);
    CHECK(pos == 12);
    CHECK(buf[0] == 1 && buf[1] == 1 && buf[2] == 0);
    CHECK(buf[9] == 1 && buf[10] == 0 && buf[11] == 1);

    /* --- bus busy: Process must not touch the bus (cache holds) --- */
    cfg.gpio_expanders[0].button_cnt = 16;
    sensors[0].source = (int8_t)SOURCE_I2C; sensors[0].rx_complete = 0;  /* DMA in flight */
    g_fake_gpio = 0xFFFF;
    I2cGpioProcess(&cfg);              /* should early-return */
    memset(buf, 0, sizeof(buf)); pos = 0;
    I2cGpioGet(buf, &cfg, &pos);
    CHECK(buf[2] == 0);                /* still the old 0x0A03 bit2, not the 0xFFFF */

    /* --- disabled slot contributes nothing --- */
    busIdle();
    dev_config_t empty; memset(&empty, 0, sizeof(empty));
    I2cGpioInit(&empty);
    pos = 0;
    I2cGpioGet(buf, &empty, &pos);
    CHECK(pos == 0);

    printf(fails ? "\n%d CHECK(S) FAILED\n" : "ALL MCP23017 FOLD TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
