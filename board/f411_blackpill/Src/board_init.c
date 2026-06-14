/**
  ******************************************************************************
  * @file           : board_init.c
  * @brief          : F411 BlackPill clock + bringup helpers.
  *
  * Clock tree for FreeJoyX on the WeAct BlackPill V3.x:
  *   HSE     25 MHz crystal
  *   PLL_M   25      (VCO input = 25/25 = 1 MHz, in [1..2] MHz recommended)
  *   PLL_N   192     (VCO output = 1 x 192 = 192 MHz, in [100..432] MHz)
  *   PLL_P   2       (SYSCLK = 192/2 = 96 MHz)
  *   PLL_Q   4       (USB OTG FS clock = 192/4 = 48 MHz, exact)
  *   AHB     /1      (HCLK = 96 MHz)
  *   APB1    /2      (PCLK1 = 48 MHz, max 50)
  *   APB2    /1      (PCLK2 = 96 MHz, max 100)
  *
  * Power scale 1 (LL_PWR_REGU_VOLTAGE_SCALE1) is required for HCLK > 84 MHz.
  * Flash wait states = 3 for 90 < HCLK <= 100 MHz (RM0383 Table 5).
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_pwr.h"
#include "stm32f4xx_ll_rcc.h"
#include "stm32f4xx_ll_system.h"
#include "stm32f4xx_ll_utils.h"

#include "board_init.h"
#include "board_clock.h"

void Board_ClockInit_F411(void)
{
	/* Power scaling first -- has to be at scale 1 to allow 96 MHz HCLK. */
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
	LL_PWR_SetRegulVoltageScaling(LL_PWR_REGU_VOLTAGE_SCALE1);

	/* Flash latency for 96 MHz @ VOS scale 1: 3 wait states. */
	LL_FLASH_SetLatency(LL_FLASH_LATENCY_3);
	while (LL_FLASH_GetLatency() != LL_FLASH_LATENCY_3) { }

	/* ART accelerator: enable the instruction cache, data cache and prefetch
	 * so the 3 flash wait states are hidden. The F103 StdPeriph SetSysClock
	 * enables the F1 prefetch buffer automatically; the F4 CMSIS leaves this
	 * to user code, so without it the core stalls ~3 cycles per uncached
	 * fetch and runs at a fraction of its real throughput. Enable BEFORE the
	 * SYSCLK switch to PLL so the faster clock lands with caches already hot.
	 * Shared by app + bootloader (both call Board_ClockInit_F411). */
	LL_FLASH_EnableInstCache();
	LL_FLASH_EnableDataCache();
	LL_FLASH_EnablePrefetch();

	/* HSE on the BlackPill V3.x is a 25 MHz crystal (no bypass). */
	LL_RCC_HSE_Enable();
	while (LL_RCC_HSE_IsReady() != 1) { }

	/* PLL: HSE 25 -> M=25 -> 1 MHz -> N=192 -> 192 MHz VCO -> P=2 -> 96 MHz SYSCLK. */
	LL_RCC_PLL_ConfigDomain_SYS(LL_RCC_PLLSOURCE_HSE,
	                            LL_RCC_PLLM_DIV_25,
	                            192,
	                            LL_RCC_PLLP_DIV_2);
	/* PLL_Q = 4 -> 48 MHz for USB OTG FS. */
	LL_RCC_PLL_ConfigDomain_48M(LL_RCC_PLLSOURCE_HSE,
	                            LL_RCC_PLLM_DIV_25,
	                            192,
	                            LL_RCC_PLLQ_DIV_4);

	LL_RCC_PLL_Enable();
	while (LL_RCC_PLL_IsReady() != 1) { }

	/* Bus prescalers BEFORE switching SYSCLK so the buses don't briefly
	 * overshoot their 50/100 MHz limits at the transition. */
	LL_RCC_SetAHBPrescaler(LL_RCC_SYSCLK_DIV_1);
	LL_RCC_SetAPB1Prescaler(LL_RCC_APB1_DIV_2);
	LL_RCC_SetAPB2Prescaler(LL_RCC_APB2_DIV_1);

	LL_RCC_SetSysClkSource(LL_RCC_SYS_CLKSOURCE_PLL);
	while (LL_RCC_GetSysClkSource() != LL_RCC_SYS_CLKSOURCE_STATUS_PLL) { }

	LL_SetSystemCoreClock(96000000);
}

/* Board-agnostic early init seam (board_clock.h). The application calls this
 * as the first line of main() so the F411 app is self-sufficient regardless
 * of how it was launched -- it no longer relies on the bootloader having
 * configured the clock/latency/grouping (those are core state that survives
 * the jump but are NOT set on a cold SWD boot, and SystemCoreClock is the
 * app's own variable that the bootloader's copy never propagates into). */
void Board_ClockInit(void)
{
	Board_ClockInit_F411();

	/* NVIC priority grouping: the HAL pattern sets this in HAL_Init() (which
	 * the app does not call) to group 4 = all 4 implemented bits used for
	 * preemption, 0 for sub-priority. The app's NVIC_SetPriority(x, 2/3)
	 * calls assume that split; set it explicitly so the grouping is correct
	 * even on a cold boot rather than inherited from the bootloader. */
	NVIC_SetPriorityGrouping(3U);
}
