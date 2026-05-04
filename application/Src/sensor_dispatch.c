/**
  ******************************************************************************
  * @file           : sensor_dispatch.c
  * @brief          : Cross-board SPI/I2C DMA-completion sensor chain dispatcher.
  *
  * Bodies hoisted verbatim from board/f103_bluepill DMA IRQ handlers so
  * the F411 BSP runs identical chain logic. Per-sensor StopDMA / StartDMA /
  * SetMuxDMA helpers are board-agnostic; SPI1 / I2C2 register pokes are
  * shared (same IP family). The only board-specific bits remain in the
  * IRQ handlers themselves: DMA flag-clear + DMA channel/stream disable
  * (StdPeriph DMA_Cmd / DMA_ClearFlag on F103, LL_DMA_DisableStream /
  * LL_DMA_ClearFlag on F411).
  ******************************************************************************
  */

#include "sensor_dispatch.h"

#include <stdint.h>

#include "analog.h"
#include "tle5011.h"
#include "tle5012.h"
#include "mcp320x.h"
#include "mlx90363.h"
#include "mlx90393.h"
#include "as5048a.h"
#include "as5600.h"
#include "ads1115.h"
#include "board_pins.h"
#include "i2c.h"
#include "spi.h"

#ifdef BOARD_F103_BLUEPILL
#include "stm32f10x.h"
#endif
#ifdef BOARD_F411_BLACKPILL
#include "stm32f4xx.h"
#endif

void Sensor_OnSpiRxComplete(void)
{
	uint8_t i = 0;

	/* Wait SPI transfer to end. SPI1->SR.BSY is on both F1 and F4
	 * peripherals -- direct register access is portable. */
	while (SPI1->SR & SPI_SR_BSY);

	/* Find the active SPI sensor whose RX just completed. */
	for (i = 0; i < MAX_AXIS_NUM; ++i) {
		if (sensors[i].source >= 0 && !sensors[i].rx_complete) break;
	}

	if (i < MAX_AXIS_NUM) {
		if (sensors[i].type == TLE5011) {
			TLE5011_StopDMA(&sensors[i++]);
		} else if (sensors[i].type == TLE5012) {
			TLE5012_StopDMA(&sensors[i++]);
		} else if (sensors[i].type == MCP3201) {
			MCP320x_StopDMA(&sensors[i++]);
		} else if (sensors[i].type == MCP3202) {
			MCP320x_StopDMA(&sensors[i]);
			if (sensors[i].curr_channel < 1) {
				MCP320x_StartDMA(&sensors[i], sensors[i].curr_channel + 1);
				return;
			}
			i++;
		} else if (sensors[i].type == MCP3204) {
			MCP320x_StopDMA(&sensors[i]);
			if (sensors[i].curr_channel < 3) {
				MCP320x_StartDMA(&sensors[i], sensors[i].curr_channel + 1);
				return;
			}
			i++;
		} else if (sensors[i].type == MCP3208) {
			MCP320x_StopDMA(&sensors[i]);
			if (sensors[i].curr_channel < 7) {
				MCP320x_StartDMA(&sensors[i], sensors[i].curr_channel + 1);
				return;
			}
			i++;
		} else if (sensors[i].type == MLX90363) {
			MLX90363_StopDMA(&sensors[i++]);
		} else if (sensors[i].type == MLX90393_SPI) {
			MLX90393_StopDMA(&sensors[i++]);
		} else if (sensors[i].type == AS5048A_SPI) {
			AS5048A_StopDMA(&sensors[i++]);
		}
	}

	/* Process next SPI sensor. */
	for ( ; i < MAX_AXIS_NUM; ++i) {
		if (sensors[i].source >= 0 && sensors[i].rx_complete && sensors[i].tx_complete) {
			if (sensors[i].type == TLE5011) {
				TLE5011_StartDMA(&sensors[i]);
				return;
			} else if (sensors[i].type == TLE5012) {
				TLE5012_StartDMA(&sensors[i]);
				return;
			} else if (sensors[i].type == MCP3201 ||
			           sensors[i].type == MCP3202 ||
			           sensors[i].type == MCP3204 ||
			           sensors[i].type == MCP3208) {
				MCP320x_StartDMA(&sensors[i], 0);
				return;
			} else if (sensors[i].type == MLX90363) {
				MLX90363_StartDMA(&sensors[i]);
				return;
			} else if (sensors[i].type == MLX90393_SPI) {
				MLX90393_StartDMA(MLX_SPI, &sensors[i]);
				return;
			} else if (sensors[i].type == AS5048A_SPI) {
				AS5048A_StartDMA(&sensors[i]);
				return;
			}
		}
	}
}

void Sensor_OnSpiTxComplete(void)
{
	uint8_t i = 0;

	/* Drain SPI -- TXE then BSY clear. Both registers are direct
	 * access on F1 and F4. */
	while (!(SPI1->SR & SPI_SR_TXE));
	while (SPI1->SR & SPI_SR_BSY);

	for (i = 0; i < MAX_AXIS_NUM; ++i) {
		if (sensors[i].source >= 0 && !sensors[i].tx_complete) {
			sensors[i].tx_complete = 1;
			sensors[i].rx_complete = 0;
			if (sensors[i].type == TLE5011) {
				SPI_HalfDuplex_Receive(&sensors[i].data[2], 6, TLE5011_SPI_MODE);
			}
			if (sensors[i].type == TLE5012) {
				/* TLE5012 needs MOSI tristated to plain floating input
				 * (not AF open-drain) for the sensor's listen turnaround
				 * to settle. F103 used to inline a GPIO_Init here; the
				 * BOARD_TLE5011_BUS_DIR_LISTEN_FLOATING seam keeps the
				 * dispatcher cross-board. */
				Board_TLE5011_BusDir(BOARD_TLE5011_BUS_DIR_LISTEN_FLOATING);
				SPI_HalfDuplex_Receive(&sensors[i].data[2], 4, TLE5012_SPI_MODE);
			}
			break;
		}
	}
}

void Sensor_OnI2cTxComplete(void)
{
	uint8_t i = 0;
	uint32_t ticks = I2C_TIMEOUT;

	/* I2C DMA is already disabled by the IRQ handler before calling us.
	 * What's left: wait BTF, generate STOP, find which sensor finished
	 * its TX, and chain the next ready I2C sensor. */
	while (((I2C2->SR1 & 0x00004) != 0x000004) && --ticks);
	if (ticks == 0) {
		sensors[i].tx_complete = 1;
		sensors[i].rx_complete = 1;
		return;
	}
	ticks = I2C_TIMEOUT;

	I2C2->CR1 |= I2C_CR1_STOP;
	while ((I2C2->CR1 & 0x200) == 0x200 && --ticks);
	if (ticks == 0) {
		sensors[i].tx_complete = 1;
		sensors[i].rx_complete = 1;
		return;
	}

	/* Mark the just-finished I2C sensor's TX as done. */
	for (i = 0; i < MAX_AXIS_NUM; ++i) {
		if (sensors[i].source == (pin_t)SOURCE_I2C && !sensors[i].tx_complete) {
			sensors[i++].tx_complete = 1;
			break;
		}
	}

	/* Process next I2C sensor in the loop. */
	for ( ; i < MAX_AXIS_NUM; ++i) {
		if (sensors[i].source == (pin_t)SOURCE_I2C &&
		    sensors[i].rx_complete && sensors[i].tx_complete)
		{
			int s;
			if (sensors[i].type == AS5600) {
				s = AS5600_StartDMA(&sensors[i]);
				if (s != 0) continue;
				else break;
			} else if (sensors[i].type == ADS1115) {
				s = ADS1115_StartDMA(&sensors[i], sensors[i].curr_channel);
				if (s != 0) continue;
				else break;
			}
		}
	}
}

void Sensor_OnI2cRxComplete(void)
{
	uint8_t i = 0;
	uint32_t ticks = I2C_TIMEOUT;

	I2C2->CR1 |= I2C_CR1_STOP;
	while ((I2C2->CR1 & 0x200) == 0x200 && --ticks);
	if (ticks == 0) {
		sensors[i].tx_complete = 1;
		sensors[i].rx_complete = 1;
		return;
	}

	for (i = 0; i < MAX_AXIS_NUM; ++i) {
		if (sensors[i].source == (pin_t)SOURCE_I2C && !sensors[i].rx_complete) {
			sensors[i].ok_cnt++;
			sensors[i].rx_complete = 1;

			if (sensors[i].type == ADS1115) {
				/* Advance to next channel; mux setting starts another
				 * TX cycle. */
				uint8_t channel = (sensors[i].curr_channel < 3) ? (sensors[i].curr_channel + 1) : 0;
				ADS1115_SetMuxDMA(&sensors[i], channel);
			}
		}
	}
}
