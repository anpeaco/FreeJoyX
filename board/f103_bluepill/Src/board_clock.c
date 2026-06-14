/**
  ******************************************************************************
  * @file           : board_clock.c
  * @brief          : F103 BluePill -- Board_ClockInit (no-op).
  *
  * On F103 the StdPeriph CMSIS startup runs SystemInit() at reset, which
  * calls SetSysClock() -- configuring the 72 MHz PLL, flash latency, and the
  * flash prefetch buffer -- and sets SystemCoreClock before main() runs. The
  * F103 app is therefore already self-sufficient, so the board-agnostic
  * Board_ClockInit() seam has nothing to do here.
  *
  * (Contrast board/f411_blackpill/Src/board_init.c::Board_ClockInit, which
  * does the real PLL + flash + ART + NVIC-grouping setup because the F411
  * CMSIS SystemInit only sets FPU + VTOR.)
  ******************************************************************************
  */

#include "board_clock.h"

void Board_ClockInit(void)
{
	/* No-op: SystemInit()/SetSysClock() already configured the clock,
	 * flash latency and prefetch buffer at reset. */
}
