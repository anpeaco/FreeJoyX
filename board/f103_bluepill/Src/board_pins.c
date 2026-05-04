/**
  ******************************************************************************
  * @file           : board_pins.c
  * @brief          : F103 BluePill pin map + GPIO mode helper.
  *
  * Maps each USED_PINS_NUM-indexed slot in dev_config_t.pins[] to its
  * physical GPIO port + pin + bit position, plus a capability bitmask
  * (PIN_CAP_*) describing which AF roles that pin supports on this board.
  * Application code references this table via the extern declaration in
  * application/Inc/periphery.h. The F411 port (Phase 5) provides its own
  * version of this table at board/f411_blackpill/Src/board_pins.c with
  * the same shape but the F411 pin assignments and AF mapping (see
  * F411_PORT_PLAN.md "Pin map" section).
  *
  * Pin slot allocations are part of the wire format (configurator and
  * firmware agree on the slot index for each role). Don't reorder.
  *
  * Caps live in this file (not the wire format) -- they describe board
  * hardware, not user config. Each board sets its own caps column.
  ******************************************************************************
  */

#include "periphery.h"
#include "board_pins.h"

pin_config_t pin_config[USED_PINS_NUM] =
{
	{GPIOA, GPIO_Pin_0,  0,  0},														// 0
	{GPIOA, GPIO_Pin_1,  1,  0},														// 1
	{GPIOA, GPIO_Pin_2,  2,  0},														// 2
	{GPIOA, GPIO_Pin_3,  3,  0},														// 3
	{GPIOA, GPIO_Pin_4,  4,  0},														// 4
	{GPIOA, GPIO_Pin_5,  5,  0},														// 5
	{GPIOA, GPIO_Pin_6,  6,  0},														// 6
	{GPIOA, GPIO_Pin_7,  7,  0},														// 7
	{GPIOA, GPIO_Pin_8,  8,  PIN_CAP_FAST_ENCODER},							// 8  -- TIM1_CH1
	{GPIOA, GPIO_Pin_9,  9,  PIN_CAP_FAST_ENCODER | PIN_CAP_UART_TX},	// 9  -- TIM1_CH2 / USART1_TX
	{GPIOA, GPIO_Pin_10, 10, PIN_CAP_LED_RGB},									// 10 -- PA10 RGB driver
	{GPIOA, GPIO_Pin_15, 15, 0},														// 11
	{GPIOB, GPIO_Pin_0,  0,  0},														// 12
	{GPIOB, GPIO_Pin_1,  1,  0},														// 13
	{GPIOB, GPIO_Pin_3,  3,  PIN_CAP_SPI_SCK},									// 14 -- SPI1_SCK (after JTAG remap)
	{GPIOB, GPIO_Pin_4,  4,  PIN_CAP_SPI_MISO},									// 15 -- SPI1_MISO (after JTAG remap)
	{GPIOB, GPIO_Pin_5,  5,  PIN_CAP_SPI_MOSI},									// 16 -- SPI1_MOSI
	{GPIOB, GPIO_Pin_6,  6,  PIN_CAP_FAST_ENCODER | PIN_CAP_TLE5011_GEN},	// 17 -- TIM4_CH1 / TLE5011 4MHz clock
	{GPIOB, GPIO_Pin_7,  7,  PIN_CAP_FAST_ENCODER},							// 18 -- TIM4_CH2
	{GPIOB, GPIO_Pin_8,  8,  0},														// 19
	{GPIOB, GPIO_Pin_9,  9,  0},														// 20
	{GPIOB, GPIO_Pin_10, 10, PIN_CAP_I2C_SCL},									// 21 -- I2C2_SCL
	{GPIOB, GPIO_Pin_11, 11, PIN_CAP_I2C_SDA},									// 22 -- I2C2_SDA
	{GPIOB, GPIO_Pin_12, 12, 0},														// 23
	{GPIOB, GPIO_Pin_13, 13, 0},														// 24
	{GPIOB, GPIO_Pin_14, 14, 0},														// 25
	{GPIOB, GPIO_Pin_15, 15, 0},														// 26
	{GPIOC, GPIO_Pin_13, 13, 0},														// 27
	{GPIOC, GPIO_Pin_14, 14, 0},														// 28
	{GPIOC, GPIO_Pin_15, 15, 0},														// 29
};

void Board_PinSetMode(uint8_t pin_idx, board_gpio_mode_t mode, board_gpio_speed_t speed)
{
	GPIO_InitTypeDef gpio_init = {0};

	if (pin_idx >= USED_PINS_NUM) return;

	gpio_init.GPIO_Pin = pin_config[pin_idx].pin;

	switch (mode)
	{
		case BOARD_GPIO_INPUT_PULLUP:     gpio_init.GPIO_Mode = GPIO_Mode_IPU;          break;
		case BOARD_GPIO_INPUT_PULLDOWN:   gpio_init.GPIO_Mode = GPIO_Mode_IPD;          break;
		case BOARD_GPIO_INPUT_FLOATING:   gpio_init.GPIO_Mode = GPIO_Mode_IN_FLOATING;  break;
		case BOARD_GPIO_INPUT_ANALOG:     gpio_init.GPIO_Mode = GPIO_Mode_AIN;          break;
		case BOARD_GPIO_OUTPUT_PUSHPULL:  gpio_init.GPIO_Mode = GPIO_Mode_Out_PP;       break;
		case BOARD_GPIO_OUTPUT_OPENDRAIN: gpio_init.GPIO_Mode = GPIO_Mode_Out_OD;       break;
		case BOARD_GPIO_AF_PUSHPULL:      gpio_init.GPIO_Mode = GPIO_Mode_AF_PP;        break;
		case BOARD_GPIO_AF_OPENDRAIN:     gpio_init.GPIO_Mode = GPIO_Mode_AF_OD;        break;
	}

	switch (speed)
	{
		case BOARD_GPIO_SPEED_2MHZ:  gpio_init.GPIO_Speed = GPIO_Speed_2MHz;  break;
		case BOARD_GPIO_SPEED_10MHZ: gpio_init.GPIO_Speed = GPIO_Speed_10MHz; break;
		case BOARD_GPIO_SPEED_50MHZ: gpio_init.GPIO_Speed = GPIO_Speed_50MHz; break;
	}

	GPIO_Init(pin_config[pin_idx].port, &gpio_init);
}

uint8_t Board_PinRead(uint8_t pin_idx)
{
	if (pin_idx >= USED_PINS_NUM) return 0;
	return GPIO_ReadInputDataBit(pin_config[pin_idx].port, pin_config[pin_idx].pin);
}

void Board_PinWrite(uint8_t pin_idx, uint8_t high)
{
	if (pin_idx >= USED_PINS_NUM) return;
	GPIO_WriteBit(pin_config[pin_idx].port, pin_config[pin_idx].pin, high ? Bit_SET : Bit_RESET);
}

void Board_TLE5011_BusDir(board_tle5011_bus_dir_t dir)
{
	GPIO_InitTypeDef gpio_init = {0};
	gpio_init.GPIO_Speed = GPIO_Speed_50MHz;
	gpio_init.GPIO_Pin   = GPIO_Pin_5;
	gpio_init.GPIO_Mode  = (dir == BOARD_TLE5011_BUS_DIR_RX) ? GPIO_Mode_AF_OD : GPIO_Mode_AF_PP;
	GPIO_Init(GPIOB, &gpio_init);
}

void Board_PinSetAfRole(uint8_t pin_idx, board_af_role_t role)
{
	/* No-op on F103: StdPeriph routes alternate functions per-peripheral
	 * (e.g. SPI1's PB3..5 routing comes from the GPIO_Remap_SPI1 call in
	 * SPI_Start, USART1_TX from the periphery default mapping). The pin's
	 * MODE is already ALTERNATE (the caller of this helper sets that via
	 * Board_PinSetMode); StdPeriph wires the rest. F411 needs the
	 * per-pin AF nibble set explicitly -- this helper is the cross-board
	 * seam that lets IO_Init / driver bringups stay branch-free. */
	(void)pin_idx;
	(void)role;
}
