/**
  ******************************************************************************
  * @file           : board_sensor_irqs.c
  * @brief          : F411 application-side DMA / I2C IRQ handlers.
  *
  * App-only -- not linked into the bootloader build (which doesn't drive
  * sensors). Each handler clears its DMA flag + disables the stream, then
  * delegates to the shared sensor-chain dispatcher in
  * application/Src/sensor_dispatch.c so F103 + F411 run identical chain
  * logic from the same source.
  *
  * Stream / IRQ map (RM0383 Tables 27 + 28):
  *   DMA2_Stream0_IRQn  -> SPI1_RX  (Channel 3)  -> Sensor_OnSpiRxComplete
  *   DMA2_Stream3_IRQn  -> SPI1_TX  (Channel 3)  -> Sensor_OnSpiTxComplete
  *   DMA2_Stream7_IRQn  -> USART1_TX (Channel 4) -> simhub telemetry only,
  *                                                  no sensor dispatch
  *   DMA1_Stream2_IRQn  -> I2C2_RX  (Channel 7)  -> Sensor_OnI2cRxComplete
  *   DMA1_Stream7_IRQn  -> I2C2_TX  (Channel 7)  -> Sensor_OnI2cTxComplete
  *   I2C2_ER_IRQn       -> bus-error recovery (no sensor dispatch)
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_dma.h"

#include "sensor_dispatch.h"

void DMA2_Stream0_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_TC0(DMA2)) {
		LL_DMA_ClearFlag_TC0(DMA2);
		LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_0);
		Sensor_OnSpiRxComplete();
	}
}

void DMA2_Stream3_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_TC3(DMA2)) {
		LL_DMA_ClearFlag_TC3(DMA2);
		LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_3);
		Sensor_OnSpiTxComplete();
	}
}

void DMA2_Stream7_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_TC7(DMA2)) {
		LL_DMA_ClearFlag_TC7(DMA2);
		LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_7);
		/* USART DMA TX request stays armed across calls -- the next
		 * UART_WriteNonBlocking re-enables the stream. No need to flap
		 * the EnableDMAReq_TX bit. simhub doesn't chain across bursts. */
	}
}

void DMA1_Stream2_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_TC2(DMA1)) {
		LL_DMA_ClearFlag_TC2(DMA1);
		LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_2);
		I2C2->CR2 &= ~I2C_CR2_DMAEN;
		Sensor_OnI2cRxComplete();
	}
}

void DMA1_Stream7_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_TC7(DMA1)) {
		LL_DMA_ClearFlag_TC7(DMA1);
		LL_DMA_DisableStream(DMA1, LL_DMA_STREAM_7);
		I2C2->CR2 &= ~I2C_CR2_DMAEN;
		Sensor_OnI2cTxComplete();
	}
}

/* I2C2 error interrupt -- AF / BUS / TIMEOUT / ARLO. F103 enables this
 * but the application doesn't drive any error recovery from the IRQ
 * (it relies on the SWRST + reinit path inside the per-transfer entry
 * points). Minimal body: clear the latched error bits so the ER IRQ
 * stops re-firing. */
void I2C2_ER_IRQHandler(void)
{
	I2C2->SR1 &= ~(I2C_SR1_BERR | I2C_SR1_ARLO | I2C_SR1_AF |
	               I2C_SR1_OVR  | I2C_SR1_PECERR | I2C_SR1_TIMEOUT |
	               I2C_SR1_SMBALERT);
}
