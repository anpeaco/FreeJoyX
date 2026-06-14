/**
  ******************************************************************************
  * @file           : board_spi.c
  * @brief          : F411 BlackPill SPI driver (LL_SPI + DMA2 streams 0/3).
  *
  * Application's sensor drivers (as5048a, mcp320x, mlx90363, mlx90393,
  * tle5011, tle5012) link against SPI_Start / SPI_HalfDuplex_Transmit /
  * SPI_HalfDuplex_Receive / SPI_FullDuplex_TransmitReceive declared in
  * application/Inc/spi.h. F103 impl is at board/f103_bluepill/Src/board_spi.c
  * (StdPeriph + DMA1 channels 2/3).
  *
  * F411 routes:
  *   SPI1   -> AF5 on PB3 (SCK) / PB4 (MISO) / PB5 (MOSI)
  *   DMA2   -> Stream 0 Channel 3 = SPI1_RX (RM0383 Table 27)
  *           -> Stream 3 Channel 3 = SPI1_TX
  *   IRQs   -> DMA2_Stream0_IRQn (RX complete)
  *           -> DMA2_Stream3_IRQn (TX complete)
  *
  * The DMA RX/TX complete IRQs (board/f411_blackpill/Src/board_sensor_irqs.c)
  * delegate to the shared chain dispatcher in application/Src/sensor_dispatch.c
  * (Sensor_OnSpiRxComplete / Sensor_OnSpiTxComplete), the same body F103's
  * stm32f10x_it.c calls. Both boards run identical sensor-chaining logic --
  * a sensor's completion IRQ kicks off the next sensor in the loop.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_dma.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_spi.h"

#include "board_pins.h"
#include "spi.h"

#define SPI_DMA_RX_STREAM   LL_DMA_STREAM_0
#define SPI_DMA_TX_STREAM   LL_DMA_STREAM_3
#define SPI_DMA_CHANNEL     LL_DMA_CHANNEL_3
#define SPI_DMA_RX_IRQn     DMA2_Stream0_IRQn
#define SPI_DMA_TX_IRQn     DMA2_Stream3_IRQn

void SPI_Start(void)
{
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

	/* PB3 SCK, PB4 MISO, PB5 MOSI -- mode + AF set via the BSP seam.
	 * Slot indices 14/15/16 from board/f411_blackpill/Src/board_pins.c. */
	Board_PinSetMode(14, BOARD_GPIO_AF_PUSHPULL,  BOARD_GPIO_SPEED_50MHZ);
	Board_PinSetAfRole(14, BOARD_AF_ROLE_SPI_SCK);
	Board_PinSetMode(15, BOARD_GPIO_AF_PUSHPULL,  BOARD_GPIO_SPEED_50MHZ);
	Board_PinSetAfRole(15, BOARD_AF_ROLE_SPI_MISO);
	Board_PinSetMode(16, BOARD_GPIO_AF_PUSHPULL,  BOARD_GPIO_SPEED_50MHZ);
	Board_PinSetAfRole(16, BOARD_AF_ROLE_SPI_MOSI);

	LL_SPI_InitTypeDef spi = {0};
	spi.TransferDirection  = LL_SPI_HALF_DUPLEX_TX;	/* matches F103 default */
	spi.Mode               = LL_SPI_MODE_MASTER;
	spi.DataWidth          = LL_SPI_DATAWIDTH_8BIT;
	/* SPI mode 3 (CPOL=high, CPHA=2nd edge) -- F103 default. Sensor
	 * drivers override CR1.CPOL/CPHA per transfer below. */
	spi.ClockPolarity      = LL_SPI_POLARITY_HIGH;
	spi.ClockPhase         = LL_SPI_PHASE_2EDGE;
	spi.NSS                = LL_SPI_NSS_SOFT;
	/* APB2 = 96 MHz. /64 = 1.5 MHz -- matches F103 SPI1 baud at 72 MHz / 64
	 * within the same order of magnitude; sensor drivers don't care about
	 * exact baud, only that it's well below their max. */
	spi.BaudRate           = LL_SPI_BAUDRATEPRESCALER_DIV64;
	spi.BitOrder           = LL_SPI_MSB_FIRST;
	spi.CRCCalculation     = LL_SPI_CRCCALCULATION_DISABLE;
	spi.CRCPoly            = 7;
	LL_SPI_Init(SPI1, &spi);

	/* DMA Tx + Rx requests enabled at the SPI peripheral; the streams
	 * themselves are armed per-transfer in the *_Transmit / *_Receive /
	 * *_TransmitReceive entry points. */
	LL_SPI_EnableDMAReq_TX(SPI1);
	LL_SPI_EnableDMAReq_RX(SPI1);

	LL_SPI_Enable(SPI1);
}

/* Common DMA stream init helper -- the three transfer entry points only
 * differ in direction, memory address, and which stream + IRQ to arm. */
static void SpiDmaInit(uint32_t stream, uint32_t direction, uint32_t mem_addr, uint16_t length)
{
	/* Disable first; LL config calls are illegal while EN=1. */
	LL_DMA_DisableStream(DMA2, stream);

	LL_DMA_InitTypeDef dma = {0};
	dma.PeriphOrM2MSrcAddress  = (uint32_t)&SPI1->DR;
	dma.MemoryOrM2MDstAddress  = mem_addr;
	dma.Direction              = direction;
	dma.Mode                   = LL_DMA_MODE_NORMAL;
	dma.PeriphOrM2MSrcIncMode  = LL_DMA_PERIPH_NOINCREMENT;
	dma.MemoryOrM2MDstIncMode  = LL_DMA_MEMORY_INCREMENT;
	dma.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
	dma.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
	dma.NbData                 = length;
	dma.Channel                = SPI_DMA_CHANNEL;
	dma.Priority               = LL_DMA_PRIORITY_MEDIUM;
	dma.FIFOMode               = LL_DMA_FIFOMODE_DISABLE;
	dma.FIFOThreshold          = LL_DMA_FIFOTHRESHOLD_1_4;
	dma.MemBurst               = LL_DMA_MBURST_SINGLE;
	dma.PeriphBurst            = LL_DMA_PBURST_SINGLE;
	LL_DMA_Init(DMA2, stream, &dma);

	LL_DMA_EnableIT_TC(DMA2, stream);
}

void SPI_HalfDuplex_Transmit(uint8_t * data, uint16_t length, uint8_t spi_mode)
{
	SpiDmaInit(SPI_DMA_TX_STREAM, LL_DMA_DIRECTION_MEMORY_TO_PERIPH,
	           (uint32_t)data, length);

	NVIC_SetPriority(SPI_DMA_TX_IRQn, 2);
	NVIC_EnableIRQ(SPI_DMA_TX_IRQn);

	/* Re-enter half-duplex TX mode: BIDIMODE=1 + BIDIOE=1 + override
	 * CPOL/CPHA per the sensor's spi_mode. Mirrors the F103 sequence. */
	uint16_t cr1 = SPI1->CR1;
	cr1 |= SPI_CR1_SPE;
	cr1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);
	cr1 |= SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE | (spi_mode & 0x03);
	SPI1->CR1 = cr1;
	(void)SPI1->DR;	/* clear RXNE */

	LL_DMA_EnableStream(DMA2, SPI_DMA_TX_STREAM);
}

void SPI_HalfDuplex_Receive(uint8_t * data, uint16_t length, uint8_t spi_mode)
{
	SpiDmaInit(SPI_DMA_RX_STREAM, LL_DMA_DIRECTION_PERIPH_TO_MEMORY,
	           (uint32_t)data, length);

	NVIC_SetPriority(SPI_DMA_RX_IRQn, 2);
	NVIC_EnableIRQ(SPI_DMA_RX_IRQn);

	/* Half-duplex RX: BIDIMODE=1, BIDIOE=0 (input). The TLE5011 / TLE5012
	 * drivers also need MOSI in open-drain; they call Board_TLE5011_BusDir
	 * before this entry point. */
	uint16_t cr1 = SPI1->CR1;
	cr1 |= SPI_CR1_SPE;
	cr1 &= ~(SPI_CR1_CPOL | SPI_CR1_CPHA);
	cr1 |= SPI_CR1_BIDIMODE | (spi_mode & 0x03);
	cr1 &= ~SPI_CR1_BIDIOE;
	SPI1->CR1 = cr1;
	(void)SPI1->DR;	/* clear RXNE */

	LL_DMA_EnableStream(DMA2, SPI_DMA_RX_STREAM);
}

void SPI_FullDuplex_TransmitReceive(uint8_t * tx_data, uint8_t * rx_data, uint16_t length, uint8_t spi_mode)
{
	/* Arm RX first so any spurious early shift register data goes
	 * straight to the buffer rather than the SPI peripheral overrunning. */
	SpiDmaInit(SPI_DMA_RX_STREAM, LL_DMA_DIRECTION_PERIPH_TO_MEMORY,
	           (uint32_t)rx_data, length);
	NVIC_SetPriority(SPI_DMA_RX_IRQn, 2);
	NVIC_EnableIRQ(SPI_DMA_RX_IRQn);

	SpiDmaInit(SPI_DMA_TX_STREAM, LL_DMA_DIRECTION_MEMORY_TO_PERIPH,
	           (uint32_t)tx_data, length);
	NVIC_SetPriority(SPI_DMA_TX_IRQn, 2);
	NVIC_EnableIRQ(SPI_DMA_TX_IRQn);

	uint16_t cr1 = SPI1->CR1;
	cr1 |= SPI_CR1_SPE;
	cr1 &= ~(SPI_CR1_BIDIMODE | SPI_CR1_BIDIOE | SPI_CR1_RXONLY |
	         SPI_CR1_CPOL | SPI_CR1_CPHA);
	cr1 |= (spi_mode & 0x03);
	SPI1->CR1 = cr1;
	(void)SPI1->DR;	/* clear RXNE */

	LL_DMA_EnableStream(DMA2, SPI_DMA_RX_STREAM);
	LL_DMA_EnableStream(DMA2, SPI_DMA_TX_STREAM);
}

uint16_t SPI_RxBytesRemaining(void)
{
	return LL_DMA_GetDataLength(DMA2, SPI_DMA_RX_STREAM);
}

uint16_t SPI_TxBytesRemaining(void)
{
	return LL_DMA_GetDataLength(DMA2, SPI_DMA_TX_STREAM);
}

void SPI_AbortTransfer(void)
{
	LL_DMA_DisableStream(DMA2, SPI_DMA_RX_STREAM);
	LL_DMA_DisableStream(DMA2, SPI_DMA_TX_STREAM);
	/* Restore half-duplex TX state so the next transmit cycle starts
	 * from a known direction. Mirrors F103's
	 * SPI_BiDirectionalLineConfig(SPI1, SPI_Direction_Tx). */
	SPI1->CR1 |= SPI_CR1_BIDIOE;
}
