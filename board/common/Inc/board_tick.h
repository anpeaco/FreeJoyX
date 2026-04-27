/**
  ******************************************************************************
  * @file           : board_tick.h
  * @brief          : Board-agnostic main-tick API.
  *
  * The application uses a single periodic timer interrupt to drive its
  * report loop, sensor sampling, and UART transmission cadence. Which
  * physical timer carries that interrupt is a board concern (TIM2 on
  * F103, TIM2 on F411 -- locked decision in CLAUDE.md).
  *
  * Application code calls Board_TickInit() to start the tick and provides
  * the Board_TickISR() body that runs once per tick. The board layer
  * owns the chip-specific IRQ-handler symbol, the timer registers, and
  * the prescaler/period math.
  ******************************************************************************
  */

#ifndef BOARD_TICK_H_
#define BOARD_TICK_H_

#include <stdint.h>

/* Configure and start the main tick at the requested update rate. Enables
 * the underlying timer's clock, sets prescaler/period, registers the
 * interrupt with the NVIC, and starts the timer.
 *
 * Typical call: Board_TickInit(2000) for 2 kHz / 500 us period (current
 * FreeJoy default). Caller must define Board_TickISR(). */
void Board_TickInit(uint32_t freq_hz);

/* Disable the tick interrupt without tearing down the timer. Used in the
 * DFU jump-to-bootloader path so HID transmission can drain cleanly
 * before USB teardown. */
void Board_TickStop(void);

/* Application-provided. Called from the board's tick IRQ handler once
 * per tick after the chip-specific status check + acknowledge. Must not
 * block; runs at NVIC priority 3 on F103. */
void Board_TickISR(void);

#endif /* BOARD_TICK_H_ */
