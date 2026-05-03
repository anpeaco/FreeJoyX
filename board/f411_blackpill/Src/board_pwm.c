/**
  ******************************************************************************
  * @file           : board_pwm.c
  * @brief          : F411 LED PWM driver -- Phase 8a stub.
  *
  * Real LL TIM3 implementation lands in Phase 8b. F411 has TIM3 free
  * (main tick on TIM2, Encoder 1 on TIM1, Encoder 2 on TIM4), so the
  * channel mapping mirrors F103: TIM3_CH1/CH3/CH4 plus optional TIM1_CH1
  * when PA8 isn't claimed by an encoder or WS2812B.
  ******************************************************************************
  */

#include "board_pwm.h"

void Board_LedPwm_Init(dev_config_t * p_dev_config)
{
	(void)p_dev_config;
}

void Board_LedPwm_SetFromAxis(dev_config_t * p_dev_config, analog_data_t * axis_data)
{
	(void)p_dev_config; (void)axis_data;
}
