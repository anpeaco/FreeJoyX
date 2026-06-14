/**
  ******************************************************************************
  * @file           : board_init.h
  * @brief          : F411 BlackPill clock bring-up API.
  *
  * Board_ClockInit_F411 configures the 96 MHz PLL, flash latency + ART, and
  * USB 48 MHz clock. It is called directly by the bootloader and (via the
  * board-agnostic Board_ClockInit() seam in board_clock.h) by the app.
  ******************************************************************************
  */

#ifndef BOARD_INIT_H_
#define BOARD_INIT_H_

void Board_ClockInit_F411(void);

#endif /* BOARD_INIT_H_ */
