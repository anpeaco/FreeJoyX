/**
  ******************************************************************************
  * @file           : stm32f4xx_it.c
  * @brief          : F411 BlackPill core/USB interrupt vectors.
  *
  * Shared between application and bootloader builds. App-only sensor /
  * peripheral DMA IRQ handlers (DMA2_Stream0/3/7, DMA1_Stream2/7,
  * I2C2_ER) live in board_sensor_irqs.c so the bootloader doesn't drag
  * in application-side dispatch headers.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

extern PCD_HandleTypeDef hpcd;   /* defined in board/f411_blackpill/Src/usbd_conf.c */

/* TimingDelay drives the shared application/Src/periphery.c::Delay_ms
 * busy-wait. Decremented from SysTick_Handler so Delay_ms returns after
 * the requested millisecond count. F103's stm32f10x_it.c does the same
 * thing -- this just mirrors that handler for the F411 build. The
 * application also has Delay_ms(1000) right after Board_USB_Init in
 * main(), which without this would hang the boot before Timers_Init
 * runs and Board_TickISR ever fires.
 *
 * Weak local fallback so the bootloader (which doesn't link
 * application/Src/periphery.c) still resolves the symbol -- in the boot
 * build this lone weak instance stays zero forever and the decrement
 * below is a no-op. In the app build, periphery.c's strong definition
 * wins and Delay_ms works as intended. */
volatile uint32_t TimingDelay __attribute__((weak)) = 0;

void NMI_Handler(void)        { while (1) { } }
void HardFault_Handler(void)  { while (1) { } }
void MemManage_Handler(void)  { while (1) { } }
void BusFault_Handler(void)   { while (1) { } }
void UsageFault_Handler(void) { while (1) { } }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }
void SysTick_Handler(void)
{
	/* HAL_GetTick increments via HAL_IncTick from this handler so HAL_PCD's
	 * busy-wait timeouts (HAL_Delay etc.) actually advance. */
	HAL_IncTick();

	/* Drive the shared application Delay_ms() countdown. Without this the
	 * Delay_ms(1000) after Board_USB_Init in main() hangs forever and the
	 * rest of init never runs -- USB enumerates (USBD_Start happened
	 * before the delay) but no params/joy reports stream out. */
	if (TimingDelay != 0U) {
		TimingDelay--;
	}
}

void OTG_FS_IRQHandler(void)
{
	HAL_PCD_IRQHandler(&hpcd);
}
