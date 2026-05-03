/**
  ******************************************************************************
  * @file           : board_misc.c
  * @brief          : F103 BluePill helpers extracted from shared application code.
  *
  * Body lifted from application/Src/stm32f10x_it.c (ADC clock gating)
  * and application/Src/usb_endp.c (version-mismatch blink). Phase 4D
  * relocation; behaviour identical.
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "stm32f10x_conf.h"
#include "common_types.h"
#include "board_misc.h"

/* The F103 LED blink loop in the version-mismatch handler used Delay_us
 * but Delay_us lives in application/Src/periphery.c which we already
 * include indirectly. Forward-declare to avoid pulling in the full
 * periphery header surface here. */
extern void Delay_us(uint32_t nTime);

void Board_AdcQuietPeripherals(uint8_t quiet, const app_config_t *cfg)
{
	if (quiet) {
		/* Disable everything that could inject noise onto ADC1. */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, DISABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2 | RCC_APB1Periph_TIM4, DISABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC, DISABLE);

		if (cfg->rgb_cnt == 0 && cfg->fast_encoder_cnt == 0) {
			RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, DISABLE);
		}
		if (cfg->rgb_cnt == 0 && cfg->pwm_cnt == 0) {
			RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, DISABLE);
		}
	} else {
		/* Re-enable for normal operation. The unconditional re-enable
		 * (TIM1 / TIM3 always come back on) matches the original
		 * stm32f10x_it.c body -- if a config disabled them, IO_Init
		 * never enabled them in the first place so the redundant
		 * re-enable is harmless. */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2 | RCC_APB1Periph_TIM3 | RCC_APB1Periph_TIM4, ENABLE);
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_TIM1, ENABLE);
	}
}

void Board_VersionMismatchBlink(void)
{
	/* Lifted from usb_endp.c::EP1_OUT_Callback's REPORT_ID_CONFIG_OUT
	 * version-mismatch refusal path. PB12 and PC13 are the BluePill's
	 * "is something wrong" indicators. */
	GPIO_InitTypeDef GPIO_InitStructure;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_13;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_12;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	for (uint8_t i = 0; i < 6; i++) {
		GPIOB->ODR ^= GPIO_Pin_12;
		GPIOC->ODR ^= GPIO_Pin_13;
		Delay_us(200000);
	}
}
