/**
  ******************************************************************************
  * @file           : board_i2c.c
  * @brief          : F411 BlackPill I2C driver (I2C2 + DMA1, raw register).
  *
  * The application's ADS1115 / AS5600 drivers link against I2C_Start /
  * I2C_WriteBlocking / I2C_ReadBlocking / I2C_WriteNonBlocking /
  * I2C_ReadNonBlocking declared in application/Inc/i2c.h. F103 impl at
  * board/f103_bluepill/Src/board_i2c.c (StdPeriph + DMA1 channels 4/5).
  *
  * Pin choice vs F103 (PB10 SCL / PB11 SDA on AF4):
  *   F411 PB11 isn't bonded on UFQFPN48, so I2C2_SDA must move. The
  *   F411 datasheet routes I2C2_SDA on either PB3 (AF9, mutex with
  *   SPI1_SCK) or PB9 (AF9, coexists with SPI1). The configurator
  *   picks the slot at config time; this driver doesn't care which
  *   pin holds the role -- pin AF setup is done by periphery.c's
  *   per-slot loop against dev_config.pins[].
  *
  * Routing summary:
  *   I2C2_SCL = PB10 (slot 21) AF4                  (always)
  *   I2C2_SDA = PB9  (slot 20) AF9                  (default, coexists with SPI1)
  *      OR    = PB3  (slot 14) AF9                  (mutex with SPI1_SCK)
  *   DMA1 Stream 2 Channel 7 = I2C2_RX              (DMA1_Stream2_IRQn)
  *   DMA1 Stream 7 Channel 7 = I2C2_TX              (DMA1_Stream7_IRQn)
  *
  * No back-compat for the pre-PR-#52 wire layout that placed SDA on slot
  * 22 (PB2 on F411, no I2C cap). Configurator FreeJoyXConfiguratorQt#48
  * strips the bad SDA pick from the F411 B2 dropdown, migrates existing
  * stored configs to clear slot 22 on read, and the bus quick-setup
  * toggle writes SDA to PB9 by default. A user who flashes this firmware
  * with a config that still has SDA on slot 22 will see I2C stop working
  * until they update the configurator and re-save -- explicit failure
  * rather than the old silent re-route to PB3.
  *
  * Clock config: APB1 peripheral clock = HCLK / 2 = 48 MHz on F411
  * with the locked Board_ClockInit_F411 plan (HCLK = 96 MHz). I2C2's
  * CR2.FREQ / CCR / TRISE are sized off APB1 peripheral, NOT APB1
  * timer, so the magic numbers differ from F103 (which uses APB1
  * peripheral 36 MHz).
  *
  * Direct register access -- LL_I2C isn't vendored on this branch and
  * the F411 I2C peripheral register layout matches F103 (same IP
  * family). Vendoring 1500 lines of LL_I2C for a single-bus driver
  * isn't worth it.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_dma.h"

#include "board_pins.h"
#include "i2c.h"

#define I2C2_DMA_RX_STREAM   LL_DMA_STREAM_2
#define I2C2_DMA_TX_STREAM   LL_DMA_STREAM_7
#define I2C2_DMA_CHANNEL     LL_DMA_CHANNEL_7
#define I2C2_DMA_RX_IRQn     DMA1_Stream2_IRQn
#define I2C2_DMA_TX_IRQn     DMA1_Stream7_IRQn

/* SR1/SR2 event masks per RM0383 section 18.4. SR1_/SR2_ bit constants
 * come from CMSIS stm32f411xe.h. The composite event values match
 * StdPeriph's I2C_EVENT_* of the same name byte-for-byte. */
#define I2C_EVENT_MASTER_MODE_SELECT                 \
	((I2C_SR1_SB) | ((I2C_SR2_MSL | I2C_SR2_BUSY) << 16))
#define I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED   \
	((I2C_SR1_ADDR | I2C_SR1_TXE) | ((I2C_SR2_MSL | I2C_SR2_BUSY | I2C_SR2_TRA) << 16))
#define I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED      \
	((I2C_SR1_ADDR) | ((I2C_SR2_MSL | I2C_SR2_BUSY) << 16))
#define I2C_EVENT_MASTER_BYTE_TRANSMITTED            \
	((I2C_SR1_BTF | I2C_SR1_TXE) | ((I2C_SR2_MSL | I2C_SR2_BUSY | I2C_SR2_TRA) << 16))
#define I2C_EVENT_MASTER_BYTE_RECEIVED               \
	((I2C_SR1_RXNE) | ((I2C_SR2_MSL | I2C_SR2_BUSY) << 16))

static inline uint8_t I2C_CheckEvent(uint32_t event)
{
	uint32_t flags = (I2C2->SR1 | (I2C2->SR2 << 16));
	return (flags & event) == event;
}

void I2C_Start(void)
{
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA1);
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_I2C2);

	/* SCL + SDA pin AF setup is done by periphery.c's per-slot loop -- it
	 * walks dev_config.pins[] and calls Board_PinSetMode + Board_PinSetAfRole
	 * for every slot whose role + cap match (I2C_SCL on slot 21, I2C_SDA on
	 * either slot 14 / PB3 or slot 20 / PB9, both cap-bearing on F411).
	 *
	 * If the configurator wrote I2C_SDA to a slot with no I2C cap (e.g. the
	 * legacy F411 wire layout that placed SDA on slot 22 / PB2), the loop
	 * silently skips it -- I2C2 stays initialised here but with no SDA pin
	 * wired, so transactions just don't complete. That's the explicit-failure
	 * UX we want: open the configurator, the per-board dropdown filter has
	 * stripped the bad SDA pick, the read-time migration has cleared the
	 * orphaned role, re-toggle the I2C bus to wire SDA on PB9 (default) or
	 * PB3 (alt). */

	/* SWRST clears any stuck state from a previous boot. F103 doesn't
	 * need this because StdPeriph's I2C_Init does it implicitly. */
	I2C2->CR1 |= I2C_CR1_SWRST;
	I2C2->CR1 &= ~I2C_CR1_SWRST;

	/* CR2.FREQ = APB1 peripheral clock in MHz. F411 = 48. */
	I2C2->CR2 = 48;

	/* Fast Mode 400 kHz, duty 2/1 (matches F103's I2C_DutyCycle_2/1
	 * variant -- F103 ships I2C_DutyCycle_16_9 but the difference is
	 * a few-percent SCL high/low ratio, irrelevant for our sensors).
	 *
	 * In Fast Mode with DUTY=0 the low period is TWICE the high period
	 * (NOT equal -- that's Standard Mode), so the SCL period spans
	 * 3*CCR*Tpclk1, not 2*CCR*Tpclk1:
	 *   T_high = CCR * Tpclk1,  T_low = 2 * CCR * Tpclk1
	 *   T_total = 3 * CCR * Tpclk1 = 1 / 400_000
	 *   CCR = Tpclk1_freq / (3 * 400_000)
	 *       = 48_000_000 / 1_200_000 = 40
	 * The old value 60 used the Standard-Mode 2*CCR formula while FS=1,
	 * so SCL ran at 48 MHz / (3*60) = 266 kHz instead of 400 kHz.
	 * Set FS=1 (Fast Mode), DUTY=0 (2/1 ratio). */
	I2C2->CCR = I2C_CCR_FS | 40;

	/* TRISE = (max_rise_time_ns / Tpclk1_ns) + 1. Fast Mode spec:
	 * 300 ns max rise. At 48 MHz Tpclk1 = 20.83 ns -> TRISE = 14 + 1. */
	I2C2->TRISE = 15;

	/* OAR1 own address irrelevant in master mode; F103 sets 0x07,
	 * mirror. Bit 14 must be 1 per RM0383 (reserved). */
	I2C2->OAR1 = (1U << 14) | (0x07 << 1);

	/* Enable peripheral + ACK after each received byte. */
	I2C2->CR1 |= I2C_CR1_PE | I2C_CR1_ACK;

	/* Error interrupt -- AF/BUS/TIMEOUT/ARLO notification path. F103
	 * enables this; F411 mirrors. The IRQ handler body just clears the
	 * error flags for now; full ADS1115 recovery chain comes with the
	 * sensor_dispatch hoist. */
	I2C2->CR2 |= I2C_CR2_ITERREN;
	NVIC_EnableIRQ(I2C2_ER_IRQn);
	NVIC_SetPriority(I2C2_ER_IRQn, 2);
}

int I2C_WriteBlocking(uint8_t dev_addr, uint8_t reg_addr, uint8_t * data, uint16_t length)
{
	uint32_t ticks = I2C_TIMEOUT;

	I2C2->CR2 &= ~I2C_CR2_DMAEN;
	I2C2->CR1 |= I2C_CR1_ACK;

	I2C2->CR1 |= I2C_CR1_START;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->DR = dev_addr << 1;	/* TX direction */
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->DR = reg_addr;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	for (uint16_t i = 0; i < length; ++i)
	{
		I2C2->DR = data[i];
		while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) && --ticks);
		if (ticks == 0) return -1;
		ticks = I2C_TIMEOUT;
	}

	I2C2->CR1 |= I2C_CR1_STOP;
	while ((I2C2->SR2 & I2C_SR2_BUSY) && --ticks);
	if (ticks == 0) return -1;

	return 0;
}

int I2C_ReadBlocking(uint8_t dev_addr, uint8_t reg_addr, uint8_t * data, uint16_t length, uint8_t nack)
{
	uint32_t ticks = I2C_TIMEOUT;

	I2C2->CR2 &= ~I2C_CR2_DMAEN;
	I2C2->CR1 |= I2C_CR1_ACK;

	/* Phase 1: write reg_addr (selects slave's internal register). */
	I2C2->CR1 |= I2C_CR1_START;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->DR = dev_addr << 1;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->DR = reg_addr;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->CR1 |= I2C_CR1_STOP;
	while ((I2C2->SR2 & I2C_SR2_BUSY) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	/* Phase 2: repeated start, switch to RX, drain length bytes. */
	I2C2->CR1 |= I2C_CR1_START;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->DR = (dev_addr << 1) | 1U;	/* RX direction */
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	for (uint8_t i = 0; i < length; ++i)
	{
		while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_RECEIVED) && --ticks);
		if (ticks == 0) return -1;
		ticks = I2C_TIMEOUT;

		data[i] = (uint8_t)I2C2->DR;

		if (nack && i == length - 2)
		{
			I2C2->CR1 &= ~I2C_CR1_ACK;	/* NACK after the next-but-final byte */
			I2C2->CR1 |= I2C_CR1_POS;
		}
	}

	I2C2->CR1 |= I2C_CR1_STOP;
	while ((I2C2->SR2 & I2C_SR2_BUSY) && --ticks);
	if (ticks == 0) return -1;

	return 0;
}

int I2C_WriteNonBlocking(uint8_t dev_addr, uint8_t * data, uint16_t length)
{
	uint32_t ticks = I2C_TIMEOUT;

	LL_DMA_DisableStream(DMA1, I2C2_DMA_TX_STREAM);

	LL_DMA_InitTypeDef dma = {0};
	dma.PeriphOrM2MSrcAddress  = (uint32_t)&I2C2->DR;
	dma.MemoryOrM2MDstAddress  = (uint32_t)data;
	dma.Direction              = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
	dma.Mode                   = LL_DMA_MODE_NORMAL;
	dma.PeriphOrM2MSrcIncMode  = LL_DMA_PERIPH_NOINCREMENT;
	dma.MemoryOrM2MDstIncMode  = LL_DMA_MEMORY_INCREMENT;
	dma.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
	dma.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
	dma.NbData                 = length;
	dma.Channel                = I2C2_DMA_CHANNEL;
	dma.Priority               = LL_DMA_PRIORITY_MEDIUM;
	dma.FIFOMode               = LL_DMA_FIFOMODE_DISABLE;
	dma.FIFOThreshold          = LL_DMA_FIFOTHRESHOLD_1_4;
	dma.MemBurst               = LL_DMA_MBURST_SINGLE;
	dma.PeriphBurst            = LL_DMA_PBURST_SINGLE;
	LL_DMA_Init(DMA1, I2C2_DMA_TX_STREAM, &dma);

	LL_DMA_EnableIT_TC(DMA1, I2C2_DMA_TX_STREAM);
	NVIC_SetPriority(I2C2_DMA_TX_IRQn, 2);
	NVIC_EnableIRQ(I2C2_DMA_TX_IRQn);

	/* Wait for bus free; on persistent BUSY do a SWRST + reinit (matches
	 * F103 recovery path after an aborted previous transfer). */
	while ((I2C2->SR2 & I2C_SR2_BUSY) && --ticks);
	if (ticks == 0)
	{
		I2C2->CR1 |= I2C_CR1_SWRST;
		I2C2->CR1 &= ~I2C_CR1_SWRST;
		I2C_Start();
	}
	ticks = I2C_TIMEOUT;

	LL_DMA_EnableStream(DMA1, I2C2_DMA_TX_STREAM);
	I2C2->CR2 |= I2C_CR2_DMAEN;

	I2C2->CR1 |= I2C_CR1_START;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->DR = dev_addr << 1;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) && --ticks);
	if (ticks == 0) return -1;

	return 0;
}

int I2C_ReadNonBlocking(uint8_t dev_addr, uint8_t reg_addr, uint8_t * data, uint16_t length, uint8_t nack)
{
	uint32_t ticks = I2C_TIMEOUT;

	I2C2->CR2 &= ~I2C_CR2_DMAEN;
	LL_DMA_DisableStream(DMA1, I2C2_DMA_RX_STREAM);
	while (LL_DMA_IsEnabledStream(DMA1, I2C2_DMA_RX_STREAM));

	LL_DMA_InitTypeDef dma = {0};
	dma.PeriphOrM2MSrcAddress  = (uint32_t)&I2C2->DR;
	dma.MemoryOrM2MDstAddress  = (uint32_t)data;
	dma.Direction              = LL_DMA_DIRECTION_PERIPH_TO_MEMORY;
	dma.Mode                   = LL_DMA_MODE_NORMAL;
	dma.PeriphOrM2MSrcIncMode  = LL_DMA_PERIPH_NOINCREMENT;
	dma.MemoryOrM2MDstIncMode  = LL_DMA_MEMORY_INCREMENT;
	dma.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_BYTE;
	dma.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_BYTE;
	dma.NbData                 = length;
	dma.Channel                = I2C2_DMA_CHANNEL;
	dma.Priority               = LL_DMA_PRIORITY_MEDIUM;
	dma.FIFOMode               = LL_DMA_FIFOMODE_DISABLE;
	dma.FIFOThreshold          = LL_DMA_FIFOTHRESHOLD_1_4;
	dma.MemBurst               = LL_DMA_MBURST_SINGLE;
	dma.PeriphBurst            = LL_DMA_PBURST_SINGLE;
	LL_DMA_Init(DMA1, I2C2_DMA_RX_STREAM, &dma);

	LL_DMA_EnableIT_TC(DMA1, I2C2_DMA_RX_STREAM);
	NVIC_SetPriority(I2C2_DMA_RX_IRQn, 2);
	NVIC_EnableIRQ(I2C2_DMA_RX_IRQn);

	while ((I2C2->SR2 & I2C_SR2_BUSY) && --ticks);
	if (ticks == 0)
	{
		I2C2->CR1 |= I2C_CR1_SWRST;
		I2C2->CR1 &= ~I2C_CR1_SWRST;
		I2C_Start();
	}
	ticks = I2C_TIMEOUT;

	if (nack) I2C2->CR2 |= I2C_CR2_LAST;
	else      I2C2->CR2 &= ~I2C_CR2_LAST;

	/* Phase 1: write reg_addr. */
	I2C2->CR1 |= I2C_CR1_START;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->DR = dev_addr << 1;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->CR1 |= I2C_CR1_PE;	/* clear EV6 */
	I2C2->DR = reg_addr;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_BYTE_TRANSMITTED) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->CR1 |= I2C_CR1_STOP;
	while ((I2C2->SR2 & I2C_SR2_BUSY) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	/* Phase 2: arm DMA + restart in RX direction. */
	LL_DMA_EnableStream(DMA1, I2C2_DMA_RX_STREAM);
	I2C2->CR2 |= I2C_CR2_DMAEN;

	I2C2->CR1 |= I2C_CR1_START;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_MODE_SELECT) && --ticks);
	if (ticks == 0) return -1;
	ticks = I2C_TIMEOUT;

	I2C2->DR = (dev_addr << 1) | 1U;
	while (!I2C_CheckEvent(I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED) && --ticks);
	if (ticks == 0) return -1;

	return 0;
}
