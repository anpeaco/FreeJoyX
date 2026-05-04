/**
  ******************************************************************************
  * @file           : board_pins.h
  * @brief          : Board-agnostic pin capabilities + GPIO mode helper API.
  *
  * Capability flags let IO_Init in application/Src/periphery.c dispatch
  * AF-routed pin roles (SPI_SCK, I2C_SDA, FAST_ENCODER, etc.) without
  * hardcoding slot indexes. Each board's board_pins.c populates the
  * .caps column of its pin_config[] table with the bitmask of AF roles
  * that pin is wired to carry.
  *
  * The Board_PinSetMode helper hides the chip-specific GPIO library
  * (StdPeriph on F103, LL or HAL on F411 -- TBD) from application code
  * so IO_Init builds against either driver layer unchanged.
  ******************************************************************************
  */

#ifndef BOARD_PINS_H_
#define BOARD_PINS_H_

#include <stdint.h>

/* Pin role capability bits. A pin's bitmask in pin_config[].caps lists
 * which AF roles the pin is wired to support on the active board. IO_Init
 * checks these instead of hardcoded slot indexes:
 *   was: `pins[i] == SPI_SCK && i == 14`
 *   is:  `pins[i] == SPI_SCK && (pin_config[i].caps & PIN_CAP_SPI_SCK)`.
 *
 * Non-AF roles (BUTTON_*, AXIS_ANALOG, SHIFT_REG_*, LED_SINGLE, LED_PWM,
 * LED_ROW, LED_COLUMN, NOT_USED, generic CS) are universal -- any pin
 * can carry them, so they get no cap bit. */
#define PIN_CAP_SPI_SCK         (1u << 0)
#define PIN_CAP_SPI_MISO        (1u << 1)
#define PIN_CAP_SPI_MOSI        (1u << 2)
#define PIN_CAP_I2C_SCL         (1u << 3)
#define PIN_CAP_I2C_SDA         (1u << 4)
#define PIN_CAP_FAST_ENCODER    (1u << 5)
#define PIN_CAP_TLE5011_GEN     (1u << 6)
#define PIN_CAP_UART_TX         (1u << 7)
#define PIN_CAP_LED_RGB         (1u << 8)

/* Board-agnostic GPIO modes. Each board's Board_PinSetMode impl maps these
 * to the chip's native GPIO_Mode_* / LL_GPIO_MODE_* / HAL_GPIO_Init constants. */
typedef enum {
	BOARD_GPIO_INPUT_PULLUP,
	BOARD_GPIO_INPUT_PULLDOWN,
	BOARD_GPIO_INPUT_FLOATING,
	BOARD_GPIO_INPUT_ANALOG,
	BOARD_GPIO_OUTPUT_PUSHPULL,
	BOARD_GPIO_OUTPUT_OPENDRAIN,
	BOARD_GPIO_AF_PUSHPULL,
	BOARD_GPIO_AF_OPENDRAIN,
} board_gpio_mode_t;

typedef enum {
	BOARD_GPIO_SPEED_2MHZ,
	BOARD_GPIO_SPEED_10MHZ,
	BOARD_GPIO_SPEED_50MHZ,
} board_gpio_speed_t;

/* Configure the pin at slot index `pin_idx` (into pin_config[]) to the given
 * mode + output-speed. The board-specific impl looks up port/pin from the
 * board's pin_config[] table and translates to native GPIO library calls.
 *
 * Caller is responsible for clock-enabling the relevant GPIO port; IO_Init
 * does this once for all ports at startup.
 *
 * Speed is irrelevant for input modes; pass any value (typically
 * BOARD_GPIO_SPEED_10MHZ to mirror previous defaults). */
void Board_PinSetMode(uint8_t pin_idx, board_gpio_mode_t mode, board_gpio_speed_t speed);

/* AF role identifiers used by Board_PinSetAfRole. F103's StdPeriph driver
 * either reads the AF route from the pin map or applies a global remap
 * (e.g. SPI1 on PB3..5 via GPIO_Remap_SPI1) so its impl is a no-op; F411
 * must write the per-pin 4-bit nibble in GPIOx->AFR[0]/[1] from the right
 * AF1..AF15 code, which depends on which peripheral the pin is currently
 * carrying (e.g. PA9 = AF1 for TIM1_CH2, AF7 for USART1_TX).
 *
 * Order is purely UI-stable; new roles append at the end. */
typedef enum {
	BOARD_AF_ROLE_SPI_SCK = 0,
	BOARD_AF_ROLE_SPI_MISO,
	BOARD_AF_ROLE_SPI_MOSI,
	BOARD_AF_ROLE_I2C_SCL,
	BOARD_AF_ROLE_I2C_SDA,
	BOARD_AF_ROLE_FAST_ENCODER,
	BOARD_AF_ROLE_TLE5011_GEN,
	BOARD_AF_ROLE_UART_TX,
	BOARD_AF_ROLE_LED_RGB,
	BOARD_AF_ROLE_LED_PWM,
} board_af_role_t;

/* Routes the pin at slot index `pin_idx` to its AF route for the given role.
 * Caller must have set the pin's MODE to ALTERNATE first via
 * Board_PinSetMode(BOARD_GPIO_AF_PUSHPULL or BOARD_GPIO_AF_OPENDRAIN); this
 * helper only writes the AF code nibble.
 *
 * On boards where the AF code is implicit in the pin selection (F103 with
 * StdPeriph + global remaps), this is a no-op. On boards where the AF
 * nibble is per-pin (F411 with LL), the impl looks up the right AF1..AF15
 * code from a per-board (pin_idx, role) table. */
void Board_PinSetAfRole(uint8_t pin_idx, board_af_role_t role);

/* Read / write a configured GPIO. Both wrap the chip's native idata /
 * odata register accesses (StdPeriph GPIO_ReadInputDataBit / GPIO_WriteBit
 * on F103, LL_GPIO_Is* / SetOutputPin / ResetOutputPin on F411) so
 * application code (buttons.c matrix scan, etc.) stays driver-agnostic.
 *
 * Return value of Board_PinRead is 0 or 1; out-of-range pin_idx returns 0.
 * Board_PinWrite ignores out-of-range pin_idx silently. */
uint8_t Board_PinRead(uint8_t pin_idx);
void    Board_PinWrite(uint8_t pin_idx, uint8_t high);

/* Half-duplex bus-direction control for the SPI1 MOSI line (PB5 on both
 * boards), used by the TLE5011 / TLE5012 sensor drivers to flip between
 * driving the line (TX, push-pull AF) and listening on it (RX, open-drain
 * AF). The driver toggles this halfway through each sample cycle.
 *
 * Belongs here because it's strictly a GPIO-mode operation; Phase 5c may
 * fold this into a board_spi.h alongside the LL SPI driver. */
typedef enum {
	BOARD_TLE5011_BUS_DIR_TX = 0,	/* MCU drives MOSI: AF push-pull */
	BOARD_TLE5011_BUS_DIR_RX = 1,	/* sensor drives line: AF open-drain (MCU listens) */
	/* TLE5012 listen mode: switch MOSI to plain floating input (no AF,
	 * no pull). The TLE5012 sensor briefly tristates the line during
	 * its turnaround between MCU-write and sensor-read; AF open-drain
	 * doesn't release fast enough for that handshake. F103 originally
	 * inlined a GPIO_Init in DMA1_Channel3_IRQHandler for this; lifted
	 * here so the SPI TX-complete dispatcher works cross-board. */
	BOARD_TLE5011_BUS_DIR_LISTEN_FLOATING = 2,
} board_tle5011_bus_dir_t;

void Board_TLE5011_BusDir(board_tle5011_bus_dir_t dir);

#endif /* BOARD_PINS_H_ */
