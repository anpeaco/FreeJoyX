/**
  ******************************************************************************
  * @file           : board_pwm.c
  * @brief          : F103 LED PWM driver -- StdPeriph TIM3 + optional TIM1.
  *
  * Lifted verbatim from application/Src/periphery.c::Timers_Init and
  * ::PWM_SetFromAxis (Phase 8a). The TIM3-on-PCLK1 and TIM1-on-PCLK2
  * prescaler math, the TIM1-vs-encoder/RGB conflict guard, and the
  * led_pwm_config[] -> channel mapping are all preserved as-is so F103
  * behaviour is byte-identical to the pre-Phase-8a build.
  ******************************************************************************
  */

#include "board_pwm.h"

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_tim.h"
#include "common_defines.h"

void Board_LedPwm_Init(dev_config_t * p_dev_config)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseInitStructure;
	TIM_OCInitTypeDef       TIM_OCInitStructure;
	RCC_ClocksTypeDef       RCC_Clocks;

	RCC_GetClocksFreq(&RCC_Clocks);

	TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;

	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
	TIM_TimeBaseStructInit(&TIM_TimeBaseInitStructure);
	TIM_TimeBaseInitStructure.TIM_Prescaler     = RCC_Clocks.PCLK1_Frequency/100000 - 1;
	TIM_TimeBaseInitStructure.TIM_Period        = 200 - 1;	// 1ms, 1000Hz
	TIM_TimeBaseInitStructure.TIM_ClockDivision = 0;
	TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
	TIM_TimeBaseInit(TIM3, &TIM_TimeBaseInitStructure);
	TIM_ARRPreloadConfig(TIM3, ENABLE);
	TIM_Cmd(TIM3, ENABLE);

	/* PWM TIM3 config */
	// Channel 1
	TIM_OCInitStructure.TIM_Pulse = p_dev_config->led_pwm_config[3].duty_cycle * (TIM_TimeBaseInitStructure.TIM_Period + 1) / 100;
	TIM_OC1Init(TIM3, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
	// Channel 3
	TIM_OCInitStructure.TIM_Pulse = p_dev_config->led_pwm_config[1].duty_cycle * (TIM_TimeBaseInitStructure.TIM_Period + 1) / 100;
	TIM_OC3Init(TIM3, &TIM_OCInitStructure);
	TIM_OC3PreloadConfig(TIM3, TIM_OCPreload_Enable);
	// Channel 4
	TIM_OCInitStructure.TIM_Pulse = p_dev_config->led_pwm_config[2].duty_cycle * (TIM_TimeBaseInitStructure.TIM_Period + 1) / 100;
	TIM_OC4Init(TIM3, &TIM_OCInitStructure);
	TIM_OC4PreloadConfig(TIM3, TIM_OCPreload_Enable);

	// prevent conflict with encoder and rgb timer
	if (p_dev_config->pins[8] == LED_PWM && p_dev_config->pins[10] != LED_RGB_WS2812B && p_dev_config->pins[10] != LED_RGB_PL9823)
	{
		/* PWM TIM1 config */
		RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM1, ENABLE);
		TIM_TimeBaseStructInit(&TIM_TimeBaseInitStructure);
		TIM_TimeBaseInitStructure.TIM_Prescaler     = RCC_Clocks.PCLK2_Frequency/100000 - 1;
		TIM_TimeBaseInitStructure.TIM_Period        = 200 - 1;	// 1ms, 1000Hz
		TIM_TimeBaseInitStructure.TIM_ClockDivision = 0;
		TIM_TimeBaseInitStructure.TIM_CounterMode   = TIM_CounterMode_Up;
		TIM_TimeBaseInit(TIM1, &TIM_TimeBaseInitStructure);
		TIM_ARRPreloadConfig(TIM1, ENABLE);
		TIM_CtrlPWMOutputs(TIM1, ENABLE);
		TIM_Cmd(TIM1, ENABLE);

		// Channel 1
		TIM_OCInitStructure.TIM_Pulse = p_dev_config->led_pwm_config[0].duty_cycle * (TIM_TimeBaseInitStructure.TIM_Period + 1) / 100;
		TIM_OC1Init(TIM1, &TIM_OCInitStructure);
		TIM_OC1PreloadConfig(TIM1, TIM_OCPreload_Enable);
		TIM_OC1PolarityConfig(TIM1, TIM_OCPolarity_High);
	}
}

void Board_LedPwm_SetFromAxis(dev_config_t * p_dev_config, analog_data_t * axis_data)
{
	int32_t tmp32;

	/* PWM TIM3 config */
	// Channel 1
	if (p_dev_config->led_pwm_config[3].is_axis)
	{
		tmp32 = (axis_data[p_dev_config->led_pwm_config[3].axis_num] + 32767)/655;
		TIM_SetCompare1(TIM3, tmp32 * p_dev_config->led_pwm_config[3].duty_cycle * (TIM3->ARR + 1) / 10000);
	}
	else
	{
		TIM_SetCompare1(TIM3, p_dev_config->led_pwm_config[3].duty_cycle * (TIM3->ARR + 1) / 100);
	}

	// Channel 3
	if (p_dev_config->led_pwm_config[1].is_axis)
	{
		tmp32 = (axis_data[p_dev_config->led_pwm_config[1].axis_num] + 32767)/655;
		TIM_SetCompare3(TIM3, tmp32 * p_dev_config->led_pwm_config[1].duty_cycle * (TIM3->ARR + 1) / 10000);
	}
	else
	{
		TIM_SetCompare3(TIM3, p_dev_config->led_pwm_config[1].duty_cycle * (TIM3->ARR + 1) / 100);
	}

	// Channel 4
	if (p_dev_config->led_pwm_config[2].is_axis)
	{
		tmp32 = (axis_data[p_dev_config->led_pwm_config[2].axis_num] + 32767)/655;
		TIM_SetCompare4(TIM3, tmp32 * p_dev_config->led_pwm_config[2].duty_cycle * (TIM3->ARR + 1) / 10000);
	}
	else
	{
		TIM_SetCompare4(TIM3, p_dev_config->led_pwm_config[2].duty_cycle * (TIM3->ARR + 1) / 100);
	}

	/* PWM TIM1 config */
	// Channel 1												// prevent conflicts with encoder and rgb timer
	if (p_dev_config->pins[8] == LED_PWM && p_dev_config->pins[10] != LED_RGB_WS2812B && p_dev_config->pins[10] != LED_RGB_PL9823)
	{
		if (p_dev_config->led_pwm_config[0].is_axis)
		{
			tmp32 = (axis_data[p_dev_config->led_pwm_config[0].axis_num] + 32767)/655;
			TIM_SetCompare1(TIM1, tmp32 * p_dev_config->led_pwm_config[0].duty_cycle * (TIM1->ARR + 1) / 10000);
		}
		else
		{
			TIM_SetCompare1(TIM1, p_dev_config->led_pwm_config[0].duty_cycle * (TIM1->ARR + 1) / 100);
		}
	}
}
