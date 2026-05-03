/**
  ******************************************************************************
  * @file           : board_spi.c
  * @brief          : F411 BlackPill SPI driver -- compile/link stub (Phase 5c).
  *
  * The application's sensor drivers (as5048a, mcp320x, mlx90363, mlx90393,
  * tle5011, tle5012) link against SPI_Start / SPI_HalfDuplex_Transmit /
  * SPI_HalfDuplex_Receive / SPI_FullDuplex_TransmitReceive (the seam
  * declared in application/Inc/spi.h). On F103 these are implemented in
  * board/f103_bluepill/Src/board_spi.c with StdPeriph + DMA1.
  *
  * F411 will eventually use SPI1 with AF5 routing on PB3 (SCK) / PB4
  * (MISO) / PB5 (MOSI), driven by LL_SPI + DMA2 Streams 0 (RX) / 3 (TX)
  * (RM0383 Table 27). For Phase 5c we ship the function bodies as no-op
  * stubs so the application compiles and links; runtime behaviour
  * (actual DMA transfers + completion IRQs) lands in a follow-up commit
  * once a BlackPill is in hand to verify on a scope.
  *
  * Sensor drivers will silently get back zero-filled buffers when these
  * stubs run. That's expected pre-hardware -- the alternative is making
  * the F411 image link-fail for an unbounded period while the LL DMA
  * driver is debugged blind. The cost is one runtime regression that's
  * obvious on a connected scope (no bus activity at all on PB3-5) and
  * fixable in one commit when needed.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_spi.h"

#include "spi.h"

void SPI_Start(void)
{
	/* Bus + GPIO clock enable. Safe to run repeatedly. */
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_SPI1);

	/* PB3 SCK, PB4 MISO, PB5 MOSI -- SPI1 = AF5 on F411. The application's
	 * IO_Init normally routes the AF via Board_PinSetAfRole; doing it here
	 * too is harmless and lets the SPI bus come up even if Board_PinSetAfRole
	 * isn't implemented yet on F411 (Phase 5c is in progress). */
	LL_GPIO_InitTypeDef gpio = {0};
	gpio.Pin        = LL_GPIO_PIN_3 | LL_GPIO_PIN_4 | LL_GPIO_PIN_5;
	gpio.Mode       = LL_GPIO_MODE_ALTERNATE;
	gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	gpio.Speed      = LL_GPIO_SPEED_FREQ_HIGH;
	gpio.Pull       = LL_GPIO_PULL_NO;
	gpio.Alternate  = LL_GPIO_AF_5;
	LL_GPIO_Init(GPIOB, &gpio);

	LL_SPI_InitTypeDef spi = {0};
	spi.TransferDirection  = LL_SPI_HALF_DUPLEX_TX;	/* matches F103 default */
	spi.Mode               = LL_SPI_MODE_MASTER;
	spi.DataWidth          = LL_SPI_DATAWIDTH_8BIT;
	spi.ClockPolarity      = LL_SPI_POLARITY_HIGH;
	spi.ClockPhase         = LL_SPI_PHASE_2EDGE;
	spi.NSS                = LL_SPI_NSS_SOFT;
	spi.BaudRate           = LL_SPI_BAUDRATEPRESCALER_DIV64;
	spi.BitOrder           = LL_SPI_MSB_FIRST;
	spi.CRCCalculation     = LL_SPI_CRCCALCULATION_DISABLE;
	spi.CRCPoly            = 7;
	LL_SPI_Init(SPI1, &spi);
	LL_SPI_Enable(SPI1);
}

void SPI_HalfDuplex_Transmit(uint8_t * data, uint16_t length, uint8_t spi_mode)
{
	(void)data; (void)length; (void)spi_mode;
	/* No-op stub. See file header for rationale. */
}

void SPI_HalfDuplex_Receive(uint8_t * data, uint16_t length, uint8_t spi_mode)
{
	(void)data; (void)length; (void)spi_mode;
}

void SPI_FullDuplex_TransmitReceive(uint8_t * tx_data, uint8_t * rx_data, uint16_t length, uint8_t spi_mode)
{
	(void)tx_data; (void)rx_data; (void)length; (void)spi_mode;
}

/* Sensor-driver completion hooks. The stub never starts a transfer, so
 * remaining-byte counters always read zero and abort is a no-op. Any
 * polling loop in the application's sensor code exits on the first
 * iteration on F411. Real DMA2-based impl arrives later. */
uint16_t SPI_RxBytesRemaining(void) { return 0; }
uint16_t SPI_TxBytesRemaining(void) { return 0; }
void     SPI_AbortTransfer(void)    { }
