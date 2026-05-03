/**
  ******************************************************************************
  * @file           : board_encoder.h
  * @brief          : Board-agnostic fast (hardware-quadrature) encoder API.
  *
  * FreeJoy supports up to MAX_FAST_ENCODER_NUM hardware-quadrature
  * encoders. Each board owns the timer choice, GPIO routing, and AF code
  * for its encoder slots; the application just asks the board to bring
  * up slot N in the requested mode and read its count back.
  *
  * Slot mapping (locked decision in CLAUDE.md / F411_PORT_PLAN.md):
  *   Slot 0 -> Encoder 1, PA8 (CH1) / PA9 (CH2)
  *     F103: TIM1, AFIO partial remap
  *     F411: TIM1, per-pin AF1
  *   Slot 1 -> Encoder 2, PB6 (CH1) / PB7 (CH2)
  *     F103: TIM4
  *     F411: TIM4, per-pin AF2
  *
  * Phase 5b will retire encoders.c::EncoderFastInit on F103 in favour of
  * this seam; for now F411 is the only implementer.
  ******************************************************************************
  */

#ifndef BOARD_ENCODER_H_
#define BOARD_ENCODER_H_

#include <stdint.h>

typedef enum {
	BOARD_ENCODER_MODE_2X = 0,	/* Count on TI1 edges only (single-edge, x2 quadrature) */
	BOARD_ENCODER_MODE_4X = 1,	/* Count on both TI1 and TI2 edges (x4 quadrature) */
} board_encoder_mode_t;

/* Bring up fast encoder slot fast_idx: enable timer/GPIO clocks, set the
 * pins to AF input, configure the timer in encoder-interface mode at the
 * requested edge sensitivity, zero CNT, start the counter. Idempotent --
 * safe to call again with a different mode. */
void Board_FastEncoderInit(uint8_t fast_idx, board_encoder_mode_t mode);

/* Read the signed 16-bit counter for slot fast_idx. Wraps modulo 2^16
 * by hardware -- callers track wrap themselves. Returns 0 if fast_idx is
 * out of range. */
int16_t Board_FastEncoderGetCount(uint8_t fast_idx);

#endif /* BOARD_ENCODER_H_ */
