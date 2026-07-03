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

/* Surface a configurator firmware-version-mismatch refusal on the
 * onboard LED so the user notices. F103: blink PB12/PC13 6x at ~3 Hz
 * (1.2 s of busy-wait). F411: no-op for now (PC13 onboard LED is single
 * pin and used differently; replacement strategy -- a single short
 * blink or status code on the device-info card -- arrives in a polish
 * pass post-hardware). */
void Board_VersionMismatchBlink(void);

#endif /* BOARD_MISC_H_ */
