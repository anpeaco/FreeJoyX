/**
  ******************************************************************************
  * @file           : gpio_expander.h
  * @brief          : 16-bit GPIO expander button-input source (MCP23017 / MCP23S17).
  ******************************************************************************
  *
  * Each enabled expander contributes up to 16 buttons to the physical-button
  * scan -- modelled on the shift-register subsystem. Two transports share one
  * config pool (dev_config_t.gpio_expanders[]) and the same register map:
  *   MCP23017 -- I2C, selected by I2C address (0x20..0x27), on the I2C bus.
  *   MCP23S17 -- SPI, selected by a CS pin (SPI_GPIO_CS role, matched to SPI-
  *               type slots in pin order, like shift-register control pins),
  *               on the SPI bus.
  *
  * Producer / consumer split (the button read runs in Board_TickISR, where
  * blocking bus access is unsafe):
  *   - GpioExp_Process() does the BLOCKING read (I2C or DMA-polled SPI) and is
  *     called from the main() super-loop. It self-gates on the bus being idle
  *     (no sensor transfer in flight) and caches each chip's GPIOA/GPIOB.
  *   - GpioExp_Get() runs inside the tick ISR (via ButtonsReadPhysical) and only
  *     folds the cached bytes into raw_button_data_buf -- pure memory, no bus.
  *
  * See MCP23017_PLAN.md.
  ******************************************************************************
  */

#ifndef __GPIO_EXPANDER_H__
#define __GPIO_EXPANDER_H__

#include "common_types.h"

/* flags bits in gpio_expander_t.flags */
#define GPIO_EXP_FLAG_PULLUPS		0x01	/* enable internal pull-ups (GPPU) */
#define GPIO_EXP_FLAG_INVERT		0x02	/* invert input polarity (IPOL) */

/* One-shot register setup for every enabled expander (IODIR/GPPU/IPOL, plus the
 * CS-pin match for SPI chips). Call once at config-apply, before sensor DMA. */
void GpioExp_Init (dev_config_t * p_dev_config);

/* Blocking read of GPIOA/GPIOB for every present expander into the cache. Call
 * from the main super-loop. No-op for the tick if a sensor transfer is in
 * flight (the cache holds its previous value). */
void GpioExp_Process (dev_config_t * p_dev_config);

/* Fold the cached expander bits into raw_button_data_buf at *pos. ISR-safe. */
void GpioExp_Get (uint8_t * raw_button_data_buf, dev_config_t * p_dev_config, uint8_t * pos);

/* Pure bit-unpack helper (no globals -> host-testable): write the low
 * `button_cnt` bits of `gpio` (GPIOA = bits 0..7, GPIOB = bits 8..15) into
 * raw_button_data_buf[*pos..], advancing *pos, never exceeding `max`. */
void GpioExp_FoldBits (uint16_t gpio, uint8_t button_cnt,
                       uint8_t * raw_button_data_buf, uint8_t * pos, uint8_t max);

/* Set non-zero by GpioExp_Process for the duration of an expander bus transfer.
 * The expander shares the SPI/I2C peripheral (and the SPI DMA channels) with the
 * sensors, but its blocking read runs in the main loop while the sensor DMA is
 * armed from Board_TickISR -- so BusIdle() alone isn't atomic against a tick
 * preempting between the gate and the transfer. The tick-ISR sensor kickoff and
 * the shared DMA-completion dispatch (Sensor_OnSpi{Rx,Tx}Complete) both honour
 * this flag: while it is set, no sensor transfer is started or re-armed, so the
 * expander owns the bus uncontended. Single-byte volatile -> atomic on Cortex-M;
 * written only by the main loop, read by the ISR. */
extern volatile uint8_t gpio_exp_bus_busy;

#endif	/* __GPIO_EXPANDER_H__ */
