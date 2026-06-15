/**
  ******************************************************************************
  * @file           : mcp23017.c
  * @brief          : MCP23017 I2C GPIO expander button-input source.
  ******************************************************************************
  * See mcp23017.h for the producer/consumer split and MCP23017_PLAN.md for the
  * design. Buttons wire to GND (pressed = LOW); by default IPOL is set so a set
  * bit means "pressed", matching the internal active-high button convention.
  ******************************************************************************
  */

#include "mcp23017.h"
#include "i2c.h"
#include "common_defines.h"
#include "analog.h"		/* sensors[] -- for the I2C bus-idle gate */

/* MCP23017 registers, BANK = 0 (power-on default): A/B ports are paired so a
 * 2-byte sequential access touches both ports in one transfer. */
#define MCP23017_IODIRA		0x00	/* 1 = input  */
#define MCP23017_IPOLA		0x02	/* 1 = invert */
#define MCP23017_GPPUA		0x0C	/* 1 = 100k pull-up */
#define MCP23017_GPIOA		0x12	/* read pins */

/* Latest GPIOA(=[0]) / GPIOB(=[1]) per slot, refreshed by I2cGpioProcess and
 * consumed by I2cGpioGet. uint8 element writes are atomic on Cortex-M, so the
 * ISR-side fold never sees a half-written byte. */
static uint8_t i2c_gpio_data[MAX_I2C_GPIO_NUM][2];

/* Slot present == its IODIR write ACKed at init. Lets I2cGpioProcess skip a
 * misconfigured / absent address instead of stalling on a per-read timeout. */
static uint8_t i2c_gpio_present[MAX_I2C_GPIO_NUM];

/* True only when no I2C sensor (ADS1115 / AS5600 / MLX90393_I2C) has a DMA
 * transfer in flight, so a blocking expander read can't corrupt it. */
static uint8_t I2cBusIdle (void)
{
	for (uint8_t i = 0; i < MAX_AXIS_NUM; i++)
	{
		if (sensors[i].source == (pin_t)SOURCE_I2C &&
		    (!sensors[i].rx_complete || !sensors[i].tx_complete))
		{
			return 0;
		}
	}
	return 1;
}

void I2cGpioInit (dev_config_t * p_dev_config)
{
	for (uint8_t i = 0; i < MAX_I2C_GPIO_NUM; i++)
	{
		i2c_gpio_data[i][0]  = 0;
		i2c_gpio_data[i][1]  = 0;
		i2c_gpio_present[i]  = 0;

		uint8_t addr = p_dev_config->i2c_gpio[i].address;
		if (addr == 0) continue;					/* disabled slot */

		uint8_t flags  = p_dev_config->i2c_gpio[i].flags;
		uint8_t pull   = (flags & I2C_GPIO_FLAG_PULLUPS) ? 0xFF : 0x00;
		/* GND-wired buttons read LOW when pressed, so invert in hardware by
		 * default (set bit == pressed); the INVERT flag flips that back. */
		uint8_t ipol   = (flags & I2C_GPIO_FLAG_INVERT) ? 0x00 : 0xFF;

		uint8_t iodir_ab[2] = { 0xFF, 0xFF };		/* all pins inputs */
		uint8_t gppu_ab[2]  = { pull, pull };
		uint8_t ipol_ab[2]  = { ipol, ipol };

		/* The IODIR write doubles as a presence probe. */
		if (I2C_WriteBlocking(addr, MCP23017_IODIRA, iodir_ab, 2) != 0) continue;
		I2C_WriteBlocking(addr, MCP23017_GPPUA, gppu_ab, 2);
		I2C_WriteBlocking(addr, MCP23017_IPOLA, ipol_ab, 2);
		i2c_gpio_present[i] = 1;
	}
}

void I2cGpioProcess (dev_config_t * p_dev_config)
{
	if (!I2cBusIdle()) return;						/* sensor DMA owns the bus */

	for (uint8_t i = 0; i < MAX_I2C_GPIO_NUM; i++)
	{
		if (!i2c_gpio_present[i]) continue;
		uint8_t buf[2];
		if (I2C_ReadBlocking(p_dev_config->i2c_gpio[i].address,
		                     MCP23017_GPIOA, buf, 2, 0) == 0)
		{
			i2c_gpio_data[i][0] = buf[0];
			i2c_gpio_data[i][1] = buf[1];
		}
	}
}

void I2cGpio_FoldBits (uint16_t gpio, uint8_t button_cnt,
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

void I2cGpioGet (uint8_t * raw_button_data_buf, dev_config_t * p_dev_config, uint8_t * pos)
{
	for (uint8_t i = 0; i < MAX_I2C_GPIO_NUM; i++)
	{
		if (p_dev_config->i2c_gpio[i].address == 0) continue;
		uint16_t gpio = (uint16_t)i2c_gpio_data[i][0] |
		                ((uint16_t)i2c_gpio_data[i][1] << 8);
		I2cGpio_FoldBits(gpio, p_dev_config->i2c_gpio[i].button_cnt,
		                 raw_button_data_buf, pos, MAX_BUTTONS_NUM);
	}
}
