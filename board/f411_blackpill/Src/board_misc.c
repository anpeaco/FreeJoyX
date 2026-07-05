/**
  ******************************************************************************
  * @file           : board_misc.c
  * @brief          : F411 BlackPill implementations of shared application helpers.
  *
  * Ported from F103's board_misc.c (same intent, LL API). F411 uses the same
  * peripheral instances as F103 (SPI1, I2C2, TIM1/3/4) -- only the bus grouping
  * and driver API differ.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "common_types.h"
#include "board_misc.h"

/* Delay_us lives in application/Src/periphery.c; forward-declare to avoid
 * pulling the full periphery header surface in here. */
extern void Delay_us(uint32_t nTime);

void Board_VersionMismatchBlink(void)
{
	/* On-device signal that a config Write was rejected for a stale
	 * firmware_version -- the F411 equivalent of F103's PB12/PC13 blink. Blinks
	 * the BlackPill's onboard LED (PC13, already an output from board_init) 6x.
	 * Re-init the pin defensively in case this ever runs before board_init. */
	LL_GPIO_InitTypeDef gpio = {0};

	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOC);
	gpio.Pin        = LL_GPIO_PIN_13;
	gpio.Mode       = LL_GPIO_MODE_OUTPUT;
	gpio.Speed      = LL_GPIO_SPEED_FREQ_LOW;
	gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	gpio.Pull       = LL_GPIO_PULL_NO;
	LL_GPIO_Init(GPIOC, &gpio);

	for (uint8_t i = 0; i < 6; i++) {
		LL_GPIO_TogglePin(GPIOC, LL_GPIO_PIN_13);
		Delay_us(200000);
	}
}
