/**
  ******************************************************************************
  * @file           : board_pins.c
  * @brief          : F411 BlackPill pin map + GPIO mode/read/write helpers (LL).
  *
  * Mirrors the F103 BSP table at board/f103_bluepill/Src/board_pins.c. Same
  * 30-slot wire format (USED_PINS_NUM); same physical pin per slot per the
  * locked F411 pin map (F411_PORT_PLAN.md "Pin map" section). Capability
  * bits differ from F103 only where the F411 alternate-function table
  * differs:
  *   - PA8 / PA9   = TIM1_CH1/CH2 on AF1 (Encoder 1)
  *   - PA9         = USART1_TX on AF7
  *   - PB3 / PB4 / PB5 = SPI1 SCK/MISO/MOSI on AF5
  *   - PB6 / PB7   = TIM4_CH1/CH2 on AF2 (Encoder 2 / TLE5011 GEN)
  *   - PB10 / PB11 = I2C2 SCL/SDA on AF4 (PB11 not bonded on UFQFPN48 -- cap kept for wire-format parity, configurator hides it)
  *
  * Phase 5b scope: pin_config[] table + Board_PinRead/Write so buttons.c
  * compiles. Board_PinSetMode is wired (LL) but currently has no caller in
  * the F411 build (periphery.c is deferred to Phase 5c).
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"

#include "periphery.h"
#include "board_pins.h"

pin_config_t pin_config[USED_PINS_NUM] =
{
	{GPIOA, LL_GPIO_PIN_0,   0,  0},                                                      // 0
	{GPIOA, LL_GPIO_PIN_1,   1,  0},                                                      // 1
	{GPIOA, LL_GPIO_PIN_2,   2,  0},                                                      // 2
	{GPIOA, LL_GPIO_PIN_3,   3,  0},                                                      // 3
	{GPIOA, LL_GPIO_PIN_4,   4,  0},                                                      // 4
	{GPIOA, LL_GPIO_PIN_5,   5,  0},                                                      // 5
	{GPIOA, LL_GPIO_PIN_6,   6,  0},                                                      // 6
	{GPIOA, LL_GPIO_PIN_7,   7,  0},                                                      // 7
	{GPIOA, LL_GPIO_PIN_8,   8,  PIN_CAP_FAST_ENCODER},                                   // 8  -- TIM1_CH1 (AF1)
	{GPIOA, LL_GPIO_PIN_9,   9,  PIN_CAP_FAST_ENCODER | PIN_CAP_UART_TX},                 // 9  -- TIM1_CH2 (AF1) / USART1_TX (AF7)
	{GPIOA, LL_GPIO_PIN_10,  10, PIN_CAP_LED_RGB},                                        // 10 -- PA10 RGB driver
	{GPIOA, LL_GPIO_PIN_15,  15, 0},                                                      // 11
	{GPIOB, LL_GPIO_PIN_0,   0,  0},                                                      // 12
	{GPIOB, LL_GPIO_PIN_1,   1,  0},                                                      // 13
	{GPIOB, LL_GPIO_PIN_3,   3,  PIN_CAP_SPI_SCK},                                        // 14 -- SPI1_SCK (AF5)
	{GPIOB, LL_GPIO_PIN_4,   4,  PIN_CAP_SPI_MISO},                                       // 15 -- SPI1_MISO (AF5)
	{GPIOB, LL_GPIO_PIN_5,   5,  PIN_CAP_SPI_MOSI},                                       // 16 -- SPI1_MOSI (AF5)
	{GPIOB, LL_GPIO_PIN_6,   6,  PIN_CAP_FAST_ENCODER | PIN_CAP_TLE5011_GEN},             // 17 -- TIM4_CH1 (AF2) / TLE5011 GEN
	{GPIOB, LL_GPIO_PIN_7,   7,  PIN_CAP_FAST_ENCODER},                                   // 18 -- TIM4_CH2 (AF2)
	{GPIOB, LL_GPIO_PIN_8,   8,  0},                                                      // 19
	{GPIOB, LL_GPIO_PIN_9,   9,  0},                                                      // 20
	{GPIOB, LL_GPIO_PIN_10,  10, PIN_CAP_I2C_SCL},                                        // 21 -- I2C2_SCL (AF4)
	{GPIOB, LL_GPIO_PIN_2,   2,  0},                                                      // 22 -- PB2 (Phase 7 option B remap; PB11 not bonded on F411 UFQFPN48 so the slot-22 wire-format index points at PB2 on this board only -- BluePill keeps slot 22 = PB11. The configurator's BoardId guard plus the firmware's per-board board_id rejection prevent cross-board config writes from corrupting either pin)
	{GPIOB, LL_GPIO_PIN_12,  12, 0},                                                      // 23
	{GPIOB, LL_GPIO_PIN_13,  13, 0},                                                      // 24
	{GPIOB, LL_GPIO_PIN_14,  14, 0},                                                      // 25
	{GPIOB, LL_GPIO_PIN_15,  15, 0},                                                      // 26
	{GPIOC, LL_GPIO_PIN_13,  13, 0},                                                      // 27
	{GPIOC, LL_GPIO_PIN_14,  14, 0},                                                      // 28 -- LSE-tied unless bridge cut; configurator hides
	{GPIOC, LL_GPIO_PIN_15,  15, 0},                                                      // 29 -- LSE-tied unless bridge cut; configurator hides
};

void Board_PinSetMode(uint8_t pin_idx, board_gpio_mode_t mode, board_gpio_speed_t speed)
{
	if (pin_idx >= USED_PINS_NUM) return;

	/* Idempotent port-clock enable. F103's IO_Init does RCC_APB2PeriphClockCmd
	 * for GPIOA/B/C at the top once; F411 enables per port here so callers
	 * (IO_Init or any board file) don't need to remember. Cheap -- LL just
	 * sets a bit. */
	GPIO_TypeDef * port = pin_config[pin_idx].port;
	if      (port == GPIOA) LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	else if (port == GPIOB) LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	else if (port == GPIOC) LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);

	LL_GPIO_InitTypeDef gpio = {0};
	gpio.Pin = pin_config[pin_idx].pin;

	switch (mode)
	{
		case BOARD_GPIO_INPUT_PULLUP:
			gpio.Mode = LL_GPIO_MODE_INPUT;
			gpio.Pull = LL_GPIO_PULL_UP;
			break;
		case BOARD_GPIO_INPUT_PULLDOWN:
			gpio.Mode = LL_GPIO_MODE_INPUT;
			gpio.Pull = LL_GPIO_PULL_DOWN;
			break;
		case BOARD_GPIO_INPUT_FLOATING:
			gpio.Mode = LL_GPIO_MODE_INPUT;
			gpio.Pull = LL_GPIO_PULL_NO;
			break;
		case BOARD_GPIO_INPUT_ANALOG:
			gpio.Mode = LL_GPIO_MODE_ANALOG;
			gpio.Pull = LL_GPIO_PULL_NO;
			break;
		case BOARD_GPIO_OUTPUT_PUSHPULL:
			gpio.Mode       = LL_GPIO_MODE_OUTPUT;
			gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
			gpio.Pull       = LL_GPIO_PULL_NO;
			break;
		case BOARD_GPIO_OUTPUT_OPENDRAIN:
			gpio.Mode       = LL_GPIO_MODE_OUTPUT;
			gpio.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
			gpio.Pull       = LL_GPIO_PULL_NO;
			break;
		case BOARD_GPIO_AF_PUSHPULL:
			gpio.Mode       = LL_GPIO_MODE_ALTERNATE;
			gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
			gpio.Pull       = LL_GPIO_PULL_NO;
			break;
		case BOARD_GPIO_AF_OPENDRAIN:
			gpio.Mode       = LL_GPIO_MODE_ALTERNATE;
			gpio.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
			gpio.Pull       = LL_GPIO_PULL_NO;
			break;
	}

	switch (speed)
	{
		case BOARD_GPIO_SPEED_2MHZ:  gpio.Speed = LL_GPIO_SPEED_FREQ_LOW;       break;
		case BOARD_GPIO_SPEED_10MHZ: gpio.Speed = LL_GPIO_SPEED_FREQ_MEDIUM;    break;
		case BOARD_GPIO_SPEED_50MHZ: gpio.Speed = LL_GPIO_SPEED_FREQ_VERY_HIGH; break;
	}

	/* AF code is intentionally not set here -- the F411 BSP needs a
	 * Board_PinSetAfRole() helper that maps a (pin, role) pair to the
	 * correct AF1..AF15 code. That helper lands in Phase 5c alongside
	 * the F411 SPI/I2C/USART LL inits. For Phase 5b only buttons.c is
	 * a caller path, and buttons are GPIO input/output -- never AF. */
	LL_GPIO_Init(pin_config[pin_idx].port, &gpio);
}

uint8_t Board_PinRead(uint8_t pin_idx)
{
	if (pin_idx >= USED_PINS_NUM) return 0;
	return LL_GPIO_IsInputPinSet(pin_config[pin_idx].port, pin_config[pin_idx].pin) ? 1 : 0;
}

void Board_PinWrite(uint8_t pin_idx, uint8_t high)
{
	if (pin_idx >= USED_PINS_NUM) return;
	if (high) LL_GPIO_SetOutputPin(pin_config[pin_idx].port, pin_config[pin_idx].pin);
	else      LL_GPIO_ResetOutputPin(pin_config[pin_idx].port, pin_config[pin_idx].pin);
}

void Board_TLE5011_BusDir(board_tle5011_bus_dir_t dir)
{
	/* SPI1 MOSI is PB5, AF5 on F411. Flip OutputType between push-pull
	 * (TX, MCU drives) and open-drain (RX, sensor drives the line, MCU
	 * listens on the same wire). Mode + AF are set once by the F411 SPI
	 * bringup in Phase 5c; here we only touch OTYPER. */
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	if (dir == BOARD_TLE5011_BUS_DIR_RX) {
		LL_GPIO_SetPinOutputType(GPIOB, LL_GPIO_PIN_5, LL_GPIO_OUTPUT_OPENDRAIN);
	} else {
		LL_GPIO_SetPinOutputType(GPIOB, LL_GPIO_PIN_5, LL_GPIO_OUTPUT_PUSHPULL);
	}
}
