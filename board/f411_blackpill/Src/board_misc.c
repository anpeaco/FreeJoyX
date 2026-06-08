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

void Board_AdcQuietPeripherals(uint8_t quiet, const app_config_t *cfg)
{
	/* Gate the peripherals that can inject noise onto ADC1 across the sample
	 * window, mirroring F103's board_misc.c. The ADC pins are on GPIOA (never
	 * gated, and USB OTG-FS lives there too), so GPIOB/GPIOC clocks can drop
	 * during the window. TIM2 (the main tick) is APB1 too but deliberately left
	 * running. */
	if (quiet) {
		LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_SPI1);
		LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_I2C2 | LL_APB1_GRP1_PERIPH_TIM4);
		LL_AHB1_GRP1_DisableClock(LL_AHB1_GRP1_PERIPH_GPIOB | LL_AHB1_GRP1_PERIPH_GPIOC);

		if (cfg->rgb_cnt == 0 && cfg->fast_encoder_cnt == 0) {
			LL_APB2_GRP1_DisableClock(LL_APB2_GRP1_PERIPH_TIM1);
		}
		if (cfg->rgb_cnt == 0 && cfg->pwm_cnt == 0) {
			LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_TIM3);
		}
	} else {
		/* Unconditional re-enable (TIM1/TIM3 always come back on) matches F103:
		 * if a config disabled them, IO_Init never configured them in the first
		 * place, so the redundant re-enable is harmless. */
		LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1 | LL_APB2_GRP1_PERIPH_TIM1);
		LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2 | LL_APB1_GRP1_PERIPH_TIM3 |
		                         LL_APB1_GRP1_PERIPH_TIM4);
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB | LL_AHB1_GRP1_PERIPH_GPIOC);
	}
}

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
