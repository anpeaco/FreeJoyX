/**
  ******************************************************************************
  * @file           : board_clock.h
  * @brief          : Board-agnostic early clock/core bring-up.
  *
  * The application calls Board_ClockInit() as the very first thing in main(),
  * before vector relocation or SysTick, so that by the time any timing code
  * runs the core clock is configured and SystemCoreClock is correct.
  *
  * The two boards differ in WHO configures the clock:
  *
  *   F103 (StdPeriph): the CMSIS SystemInit() already calls SetSysClock()
  *     at reset -- it sets the 72 MHz PLL, flash latency, and the prefetch
  *     buffer before main() runs. So Board_ClockInit() is a no-op; the app
  *     is self-sufficient out of the box.
  *
  *   F411 (Cube/LL): the modern CMSIS SystemInit() only sets FPU + VTOR and
  *     deliberately leaves clock setup to user code. Without an explicit
  *     call the app inherits the PLL/latency the bootloader happened to set
  *     and runs with SystemCoreClock stuck at the 16 MHz reset default --
  *     which breaks Delay_us/Delay_ms and dies entirely on a no-bootloader
  *     (cold SWD) boot. Board_ClockInit() runs the real PLL + flash + ART +
  *     NVIC-grouping setup so the F411 app is self-sufficient like F103.
  ******************************************************************************
  */

#ifndef BOARD_CLOCK_H_
#define BOARD_CLOCK_H_

/* Configure the system clock, flash latency/accelerator, and NVIC priority
 * grouping so the application is self-sufficient regardless of how it was
 * launched. Idempotent -- safe to call even if a bootloader already set the
 * clock. No-op on F103 (CMSIS SystemInit already did this at reset). */
void Board_ClockInit(void);

#endif /* BOARD_CLOCK_H_ */
