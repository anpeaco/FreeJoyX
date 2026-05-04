/**
  ******************************************************************************
  * @file           : board_uart.c
  * @brief          : F411 BlackPill UART driver (LL_USART + DMA2 Stream 7).
  *
  * simhub.c links against UART_Start / UART_WriteNonBlocking / gen_crc16
  * declared in application/Inc/uart.h. F103 impl at
  * board/f103_bluepill/Src/board_uart.c (StdPeriph + DMA1 Channel 4).
  *
  * F411 routes:
  *   USART1_TX -> PA9 (slot 9), AF7
  *   DMA       -> DMA2 Stream 7 Channel 4 (RM0383 Table 28)
  *   IRQ       -> DMA2_Stream7_IRQn
  *
  * PA9 mutex: TIM1_CH2 (Encoder 1 B) on AF1 vs USART1_TX on AF7. The
  * configurator's pin-role validator already blocks selecting both on
  * the same pin, so application code can call UART_Start unconditionally
  * when SimHub is enabled.
  *
  * gen_crc16 is chip-agnostic and provided in full here so simhub builds
  * without dragging in board/f103_bluepill code on F411.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_dma.h"

#include "board_pins.h"
#include "uart.h"

#define UART_DMA_TX_STREAM   LL_DMA_STREAM_7
#define UART_DMA_TX_CHANNEL  LL_DMA_CHANNEL_4
#define UART_DMA_TX_IRQn     DMA2_Stream7_IRQn

void UART_Start(void)
{
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_USART1);

	/* PA9 (slot 9) AF7 -> USART1_TX. */
	Board_PinSetMode(9, BOARD_GPIO_AF_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);
	Board_PinSetAfRole(9, BOARD_AF_ROLE_UART_TX);

	/* Direct register access -- LL_USART driver isn't vendored on this
	 * branch and the USART1 setup is small enough that vendoring it would
	 * be more code than this. USART1 lives on APB2 = 96 MHz.
	 *
	 * BRR calc (16x oversampling, OVER8=0):
	 *   USART_DIV = 96_000_000 / (16 * 115_200) = 52.083333...
	 *   DIV_MANTISSA = 52 (= 0x34)
	 *   DIV_FRACTION = round(0.083333 * 16) = 1
	 *   BRR = (52 << 4) | 1 = 0x341
	 *
	 * Verify: actual baud = 96_000_000 / (16 * (52 + 1/16))
	 *                     = 96_000_000 / 833 = 115246 baud
	 *   error vs target 115200 = 0.04%, well inside USART tolerance. */
	USART1->BRR = ((52U << 4) | 1U);
	/* CR1 stays at reset (default 8N1, no parity); enable TE + UE. */
	USART1->CR1 = USART_CR1_TE | USART_CR1_UE;
	USART1->CR2 = 0;
	USART1->CR3 = 0;
}

void UART_WriteNonBlocking(uint8_t * data, uint16_t length)
{
	LL_DMA_DisableStream(DMA2, UART_DMA_TX_STREAM);

	LL_DMA_InitTypeDef dma = {0};
	dma.PeriphOrM2MSrcAddress  = (uint32_t)&USART1->DR;
	dma.MemoryOrM2MDstAddress  = (uint32_t)data;
	dma.Direction              = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
	dma.Mode                   = LL_DMA_MODE_NORMAL;
	dma.PeriphOrM2MSrcIncMode  = LL_DMA_PERIPH_NOINCREMENT;
	dma.MemoryOrM2MDstIncMode  = LL_DMA_MEMORY_INCREMENT;
	dma.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
	dma.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
	dma.NbData                 = length;
	dma.Channel                = UART_DMA_TX_CHANNEL;
	/* F103 sets VeryHigh -- preserved here so the simhub telemetry burst
	 * preempts other DMA traffic if they coincide. */
	dma.Priority               = LL_DMA_PRIORITY_VERYHIGH;
	dma.FIFOMode               = LL_DMA_FIFOMODE_DISABLE;
	dma.FIFOThreshold          = LL_DMA_FIFOTHRESHOLD_1_4;
	dma.MemBurst               = LL_DMA_MBURST_SINGLE;
	dma.PeriphBurst            = LL_DMA_PBURST_SINGLE;
	LL_DMA_Init(DMA2, UART_DMA_TX_STREAM, &dma);

	LL_DMA_EnableIT_TC(DMA2, UART_DMA_TX_STREAM);
	NVIC_SetPriority(UART_DMA_TX_IRQn, 2);
	NVIC_EnableIRQ(UART_DMA_TX_IRQn);

	LL_DMA_EnableStream(DMA2, UART_DMA_TX_STREAM);
	/* DMA TX request enable -- routes USART1's TXE event to the DMA stream. */
	USART1->CR3 |= USART_CR3_DMAT;
}

uint16_t gen_crc16(const uint8_t *data, uint16_t size)
{
	uint16_t out = 0, crc = 0;
	int32_t bits_read = 0, bit_flag = 0, i = 0;
	int32_t j = 0x0001;

	if (data == NULL) return 0;

	while (size > 0)
	{
		bit_flag = out >> 15;
		out <<= 1;
		out |= (*data >> bits_read) & 1;

		bits_read++;
		if (bits_read > 7)
		{
			bits_read = 0;
			data++;
			size --;
		}

		if (bit_flag) out ^= CRC16;
	}

	for (i = 0; i < 16; ++i)
	{
		bit_flag = out >> 15;
		out <<= 1;
		if (bit_flag) out ^= CRC16;
	}

	i = 0x8000;
	for (; i !=0; i >>= 1, j <<= 1)
	{
		if (i & out) crc |= j;
	}

	return crc;
}
