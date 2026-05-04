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

/* USART1 TX-complete via DMA2 Stream 7 Channel 4. simhub telemetry burst
 * lands here -- F103's analogue is DMA1_Channel4 which only clears the
 * flag and disables the channel; matching that minimal behaviour here. */
void DMA2_Stream7_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_TC7(DMA2)) {
		LL_DMA_ClearFlag_TC7(DMA2);
		LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_7);
		/* USART DMA TX request stays armed across calls -- the next
		 * UART_WriteNonBlocking re-enables the stream. No need to flap
		 * the EnableDMAReq_TX bit. */
	}
}

/* I2C2 RX-complete via DMA1 Stream 2 Channel 7. ADS1115 / AS5600
 * conversion-complete handler in F103 lives in
 * stm32f10x_it.c::DMA1_Channel5_IRQHandler -- the body generates STOP,
 * polls for STOP-complete, then chains the next sensor in the loop.
 * That dispatch lands in the upcoming sensor_dispatch hoist; minimal
 * flag-clear here so the polling loops in the application path exit
 * cleanly. */
void DMA1_Stream2_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_TC2(DMA1)) {
		LL_DMA_ClearFlag_TC2(DMA1);
		LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_2);
		I2C2->CR2 &= ~I2C_CR2_DMAEN;
		I2C2->CR1 |= I2C_CR1_STOP;
	}
}

/* I2C2 TX-complete via DMA1 Stream 7 Channel 7. F103's analogue is
 * DMA1_Channel4_IRQHandler which polls BTF + generates STOP +
 * mux-set chain (see sensor_dispatch hoist note above). */
void DMA1_Stream7_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_TC7(DMA1)) {
		LL_DMA_ClearFlag_TC7(DMA1);
		LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_7);
		I2C2->CR2 &= ~I2C_CR2_DMAEN;
	}
}

/* I2C2 error interrupt -- AF / BUS / TIMEOUT / ARLO. F103 enables this
 * but the application doesn't drive any error recovery from the IRQ
 * (it relies on the SWRST + reinit path inside the per-transfer
 * entry points). Minimal body: clear the latched error bits so the
 * ER IRQ stops re-firing. */
void I2C2_ER_IRQHandler(void)
{
	I2C2->SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF |
	               I2C_SR1_OVR  | I2C_SR1_PECERR | I2C_SR1_TIMEOUT |
	               I2C_SR1_SMBALERT);
}
