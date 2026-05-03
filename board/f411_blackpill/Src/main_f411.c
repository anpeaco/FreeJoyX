/**
  ******************************************************************************
  * @file           : main_f411.c
  * @brief          : F411 BlackPill standalone blinky entry point (Phase 2).
  *
  * This is the temporary F411 main() while the F411 BSP is being stood
  * up. Phase 5 will replace it with a real entry point that calls the
  * shared application-layer init (buttons, axes, encoders, USB, etc.)
  * once the BSP is feature-complete enough to support it.
  *
  * Phase 3 adds a single boot-time flash self-test:
  *   1. Unlock flash, erase config sector, write 0xCAFEBABE @ CONFIG_ADDR.
  *   2. Read it back; on match, blink fast (~5 Hz). On mismatch, blink slow
  *      (~1 Hz). On flash error before the read-back, no blink at all.
  * The self-test gives a 5-second sanity check the day the BlackPill arrives:
  * fast blink = ConfigFlash_* path works end-to-end; slow blink = wrote but
  * read-back wrong (likely sector-erase or address mapping bug); dead LED =
  * boot path is broken before the self-test runs. The whole self-test goes
  * away in Phase 6 once the real F411 bootloader is the first flash caller.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "board_init.h"
#include "board_flash.h"
#include "board_config.h"

static void delay_busy(volatile uint32_t loops)
{
	while (loops--) { __asm volatile ("nop"); }
}

#define F411_FLASH_SELFTEST_SENTINEL  0xCAFEBABEU

/* Returns 1 on round-trip success, 0 on mismatch. The 4-byte write at
 * CONFIG_ADDR consumes one word of sector 4; the bootloader / app aren't
 * running yet so there's nothing to clobber. */
static int flash_selftest(void)
{
	ConfigFlash_Unlock();
	ConfigFlash_ErasePage(CONFIG_ADDR);
	ConfigFlash_WriteWord(CONFIG_ADDR, F411_FLASH_SELFTEST_SENTINEL);
	ConfigFlash_Lock();

	const uint32_t readback = *(volatile uint32_t *)CONFIG_ADDR;
	return readback == F411_FLASH_SELFTEST_SENTINEL ? 1 : 0;
}

int main(void)
{
	Board_ClockInit_F411();
	Board_LedInit_F411();

	const int flash_ok = flash_selftest();

	/* Fast blink on success (~5 Hz), slow on mismatch (~1 Hz). Loop
	 * counts are eyeballed against the 96 MHz HSE-PLL system clock. */
	const uint32_t blink_loops = flash_ok ? 400000U : 4000000U;

	for (;;)
	{
		Board_LedToggle_F411();
		delay_busy(blink_loops);
	}
}
