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
}

void OTG_FS_IRQHandler(void)
{
	HAL_PCD_IRQHandler(&hpcd);
}
