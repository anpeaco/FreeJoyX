/**
  ******************************************************************************
  * @file           : board_pwm.c
  * @brief          : F411 LED PWM driver -- LL TIM3 (+ optional TIM1_CH1).
  *
  * F411 clock plan (board_init.c::Board_ClockInit_F411):
  *   APB1 timer-clock = 96 MHz   (TIM3)
  *   APB2 timer-clock = 96 MHz   (TIM1)
  *
  * Channel-to-config mapping mirrors F103 verbatim (board/f103_bluepill/Src/
  * board_pwm.c):
  *   TIM3_CH1 (PA6, AF2)  -> led_pwm_config[3]
  *   TIM3_CH3 (PB0, AF2)  -> led_pwm_config[1]
  *   TIM3_CH4 (PB1, AF2)  -> led_pwm_config[2]
  *   TIM1_CH1 (PA8, AF1)  -> led_pwm_config[0]   (only when pins[8]==LED_PWM
  *                                                and pins[10] isn't an RGB
  *                                                driver -- mutex with
  *                                                Encoder 1 + WS2812B)
  *
  * Period 200 @ 100 kHz tick -> 1 kHz PWM, 0.5% duty resolution -- matches
  * F103. IO_Init (application/Src/periphery.c) sets each LED_PWM-roled slot
  * to BOARD_GPIO_AF_PUSHPULL via Board_PinSetMode, but F411's Board_PinSetMode
  * doesn't write the AF code (per its top-of-file note in board_pins.c).
  * We finish the per-pin AF setup here for the four valid LED_PWM pins.
  *
  * Pre-hardware build only -- behaviour not verified on a BlackPill yet.
  ******************************************************************************
  */

#include "board_pwm.h"

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_tim.h"

#define PWM_TIMER_TICK_HZ    100000U     /* 100 kHz intermediate counter */
#define PWM_TIMER_PERIOD     200U        /* 1 ms cycle -> 1 kHz output */

static void f411_pwm_config_channel(TIM_TypeDef *tim, uint32_t channel, uint16_t pulse)
{
	LL_TIM_OC_InitTypeDef oc = {0};
	oc.OCMode       = LL_TIM_OCMODE_PWM1;
	oc.OCState      = LL_TIM_OCSTATE_ENABLE;
	oc.OCNState     = LL_TIM_OCSTATE_DISABLE;
	oc.OCPolarity   = LL_TIM_OCPOLARITY_HIGH;
	oc.OCNPolarity  = LL_TIM_OCPOLARITY_HIGH;
	oc.OCIdleState  = LL_TIM_OCIDLESTATE_LOW;
	oc.OCNIdleState = LL_TIM_OCIDLESTATE_LOW;
	oc.CompareValue = pulse;
	LL_TIM_OC_Init(tim, channel, &oc);
	LL_TIM_OC_EnablePreload(tim, channel);
}

static uint16_t f411_pwm_static_pulse(uint8_t cfg_idx, dev_config_t *p)
{
	return (uint16_t)((uint32_t)p->led_pwm_config[cfg_idx].duty_cycle * PWM_TIMER_PERIOD / 100U);
}

void Board_LedPwm_Init(dev_config_t * p_dev_config)
{
	/* TIM3 brought up unconditionally (mirrors F103). When no slot is
	 * tagged LED_PWM the channels just toggle internally with no pin
	 * driven, harmless. */
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM3);

	LL_TIM_InitTypeDef tim = {0};
	tim.Prescaler         = (96000000U / PWM_TIMER_TICK_HZ) - 1U;
	tim.CounterMode       = LL_TIM_COUNTERMODE_UP;
	tim.Autoreload        = PWM_TIMER_PERIOD - 1U;
	tim.ClockDivision     = LL_TIM_CLOCKDIVISION_DIV1;
	tim.RepetitionCounter = 0;
	LL_TIM_Init(TIM3, &tim);
	LL_TIM_EnableARRPreload(TIM3);

	f411_pwm_config_channel(TIM3, LL_TIM_CHANNEL_CH1, f411_pwm_static_pulse(3, p_dev_config));
	f411_pwm_config_channel(TIM3, LL_TIM_CHANNEL_CH3, f411_pwm_static_pulse(1, p_dev_config));
	f411_pwm_config_channel(TIM3, LL_TIM_CHANNEL_CH4, f411_pwm_static_pulse(2, p_dev_config));

	LL_TIM_EnableCounter(TIM3);

	/* Per-pin AF setup. Only PA6/PB0/PB1 (TIM3 AF2) carry the LED_PWM role
	 * by F411 design; PA8 (TIM1 AF1) is handled in the conditional TIM1
	 * block below. Other slots tagged LED_PWM are nonsense (configurator
	 * filters them) and silently ignored. */
	if (p_dev_config->pins[6] == LED_PWM) {
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
		LL_GPIO_SetAFPin_0_7(GPIOA, LL_GPIO_PIN_6, LL_GPIO_AF_2);
	}
	if (p_dev_config->pins[12] == LED_PWM) {
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
		LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_0, LL_GPIO_AF_2);
	}
	if (p_dev_config->pins[13] == LED_PWM) {
		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);
		LL_GPIO_SetAFPin_0_7(GPIOB, LL_GPIO_PIN_1, LL_GPIO_AF_2);
	}

	/* TIM1_CH1 (PA8) only when slot 8 is LED_PWM AND PA10 isn't an RGB
	 * driver. PA10 is TIM1_CH3 -- WS2812B grabs the whole TIM1, so we must
	 * not bring up TIM1 for PWM in that case. Same arbitration as F103. */
	if (p_dev_config->pins[8] == LED_PWM &&
	    p_dev_config->pins[10] != LED_RGB_WS2812B &&
	    p_dev_config->pins[10] != LED_RGB_PL9823)
	{
		LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);

		tim.Prescaler  = (96000000U / PWM_TIMER_TICK_HZ) - 1U;
		tim.Autoreload = PWM_TIMER_PERIOD - 1U;
		LL_TIM_Init(TIM1, &tim);
		LL_TIM_EnableARRPreload(TIM1);

		f411_pwm_config_channel(TIM1, LL_TIM_CHANNEL_CH1, f411_pwm_static_pulse(0, p_dev_config));

		LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
		LL_GPIO_SetAFPin_8_15(GPIOA, LL_GPIO_PIN_8, LL_GPIO_AF_1);

		/* TIM1 is an advanced-control timer: outputs are gated by BDTR.MOE.
		 * Without this the CCxE bits do nothing and the pin stays idle. */
		LL_TIM_EnableAllOutputs(TIM1);
		LL_TIM_EnableCounter(TIM1);
	}
}

static uint16_t f411_pwm_axis_pulse(uint8_t cfg_idx, dev_config_t *p, analog_data_t *axis_data, uint16_t arr)
{
	if (p->led_pwm_config[cfg_idx].is_axis)
	{
		int32_t v = (axis_data[p->led_pwm_config[cfg_idx].axis_num] + 32767) / 655;
		return (uint16_t)(v * p->led_pwm_config[cfg_idx].duty_cycle * (arr + 1) / 10000);
	}
	return (uint16_t)(p->led_pwm_config[cfg_idx].duty_cycle * (arr + 1) / 100);
}

void Board_LedPwm_SetFromAxis(dev_config_t * p_dev_config, analog_data_t * axis_data)
{
	uint16_t arr3 = (uint16_t)LL_TIM_GetAutoReload(TIM3);
	LL_TIM_OC_SetCompareCH1(TIM3, f411_pwm_axis_pulse(3, p_dev_config, axis_data, arr3));
	LL_TIM_OC_SetCompareCH3(TIM3, f411_pwm_axis_pulse(1, p_dev_config, axis_data, arr3));
	LL_TIM_OC_SetCompareCH4(TIM3, f411_pwm_axis_pulse(2, p_dev_config, axis_data, arr3));

	if (p_dev_config->pins[8] == LED_PWM &&
	    p_dev_config->pins[10] != LED_RGB_WS2812B &&
	    p_dev_config->pins[10] != LED_RGB_PL9823)
	{
		uint16_t arr1 = (uint16_t)LL_TIM_GetAutoReload(TIM1);
		LL_TIM_OC_SetCompareCH1(TIM1, f411_pwm_axis_pulse(0, p_dev_config, axis_data, arr1));
	}
}
