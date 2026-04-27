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

#endif /* BOARD_PINS_H_ */
