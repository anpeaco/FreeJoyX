/**
  ******************************************************************************
  * @file           : stm32f4xx_it.c
  * @brief          : F411 BlackPill interrupt vector bodies.
  *
  * Minimal stubs for Phase 2 (blinky). Cortex-M exception handlers loop
  * forever on a fault so a debugger can locate the cause. Peripheral IRQs
  * (TIM, SPI, I2C, USB, etc.) are added as the matching peripherals get
  * wired in subsequent phases.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_dma.h"

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

/* SPI1 RX-complete (DMA2 Stream 0 Channel 3). Currently clears the flag
 * + disables the stream so SPI_RxBytesRemaining returns 0 on completion;
 * the full sensor-dispatch chain (TLE5011_StopDMA / MCP320x_StopDMA /
 * etc., mirroring the F103 body in stm32f10x_it.c::DMA1_Channel2_IRQHandler)
 * lands in the upcoming sensor_dispatch hoist commit. Until then F411
 * sensor reads complete one transfer at a time without auto-chaining
 * the next sensor in the loop. */
void DMA2_Stream0_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_TC0(DMA2)) {
		LL_DMA_ClearFlag_TC0(DMA2);
		LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_0);
		while (SPI1->SR & SPI_SR_BSY) { }
	}
}

/* SPI1 TX-complete (DMA2 Stream 3 Channel 3). Same caveat as Stream 0 --
 * full TLE5011/TLE5012 chain-into-receive logic from the F103
 * DMA1_Channel3_IRQHandler arrives in the sensor_dispatch hoist. */
void DMA2_Stream3_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_TC3(DMA2)) {
		LL_DMA_ClearFlag_TC3(DMA2);
		LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_3);
		while (!(SPI1->SR & SPI_SR_TXE)) { }
		while (SPI1->SR & SPI_SR_BSY) { }
	}
}
