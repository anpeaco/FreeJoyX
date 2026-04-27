/**
  ******************************************************************************
  * @file           : board_tick.c
  * @brief          : F103 BluePill main-tick implementation on TIM2.
  *
  * TIM2 was chosen for the main tick on F103 historically and is locked
  * in (CLAUDE.md "Tick location" decision). On F103 with APB1 prescaler /2,
  * TIM2's input clock is 2*PCLK1 = 72 MHz (the silicon timer doubler).
  * The prescaler+period math here preserves the pre-Phase-1.G behaviour
  * bit-for-bit at 2 kHz: PSC = PCLK1/100000 - 1 → 200 kHz counter,
  * ARR = 200000/freq_hz - 1.
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "board_tick.h"

void Board_TickInit(uint32_t freq_hz)
{
	TIM_TimeBaseInitTypeDef tim_init;
	RCC_ClocksTypeDef clocks;

	RCC_GetClocksFreq(&clocks);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

	TIM_TimeBaseStructInit(&tim_init);
	tim_init.TIM_Prescaler = clocks.PCLK1_Frequency / 100000 - 1;
	tim_init.TIM_Period = 200000 / freq_hz - 1;
	tim_init.TIM_ClockDivision = 0;
	tim_init.TIM_CounterMode = TIM_CounterMode_Up;

	TIM_TimeBaseInit(TIM2, &tim_init);
	TIM_ARRPreloadConfig(TIM2, ENABLE);

	TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);
	NVIC_SetPriority(TIM2_IRQn, 3);
	NVIC_EnableIRQ(TIM2_IRQn);

	TIM_Cmd(TIM2, ENABLE);
}

void Board_TickStop(void)
{
	NVIC_DisableIRQ(TIM2_IRQn);
}

void TIM2_IRQHandler(void)
{
	if (TIM_GetITStatus(TIM2, TIM_IT_Update))
	{
		TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
		Board_TickISR();
	}
}
