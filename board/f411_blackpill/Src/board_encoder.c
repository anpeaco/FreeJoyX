/**
  ******************************************************************************
  * @file           : board_encoder.c
  * @brief          : F411 implementation of fast hardware-quadrature encoders.
  *
  * Slot 0: TIM1 on PA8 (CH1) / PA9 (CH2), AF1, APB2 bus.
  * Slot 1: TIM4 on PB6 (CH1) / PB7 (CH2), AF2, APB1 bus.
  *
  * The slot/timer/AF mapping matches the F103 layout shipped in Step 1 --
  * F411 just needs a per-pin AF code instead of F103's global AFIO_MAPR
  * remap. Both encoders count on both edges (full x4 quadrature) when
  * BOARD_ENCODER_MODE_4X is requested; x2 mode falls back to TI1-only
  * edge counting like the StdPeriph driver did on F103.
  *
  * Pre-hardware build only -- TIMx CNT readback / direction / wrap have
  * not been verified on a BlackPill yet (none in hand). The clock plan
  * (board_init.c::Board_ClockInit_F411 -> APB1 timer-clock 96 MHz / APB2
  * timer-clock 96 MHz) means no extra prescaling; the encoder runs the
  * timer counter at full timer-clock just like F103.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_tim.h"

#include "board_encoder.h"
#include "common_defines.h"		/* MAX_FAST_ENCODER_NUM */

typedef struct {
	TIM_TypeDef * timer;
	GPIO_TypeDef *gpio_port;
	uint32_t      pin_a;		/* LL_GPIO_PIN_x */
	uint32_t      pin_b;
	uint32_t      af_code;		/* LL_GPIO_AF_x */
	uint8_t       on_apb2;		/* 1 -> APB2 timer (TIM1), 0 -> APB1 timer (TIM4) */
} f411_fast_enc_hw_t;

/* The order matches application/Src/encoders.c::fast_encoder_hw[]; slot
 * indices and pin assignments are part of the locked layout. */
static const f411_fast_enc_hw_t f411_fast_enc_hw[MAX_FAST_ENCODER_NUM] = {
	/* Slot 0: TIM1 / PA8 / PA9, AF1 */
	{ TIM1, GPIOA, LL_GPIO_PIN_8,  LL_GPIO_PIN_9,  LL_GPIO_AF_1, 1 },
	/* Slot 1: TIM4 / PB6 / PB7, AF2 */
	{ TIM4, GPIOB, LL_GPIO_PIN_6,  LL_GPIO_PIN_7,  LL_GPIO_AF_2, 0 },
};

void Board_FastEncoderInit(uint8_t fast_idx, board_encoder_mode_t mode)
{
	if (fast_idx >= MAX_FAST_ENCODER_NUM) return;

	const f411_fast_enc_hw_t * hw = &f411_fast_enc_hw[fast_idx];

	/* GPIO clock: PA8/PA9 -> GPIOA, PB6/PB7 -> GPIOB. */
	if (hw->gpio_port == GPIOA) {
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	} else if (hw->gpio_port == GPIOB) {
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
	}

	/* Timer bus clock: TIM1 on APB2, TIM4 on APB1. */
	if (hw->on_apb2) {
		LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);
	} else {
		LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4);
	}

	/* GPIOs as alternate-function inputs with pull-up. EC11-class
	 * encoders are open-collector; the pull-up matches the F103
	 * StdPeriph GPIO_Mode_IPU configuration the original driver used. */
	LL_GPIO_InitTypeDef gpio = {0};
	gpio.Pin        = hw->pin_a | hw->pin_b;
	gpio.Mode       = LL_GPIO_MODE_ALTERNATE;
	gpio.Speed      = LL_GPIO_SPEED_FREQ_HIGH;
	gpio.OutputType = LL_GPIO_OUTPUT_PUSHPULL;
	gpio.Pull       = LL_GPIO_PULL_UP;
	gpio.Alternate  = hw->af_code;
	LL_GPIO_Init(hw->gpio_port, &gpio);

	/* Timer time base: full 16-bit free-running so CNT wraps mod 65536.
	 * NB: CounterMode is edge-aligned UP (CMS=00), NOT center-aligned.
	 * RM0383 sec 13.4.1 requires CMS=00 in encoder-interface mode --
	 * the F103 sibling (application/Src/encoders.c::EncoderFastInit) sets
	 * TIM_CounterMode_Up | TIM_CounterMode_Down which compiles to
	 * CMS=11 and is a long-standing StdPeriph quirk that happens to work
	 * because SMS overrides DIR. Do not "fix" this back to match F103. */
	LL_TIM_InitTypeDef tim = {0};
	tim.Prescaler         = 0;
	tim.CounterMode       = LL_TIM_COUNTERMODE_UP;
	tim.Autoreload        = 0xFFFFU;
	tim.ClockDivision     = LL_TIM_CLOCKDIVISION_DIV1;
	tim.RepetitionCounter = 0;
	LL_TIM_Init(hw->timer, &tim);

	/* Match F103's TIM_ICPolarity_Falling: both StdPeriph
	 * TIM_ICPolarity_Falling and LL_TIM_IC_POLARITY_FALLING resolve to
	 * CCER CCxP bits set, which selects the falling-edge sense for the
	 * input capture stage. In encoder mode the hardware counts both
	 * edges anyway when SMS = x2/x4; this bit just controls the
	 * inversion sense so the count direction matches the F103 build. */
	LL_TIM_IC_SetPolarity(hw->timer, LL_TIM_CHANNEL_CH1, LL_TIM_IC_POLARITY_FALLING);
	LL_TIM_IC_SetPolarity(hw->timer, LL_TIM_CHANNEL_CH2, LL_TIM_IC_POLARITY_FALLING);

	switch (mode) {
		default:
		case BOARD_ENCODER_MODE_2X:
			LL_TIM_SetEncoderMode(hw->timer, LL_TIM_ENCODERMODE_X2_TI1);
			break;
		case BOARD_ENCODER_MODE_4X:
			LL_TIM_SetEncoderMode(hw->timer, LL_TIM_ENCODERMODE_X4_TI12);
			break;
	}

	LL_TIM_SetCounter(hw->timer, 0);
	LL_TIM_EnableCounter(hw->timer);
}

int16_t Board_FastEncoderGetCount(uint8_t fast_idx)
{
	if (fast_idx >= MAX_FAST_ENCODER_NUM) return 0;
	return (int16_t)LL_TIM_GetCounter(f411_fast_enc_hw[fast_idx].timer);
}
