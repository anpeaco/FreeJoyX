/**
  ******************************************************************************
  * @file           : board_pwm.h
  * @brief          : Board-agnostic LED PWM driver API.
  *
  * FreeJoy supports up to 4 LED_PWM channels driven from led_pwm_config[]
  * in dev_config_t. F103 routes them to TIM3_CH1/CH3/CH4 (PA6/PB0/PB1)
  * and optionally TIM1_CH1 (PA8 -- only if PA8 isn't used by Encoder 1
  * or WS2812B). F411 will land in 8b on TIM3 (TIM3 is free since the
  * main tick stays on TIM2 and Fast Encoder 1 owns TIM1); pin AFs differ.
  *
  * Both bringup and per-tick duty-cycle update are encapsulated so the
  * application doesn't need to know which timer carries which channel.
  ******************************************************************************
  */

#ifndef BOARD_PWM_H_
#define BOARD_PWM_H_

#include "common_types.h"

/* Bring up the LED PWM channels per the configured led_pwm_config[] +
 * pins[] layout. Idempotent -- safe to call again after a config write.
 *
 * On boards/configurations with no PWM-capable channels enabled this is
 * a no-op. */
void Board_LedPwm_Init(dev_config_t * p_dev_config);

/* Per-tick update of the PWM duty cycle for each enabled channel. If a
 * channel's led_pwm_config[i].is_axis is true, the duty cycle is scaled
 * by the current axis value; otherwise it's the static configured duty.
 *
 * Called from the main tick. No-op on boards that haven't implemented
 * PWM yet. */
void Board_LedPwm_SetFromAxis(dev_config_t * p_dev_config, analog_data_t * axis_data);

#endif /* BOARD_PWM_H_ */
