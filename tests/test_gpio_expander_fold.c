/* Host logic-level test for the GPIO-expander driver (MCP23017 I2C + MCP23S17
 * SPI). Compiles application/Src/gpio_expander.c on a desktop toolchain against
 * stub i2c.h / spi.h / analog.h / periphery.h (tests/stubs) and fakes both
 * buses, so the pure fold, the I2C + SPI Init/Process/Get paths, CS-pin
 * matching, and the bus-idle gate are exercised without hardware.
 *
 * Build + run (from the FreeJoyX repo root), e.g. with the Qt MinGW gcc:
 *   gcc -std=c11 -I tests/stubs -I application/Inc \
 *       tests/test_gpio_expander_fold.c application/Src/gpio_expander.c \
 *       -o build/test_gpio_expander_fold
 *   ./build/test_gpio_expander_fold      # exit 0 = pass
 */
#include <stdio.h>
#include <string.h>

#include "common_types.h"
#include "common_defines.h"
#include "gpio_expander.h"
#include "periphery.h"

/* ---- faked buses + globals the driver links against ---- */
sensor_t      sensors[MAX_AXIS_NUM];
pin_config_t  pin_config[USED_PINS_NUM];
static GPIO_TypeDef g_cs_port;            /* backing store for a CS pin's ODR */

static uint16_t g_i2c_gpio = 0;           /* canned MCP23017 GPIOA|GPIOB<<8 */
static uint16_t g_spi_gpio = 0;           /* canned MCP23S17 GPIOA|GPIOB<<8 */

int I2C_WriteBlocking(uint8_t a, uint8_t r, uint8_t *d, uint16_t l)
{ (void)a; (void)r; (void)d; (void)l; return 0; }
int I2C_ReadBlocking(uint8_t a, uint8_t r, uint8_t *d, uint16_t l, uint8_t n)
{ (void)a; (void)r; (void)n; if (l >= 2) { d[0]=(uint8_t)g_i2c_gpio; d[1]=(uint8_t)(g_i2c_gpio>>8); } return 0; }

void SPI_FullDuplex_TransmitReceive(uint8_t *tx, uint8_t *rx, uint16_t len, uint8_t mode)
{
    (void)mode;
    /* MCP23S17 read of GPIOA (reg 0x12): opcode has the read bit (bit0) set. */
    if (len >= 4 && (tx[0] & 0x01) && tx[1] == 0x12) {
        rx[2] = (uint8_t)g_spi_gpio;
        rx[3] = (uint8_t)(g_spi_gpio >> 8);
    }
}
uint16_t SPI_RxBytesRemaining(void) { return 0; }   /* transfer "instant" */
void     SPI_AbortTransfer(void) { }

static int fails = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static void busIdle(void)
{ for (int i = 0; i < MAX_AXIS_NUM; i++) { sensors[i].source = -1; sensors[i].rx_complete = 1; sensors[i].tx_complete = 1; } }

int main(void)
{
    busIdle();
    memset(pin_config, 0, sizeof(pin_config));
    uint8_t buf[MAX_BUTTONS_NUM];
    uint8_t pos;

    /* --- pure fold --- */
    memset(buf, 0xAA, sizeof(buf)); pos = 0;
    GpioExp_FoldBits(0x0005, 4, buf, &pos, MAX_BUTTONS_NUM);   /* bits 0,2 */
    CHECK(pos == 4);
    CHECK(buf[0] == 1 && buf[1] == 0 && buf[2] == 1 && buf[3] == 0);

    /* --- I2C (MCP23017) end-to-end --- */
    dev_config_t cfg; memset(&cfg, 0, sizeof(cfg));
    for (int i = 0; i < USED_PINS_NUM; i++) cfg.pins[i] = 0;   /* NOT_USED */
    /* GpioExp_Init only touches a bus whose pins are assigned (so it can't stall
     * on an un-started peripheral). The I2C slot needs an I2C_SCL pin present. */
    cfg.pins[0] = I2C_SCL;
    cfg.pins[1] = I2C_SDA;
    cfg.gpio_expanders[0].type       = GPIO_EXP_MCP23017;
    cfg.gpio_expanders[0].address    = 0x20;
    cfg.gpio_expanders[0].button_cnt = 12;
    GpioExp_Init(&cfg);
    g_i2c_gpio = 0x0A03;                  /* b0,b1,b9,b11 */
    GpioExp_Process(&cfg);
    memset(buf, 0, sizeof(buf)); pos = 0;
    GpioExp_Get(buf, &cfg, &pos);
    CHECK(pos == 12);
    CHECK(buf[0]==1 && buf[1]==1 && buf[2]==0 && buf[9]==1 && buf[10]==0 && buf[11]==1);

    /* --- SPI (MCP23S17) end-to-end: slot 1 is SPI, CS on pin 5 --- */
    cfg.gpio_expanders[1].type       = GPIO_EXP_MCP23S17;
    cfg.gpio_expanders[1].address    = 0;            /* hw subaddr 0 */
    cfg.gpio_expanders[1].button_cnt = 10;
    cfg.pins[3]            = SPI_SCK;     /* SPI bus must be assigned for the SPI slot */
    cfg.pins[4]            = SPI_MOSI;
    cfg.pins[5]            = SPI_GPIO_CS;
    pin_config[5].port    = &g_cs_port;
    pin_config[5].pin     = 0x0001;
    GpioExp_Init(&cfg);                  /* matches CS pin 5 to the SPI slot */
    g_spi_gpio = 0x0102;                 /* GPIOA=0x02 (b1), GPIOB=0x01 (b8) */
    GpioExp_Process(&cfg);
    memset(buf, 0, sizeof(buf)); pos = 0;
    GpioExp_Get(buf, &cfg, &pos);
    /* slot 0 (I2C, 12 buttons) then slot 1 (SPI, 10 buttons) -> 22 total */
    CHECK(pos == 22);
    /* SPI buttons start at offset 12: bit1 set -> idx 13; bit8 set -> idx 20 */
    CHECK(buf[12]==0 && buf[13]==1 && buf[20]==1 && buf[21]==0);

    /* --- bus busy: Process must not touch either bus --- */
    g_i2c_gpio = 0xFFFF; g_spi_gpio = 0xFFFF;
    sensors[0].source = (int8_t)SOURCE_I2C; sensors[0].rx_complete = 0;   /* in flight */
    GpioExp_Process(&cfg);               /* should early-return */
    memset(buf, 0, sizeof(buf)); pos = 0;
    GpioExp_Get(buf, &cfg, &pos);
    CHECK(buf[2] == 0 && buf[12] == 0);  /* still the old 0x0A03 / 0x0102 values */

    /* --- bus pins absent: a configured expander whose bus was never assigned
       must stay un-present (no peripheral poke), so it folds nothing. --- */
    busIdle();
    dev_config_t nobus; memset(&nobus, 0, sizeof(nobus));
    for (int i = 0; i < USED_PINS_NUM; i++) nobus.pins[i] = 0;   /* no I2C_SCL / SPI_SCK */
    nobus.gpio_expanders[0].type       = GPIO_EXP_MCP23017;
    nobus.gpio_expanders[0].address    = 0x20;
    nobus.gpio_expanders[0].button_cnt = 12;
    g_i2c_gpio = 0xFFFF;
    GpioExp_Init(&nobus);
    GpioExp_Process(&nobus);
    memset(buf, 0, sizeof(buf)); pos = 0;
    GpioExp_Get(buf, &nobus, &pos);
    CHECK(pos == 0);                     /* chip not present -> nothing folded */

    printf(fails ? "\n%d CHECK(S) FAILED\n" : "ALL GPIO-EXPANDER TESTS PASSED\n", fails);
    return fails ? 1 : 0;
}
