/**
  ******************************************************************************
  * @file           : board_tick.c
  * @brief          : F411 BlackPill main-tick implementation on TIM2 (LL).
  *
  * F411 clock plan (board_init.c::Board_ClockInit_F411):
  *   SYSCLK    96 MHz   (PLL: HSE 25 / M=25 -> 1 -> N=192 -> 192 / P=2)
  *   AHB       96 MHz
  *   APB1      48 MHz   (TIM2/3/4/5/12/13/14 timers fed by APB1*2 = 96 MHz)
  *   APB2      96 MHz
  *
  * TIM2 lives on APB1 -> 96 MHz timer clock. Same Board_TickInit /
  * Board_TickStop / Board_TickISR contract as F103, just LL flavoured
  * because the F411 driver layer is LL (locked decision in
  * F411_PORT_PLAN.md, 2026-04-27). The 200 kHz intermediate counter is
  * preserved for parity with the F103 implementation in
  * board/f103_bluepill/Src/board_tick.c, so behaviour is byte-equivalent
  * once application/Src/main.c is wired up in Phase 5.
  *
  * Pre-hardware build only -- no BlackPill in hand to verify the actual
  * tick frequency on a scope. Stays compile-clean alongside the existing
  * F103 build.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_tim.h"
#include "board_tick.h"

void Board_TickInit(uint32_t freq_hz)
{
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM2);

	LL_TIM_InitTypeDef tim_init;
	LL_TIM_StructInit(&tim_init);

	/* TIM2 input clock = 96 MHz (APB1 timer-clock with /2 prescaler).
	 * Match F103 convention: prescale to 200 kHz counter, then ARR
	 * divides that down to the requested freq_hz. The 200 kHz
	 * intermediate keeps the same usable freq_hz range as F103
	 * (1 Hz to 100 kHz) and avoids per-board calculations leaking up
	 * into the application layer. */
	tim_init.Prescaler   = (96000000U / 100000U) - 1U;   /* 96 MHz / 480 = 200 kHz */
	tim_init.Autoreload  = (200000U / freq_hz) - 1U;
	tim_init.CounterMode = LL_TIM_COUNTERMODE_UP;
	tim_init.ClockDivision = LL_TIM_CLOCKDIVISION_DIV1;
	tim_init.RepetitionCounter = 0;

	LL_TIM_Init(TIM2, &tim_init);
	LL_TIM_EnableARRPreload(TIM2);

	LL_TIM_EnableIT_UPDATE(TIM2);
	NVIC_SetPriority(TIM2_IRQn, 3);
	NVIC_EnableIRQ(TIM2_IRQn);

	LL_TIM_EnableCounter(TIM2);
}

void Board_TickStop(void)
{
	NVIC_DisableIRQ(TIM2_IRQn);
}

void TIM2_IRQHandler(void)
{
	if (LL_TIM_IsActiveFlag_UPDATE(TIM2))
	{
		LL_TIM_ClearFlag_UPDATE(TIM2);
		Board_TickISR();
	}
}

/* Weak default so the F411 image links pre-Phase 5b: nothing calls
 * Board_TickInit yet (main_f411.c is a standalone blinky), but the
 * TIM2_IRQHandler above references Board_TickISR and the linker resolves
 * it eagerly. The real Board_TickISR ships with the application layer in
 * application/Src/stm32f10x_it.c (which the F411 build will pull in
 * during Phase 5b once the F1-only headers are gated out) and overrides
 * this stub. */
__attribute__((weak)) void Board_TickISR(void)
{
}
