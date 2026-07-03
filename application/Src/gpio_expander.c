/**
  ******************************************************************************
  * @file           : gpio_expander.c
  * @brief          : MCP23017 (I2C) / MCP23S17 (SPI) GPIO expander button source.
  ******************************************************************************
  * See gpio_expander.h for the producer/consumer split and MCP23017_PLAN.md for
  * the design. Buttons wire to GND (pressed = LOW); by default IPOL is set so a
  * set bit means "pressed", matching the internal active-high convention.
  ******************************************************************************
  */

#include "gpio_expander.h"
#include "i2c.h"
#include "spi.h"
#include "periphery.h"		/* pin_config[] (CS GPIO) */
#include "common_defines.h"
#include "analog.h"			/* sensors[] -- for the bus-idle gate */

/* MCP2301x registers, BANK = 0 (power-on default): A/B ports are paired so a
 * 2-byte sequential access touches both ports in one transfer. */
#define MCP_IODIRA		0x00	/* 1 = input  */
#define MCP_IPOLA		0x02	/* 1 = invert */
#define MCP_IOCON		0x0A	/* config (HAEN lives here) */
#define MCP_GPPUA		0x0C	/* 1 = 100k pull-up */
#define MCP_GPIOA		0x12	/* read pins */

#define MCP_IOCON_HAEN	0x08	/* honour hardware address pins (SPI multi-chip per CS) */

/* MCP23S17 SPI device opcode: 0b0100 A2 A1 A0 R/W. */
#define MCPS_OP_WRITE(hw)	((uint8_t)(0x40 | (((hw) & 0x07) << 1)))
#define MCPS_OP_READ(hw)	((uint8_t)(0x41 | (((hw) & 0x07) << 1)))
#define GPIO_EXP_SPI_MODE	0		/* MCP23S17 supports SPI mode 0 and 3 */
#define GPIO_EXP_SPI_POLL	200000	/* busy-wait guard for the DMA completion poll */

static uint8_t exp_data[MAX_GPIO_EXPANDER_NUM][2];	/* latest GPIOA/GPIOB per slot */
static uint8_t exp_present[MAX_GPIO_EXPANDER_NUM];	/* slot is responding / wired */
static int8_t  exp_cs[MAX_GPIO_EXPANDER_NUM];		/* SPI CS pin index, -1 for I2C/unused */

/* See gpio_expander.h. gpio_exp_bus_busy: the tick-ISR sensor kickoff skips
 * while set, so GpioExp_Process holds the shared bus for its whole scan.
 * gpio_exp_spi_active: set ONLY around the expander's own SPI DMA transfer;
 * Sensor_OnSpi*Complete skips on this one, so a real sensor completion arriving
 * before the expander actually takes the bus is serviced, not dropped. */
volatile uint8_t gpio_exp_bus_busy = 0;
volatile uint8_t gpio_exp_spi_active = 0;

static uint8_t slot_is_i2c (const dev_config_t * c, uint8_t i)
{
	return c->gpio_expanders[i].type == GPIO_EXP_MCP23017 &&
	       c->gpio_expanders[i].address >= 0x20 &&
	       c->gpio_expanders[i].address <= 0x27;
}
static uint8_t slot_is_spi (const dev_config_t * c, uint8_t i)
{
	return c->gpio_expanders[i].type == GPIO_EXP_MCP23S17;
}

/* True only when no sensor (I2C or SPI) has a transfer in flight, so a blocking
 * expander access can't corrupt it -- and, for SPI, so the shared DMA RX-complete
 * ISR (Sensor_OnSpiRxComplete) finds no active sensor and no-ops on our transfer. */
static uint8_t BusIdle (void)
{
	for (uint8_t i = 0; i < MAX_AXIS_NUM; i++)
	{
		const int8_t src = sensors[i].source;
		const uint8_t in_use = (src == (pin_t)SOURCE_I2C) || (src >= 0);
		if (in_use && (!sensors[i].rx_complete || !sensors[i].tx_complete))
			return 0;
	}
	return 1;
}

/* ---- SPI helpers (MCP23S17): reuse the sensor DMA primitive, gated by BusIdle. */
static void spi_xfer4 (int8_t cs, const uint8_t tx[4], uint8_t rx[4])
{
	uint8_t txb[4]; for (uint8_t k = 0; k < 4; k++) txb[k] = tx[k];
	/* Mark our transfer active across the DMA start + completion IRQ so
	 * Sensor_OnSpi{Rx,Tx}Complete no-op ours. Callers only reach here with the
	 * bus idle (GpioExp_Process after BusIdle(), or GpioExp_Init pre-sensors),
	 * so no genuine sensor completion can occur inside this window. */
	gpio_exp_spi_active = 1;
	pin_config[cs].port->ODR &= ~pin_config[cs].pin;			/* CS low */
	SPI_FullDuplex_TransmitReceive(txb, rx, 4, GPIO_EXP_SPI_MODE);
	uint32_t guard = 0;
	while (SPI_RxBytesRemaining() > 0 && ++guard < GPIO_EXP_SPI_POLL) { }
	SPI_AbortTransfer();
	pin_config[cs].port->ODR |= pin_config[cs].pin;				/* CS high */
	gpio_exp_spi_active = 0;
}
static void spi_write_reg (int8_t cs, uint8_t hw, uint8_t reg, uint8_t a, uint8_t b)
{
	uint8_t tx[4] = { MCPS_OP_WRITE(hw), reg, a, b };
	uint8_t rx[4];
	spi_xfer4(cs, tx, rx);
}

void GpioExp_Init (dev_config_t * p_dev_config)
{
	/* Match SPI_GPIO_CS-role pins to SPI-type slots in pin order (like the
	 * shift-register control-pin scan). */
	uint8_t cs_idx = 0;
	int8_t  cs_pins[MAX_GPIO_EXPANDER_NUM];
	for (uint8_t i = 0; i < MAX_GPIO_EXPANDER_NUM; i++) cs_pins[i] = -1;
	for (uint8_t p = 0; p < USED_PINS_NUM && cs_idx < MAX_GPIO_EXPANDER_NUM; p++)
		if (p_dev_config->pins[p] == SPI_GPIO_CS) cs_pins[cs_idx++] = (int8_t)p;

	/* Only touch a bus whose pins were actually assigned (and so whose
	 * peripheral SPI_Start/I2C_Start brought up in periphery.c). Probing an
	 * un-started peripheral would otherwise burn a per-transfer timeout for
	 * every configured-but-unwired expander. */
	uint8_t i2c_bus_up = 0, spi_bus_up = 0;
	for (uint8_t p = 0; p < USED_PINS_NUM; p++) {
		if (p_dev_config->pins[p] == I2C_SCL)                                i2c_bus_up = 1;
		else if (p_dev_config->pins[p] == SPI_SCK || p_dev_config->pins[p] == SPI_MOSI) spi_bus_up = 1;
	}

	for (uint8_t i = 0; i < MAX_GPIO_EXPANDER_NUM; i++)
	{
		exp_data[i][0] = 0; exp_data[i][1] = 0;
		exp_present[i] = 0; exp_cs[i] = -1;

		const uint8_t flags = p_dev_config->gpio_expanders[i].flags;
		const uint8_t pull  = (flags & GPIO_EXP_FLAG_PULLUPS) ? 0xFF : 0x00;
		/* GND-wired buttons read LOW when pressed -> invert in hardware by
		 * default (set bit == pressed); the INVERT flag flips that back. */
		const uint8_t ipol  = (flags & GPIO_EXP_FLAG_INVERT) ? 0x00 : 0xFF;

		if (slot_is_i2c(p_dev_config, i) && i2c_bus_up)
		{
			const uint8_t addr = p_dev_config->gpio_expanders[i].address;
			uint8_t iodir_ab[2] = { 0xFF, 0xFF };
			uint8_t gppu_ab[2]  = { pull, pull };
			uint8_t ipol_ab[2]  = { ipol, ipol };
			/* The IODIR write doubles as a presence probe (skips absent chips
			 * so GpioExp_Process doesn't stall on a per-read timeout). */
			if (I2C_WriteBlocking(addr, MCP_IODIRA, iodir_ab, 2) != 0) continue;
			I2C_WriteBlocking(addr, MCP_GPPUA, gppu_ab, 2);
			I2C_WriteBlocking(addr, MCP_IPOLA, ipol_ab, 2);
			exp_present[i] = 1;
		}
		else if (slot_is_spi(p_dev_config, i) && spi_bus_up)
		{
			/* Resolve this chip's CS from its stored index (flags bits 4:2) into
			 * the assigned SPI_GPIO_CS pins. Chips sharing an index share a CS
			 * line; their `address` DIP straps (0..7) tell them apart, honoured
			 * because HAEN is set below. Skip a chip whose index names a CS pin
			 * that isn't assigned. */
			const uint8_t cs_index = (flags & GPIO_EXP_CS_MASK) >> GPIO_EXP_CS_SHIFT;
			if (cs_index >= cs_idx) continue;
			const int8_t  cs = cs_pins[cs_index];
			const uint8_t hw = p_dev_config->gpio_expanders[i].address & 0x07;
			exp_cs[i] = cs;
			/* HAEN honours the A2:A0 straps so multiple chips can share this CS.
			 * The first HAEN write on a shared CS reaches every chip on it (all
			 * still HAEN=0 at power-on, so they ignore the address bits) and
			 * enables HAEN on all at once -- harmless; each is then addressable
			 * at its strap for the per-chip register writes below. */
			spi_write_reg(cs, hw, MCP_IOCON, MCP_IOCON_HAEN, MCP_IOCON_HAEN);
			spi_write_reg(cs, hw, MCP_IODIRA, 0xFF, 0xFF);
			spi_write_reg(cs, hw, MCP_GPPUA, pull, pull);
			spi_write_reg(cs, hw, MCP_IPOLA, ipol, ipol);
			exp_present[i] = 1;
		}
	}
}

void GpioExp_Process (dev_config_t * p_dev_config)
{
	/* Claim the bus BEFORE the idle check, so the ordering is race-free against
	 * the tick ISR: set busy -> if a sensor is already mid-transfer (BusIdle
	 * false) back off and let it finish; otherwise we hold the claim and the
	 * tick-ISR kickoff (which reads gpio_exp_bus_busy) won't start a sensor
	 * until we clear it. A sensor completion IRQ landing in the set->check gap
	 * is NOT dropped: Sensor_OnSpi*Complete gate on gpio_exp_spi_active (set only
	 * around our actual transfer below), so such a completion is serviced, and
	 * BusIdle() then observes the still-busy sensor and backs off this tick. */
	gpio_exp_bus_busy = 1;
	if (!BusIdle()) { gpio_exp_bus_busy = 0; return; }	/* a sensor owns the bus this tick */

	for (uint8_t i = 0; i < MAX_GPIO_EXPANDER_NUM; i++)
	{
		if (!exp_present[i]) continue;

		if (slot_is_i2c(p_dev_config, i))
		{
			uint8_t buf[2];
			if (I2C_ReadBlocking(p_dev_config->gpio_expanders[i].address,
			                     MCP_GPIOA, buf, 2, 0) == 0)
			{
				exp_data[i][0] = buf[0];
				exp_data[i][1] = buf[1];
			}
		}
		else if (slot_is_spi(p_dev_config, i) && exp_cs[i] >= 0)
		{
			const uint8_t hw = p_dev_config->gpio_expanders[i].address & 0x07;
			uint8_t tx[4] = { MCPS_OP_READ(hw), MCP_GPIOA, 0x00, 0x00 };
			uint8_t rx[4] = { 0, 0, 0, 0 };
			spi_xfer4(exp_cs[i], tx, rx);
			exp_data[i][0] = rx[2];				/* GPIOA */
			exp_data[i][1] = rx[3];				/* GPIOB */
		}
	}

	gpio_exp_bus_busy = 0;						/* release the bus to the sensors */
}

void GpioExp_FoldBits (uint16_t gpio, uint8_t button_cnt,
                       uint8_t * raw_button_data_buf, uint8_t * pos, uint8_t max)
{
	if (button_cnt > 16) button_cnt = 16;
	for (uint8_t b = 0; b < button_cnt; b++)
	{
		if (*pos >= max) return;
		raw_button_data_buf[*pos] = (uint8_t)((gpio >> b) & 0x01u);
		(*pos)++;
	}
}

void GpioExp_Get (uint8_t * raw_button_data_buf, dev_config_t * p_dev_config, uint8_t * pos)
{
	for (uint8_t i = 0; i < MAX_GPIO_EXPANDER_NUM; i++)
	{
		if (!exp_present[i]) continue;
		uint16_t gpio = (uint16_t)exp_data[i][0] | ((uint16_t)exp_data[i][1] << 8);
		GpioExp_FoldBits(gpio, p_dev_config->gpio_expanders[i].button_cnt,
		                 raw_button_data_buf, pos, MAX_BUTTONS_NUM);
	}
}
