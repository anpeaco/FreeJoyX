/**
  ******************************************************************************
  * @file           : mcp23017.h
  * @brief          : MCP23017 I2C GPIO expander as a button input source.
  ******************************************************************************
  *
  * Each enabled expander contributes up to 16 buttons into the physical-button
  * scan -- modelled on the shift-register subsystem (shift_registers.c), but the
  * inputs arrive over the shared I2C bus instead of a bit-banged latch/clk/data.
  *
  * Producer / consumer split (mirrors the rest of the firmware):
  *   - I2cGpioProcess() does the BLOCKING I2C read and is called from the
  *     main() super-loop, where blocking is safe. It self-gates on the I2C bus
  *     being idle (no sensor DMA in flight) and caches each chip's GPIOA/GPIOB.
  *   - I2cGpioGet() runs inside Board_TickISR (via ButtonsReadPhysical) and only
  *     folds the cached bytes into raw_button_data_buf -- pure memory, no I2C.
  *
  * See MCP23017_PLAN.md.
  ******************************************************************************
  */

#ifndef __MCP23017_H__
#define __MCP23017_H__

#include "common_types.h"

/* flags bits in gpio_expander_t.flags */
#define I2C_GPIO_FLAG_PULLUPS		0x01	/* enable the chip's internal pull-ups (GPPU) */
#define I2C_GPIO_FLAG_INVERT		0x02	/* invert input polarity (IPOL) */

/* One-shot register setup for every enabled expander (IODIR/GPPU/IPOL). Call
 * once at config-apply, before any sensor DMA is running. */
void I2cGpioInit (dev_config_t * p_dev_config);

/* Blocking read of GPIOA/GPIOB for every present expander into the local cache.
 * Call from the main super-loop. No-op for the current tick if an I2C sensor
 * transfer is in flight (the cache simply holds its previous value). */
void I2cGpioProcess (dev_config_t * p_dev_config);

/* Fold the cached expander bits into raw_button_data_buf at *pos, advancing
 * *pos by each enabled chip's button_cnt. Safe to call from the tick ISR. */
void I2cGpioGet (uint8_t * raw_button_data_buf, dev_config_t * p_dev_config, uint8_t * pos);

/* Pure bit-unpack helper (no globals -> host-testable): write the low
 * `button_cnt` bits of `gpio` (GPIOA = bits 0..7, GPIOB = bits 8..15) into
 * raw_button_data_buf[*pos..], advancing *pos, never exceeding `max`. */
void I2cGpio_FoldBits (uint16_t gpio, uint8_t button_cnt,
                       uint8_t * raw_button_data_buf, uint8_t * pos, uint8_t max);

#endif	/* __MCP23017_H__ */
