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
