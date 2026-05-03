/**
  ******************************************************************************
  * @file           : board_misc.h
  * @brief          : Small board-specific helpers used from shared application code.
  *
  * Each function here exists to keep one F1-specific block out of
  * application/Src/usb_app.c (the shared tick + OUT dispatch). When the
  * F411 path needs an equivalent, it lands as the F411 implementation;
  * until then the F411 stub is a no-op.
  ******************************************************************************
  */

#ifndef BOARD_MISC_H_
#define BOARD_MISC_H_

#include <stdint.h>
#include "common_types.h"

/* Toggle clock gates on neighbouring peripherals across an ADC sample
 * window to reduce noise injected onto ADC1's analog inputs. F103 turns
 * off SPI1 / I2C2 / TIM3 / TIM4 / GPIOB / GPIOC clocks while ADC samples
 * (re-enables after). F411 no-ops -- the F411 ADC runtime path isn't
 * wired yet, and even when it lands the noise budget on the BlackPill
 * is different (separate analog supply, different clock tree).
 *
 * quiet=1: gate clocks off (entering ADC sample window)
 * quiet=0: gate clocks back on (exiting ADC sample window)
 *
 * The app_config_t pointer lets the F103 impl skip gating peripherals
 * the user has actually configured -- e.g. don't disable TIM1 if a fast
 * encoder needs it. */
void Board_AdcQuietPeripherals(uint8_t quiet, const app_config_t *cfg);

/* Surface a configurator firmware-version-mismatch refusal on the
 * onboard LED so the user notices. F103: blink PB12/PC13 6x at ~3 Hz
 * (1.2 s of busy-wait). F411: no-op for now (PC13 onboard LED is single
 * pin and used differently; replacement strategy -- a single short
 * blink or status code on the device-info card -- arrives in a polish
 * pass post-hardware). */
void Board_VersionMismatchBlink(void);

#endif /* BOARD_MISC_H_ */
