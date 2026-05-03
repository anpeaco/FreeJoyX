/**
  ******************************************************************************
  * @file           : board_encoder.c
  * @brief          : F103 implementation of fast hardware-quadrature encoders.
  *
  * Slot 0: TIM1 on PA8 (CH1) / PA9 (CH2). APB2 bus. Routed via the global
  *         AFIO partial remap that periphery.c::IO_Init applies once at
  *         startup -- F103 has no per-pin AF code, so the BSP doesn't
  *         touch GPIO setup here.
  * Slot 1: TIM4 on PB6 (CH1) / PB7 (CH2). APB1 bus. No remap needed.
  *
  * Hoisted out of application/Src/encoders.c (Phase 5b) so encoders.c is
  * board-agnostic and the F411 BSP can implement the same seam in LL.
  ******************************************************************************
  */

#include "stm32f10x.h"
#include "stm32f10x_conf.h"

#include "board_encoder.h"
#include "common_defines.h"		/* MAX_FAST_ENCODER_NUM */

typedef struct {
	TIM_TypeDef * timer;
	uint8_t       on_apb2;		/* 1 -> APB2 (TIM1), 0 -> APB1 (TIM4) */
} f103_fast_enc_hw_t;

static const f103_fast_enc_hw_t f103_fast_enc_hw[MAX_FAST_ENCODER_NUM] = {
	{ TIM1, 1 },	/* Encoder 1 -- PA8/PA9 */
	{ TIM4, 0 },	/* Encoder 2 -- PB6/PB7 */
};

void Board_FastEncoderInit(uint8_t fast_idx, board_encoder_mode_t mode)
{
	if (fast_idx >= MAX_FAST_ENCODER_NUM) return;

	TIM_TypeDef * timer = f103_fast_enc_hw[fast_idx].timer;

	if (f103_fast_enc_hw[fast_idx].on_apb2) {
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
	} else {
		RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
	}

	TIM_TimeBaseInitTypeDef base;
	base.TIM_Prescaler     = 0;
	base.TIM_Period        = 65535;
	base.TIM_ClockDivision = 0;
	/* StdPeriph encoder driver tolerates the unusual CMS=11 (CounterMode_Up |
	 * Down) here -- SMS overrides DIR in encoder mode regardless. The F411
	 * sibling uses the spec-correct CMS=00 per RM0383 13.4.1; do not "fix"
	 * this F103 line to match F411 unless you also re-verify on hardware. */
	base.TIM_CounterMode   = TIM_CounterMode_Up | TIM_CounterMode_Down;
	TIM_TimeBaseInit(timer, &base);

	switch (mode)
	{
		default:
		case BOARD_ENCODER_MODE_2X:
			TIM_EncoderInterfaceConfig(timer, TIM_EncoderMode_TI1,
				TIM_ICPolarity_Falling, TIM_ICPolarity_Falling);
			break;
		case BOARD_ENCODER_MODE_4X:
			TIM_EncoderInterfaceConfig(timer, TIM_EncoderMode_TI12,
				TIM_ICPolarity_Falling, TIM_ICPolarity_Falling);
			break;
	}

	timer->CNT = 0;
	TIM_Cmd(timer, ENABLE);
}

int16_t Board_FastEncoderGetCount(uint8_t fast_idx)
{
	if (fast_idx >= MAX_FAST_ENCODER_NUM) return 0;
	return (int16_t)f103_fast_enc_hw[fast_idx].timer->CNT;
}
