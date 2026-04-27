/**
  ******************************************************************************
  * @file           : main_f411.c
  * @brief          : F411 BlackPill standalone blinky entry point (Phase 2).
  *
  * This is the temporary F411 main() while the F411 BSP is being stood
  * up. Phase 5 will replace it with a real entry point that calls the
  * shared application-layer init (buttons, axes, encoders, USB, etc.)
  * once the BSP is feature-complete enough to support it.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "board_init.h"

static void delay_busy(volatile uint32_t loops)
{
	while (loops--) { __asm volatile ("nop"); }
}

int main(void)
{
	Board_ClockInit_F411();
	Board_LedInit_F411();

	for (;;)
	{
		Board_LedToggle_F411();
		delay_busy(2000000);
	}
}
