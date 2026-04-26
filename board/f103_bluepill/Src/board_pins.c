/**
  ******************************************************************************
  * @file           : board_pins.c
  * @brief          : F103 BluePill pin map.
  *
  * Maps each USED_PINS_NUM-indexed slot in dev_config_t.pins[] to its
  * physical GPIO port + pin + bit position. Application code references this
  * table via the extern declaration in application/Inc/periphery.h. The F411
  * port (Phase 5) provides its own version of this table at
  * board/f411_blackpill/Src/board_pins.c with the same shape but the F411
  * pin assignments (see F411_PORT_PLAN.md "Pin map" section).
  *
  * Pin slot allocations are part of the wire format (configurator and
  * firmware agree on the slot index for each role). Don't reorder.
  ******************************************************************************
  */

#include "periphery.h"

pin_config_t pin_config[USED_PINS_NUM] =
{
	{GPIOA, GPIO_Pin_0,  0},		// 0
	{GPIOA, GPIO_Pin_1,  1},		// 1
	{GPIOA, GPIO_Pin_2,  2},		// 2
	{GPIOA, GPIO_Pin_3,  3},		// 3
	{GPIOA, GPIO_Pin_4,  4},		// 4
	{GPIOA, GPIO_Pin_5,  5},		// 5
	{GPIOA, GPIO_Pin_6,  6},		// 6
	{GPIOA, GPIO_Pin_7,  7},		// 7
	{GPIOA, GPIO_Pin_8,  8},		// 8 -- Fast Encoder 1 A (TIM1_CH1)
	{GPIOA, GPIO_Pin_9,  9},		// 9 -- Fast Encoder 1 B (TIM1_CH2)
	{GPIOA, GPIO_Pin_10, 10},		// 10
	{GPIOA, GPIO_Pin_15, 15},		// 11
	{GPIOB, GPIO_Pin_0,  0},		// 12
	{GPIOB, GPIO_Pin_1,  1},		// 13
	{GPIOB, GPIO_Pin_3,  3},		// 14
	{GPIOB, GPIO_Pin_4,  4},		// 15
	{GPIOB, GPIO_Pin_5,  5},		// 16
	{GPIOB, GPIO_Pin_6,  6},		// 17 -- Fast Encoder 2 A (TIM4_CH1)
	{GPIOB, GPIO_Pin_7,  7},		// 18 -- Fast Encoder 2 B (TIM4_CH2)
	{GPIOB, GPIO_Pin_8,  8},		// 19
	{GPIOB, GPIO_Pin_9,  9},		// 20
	{GPIOB, GPIO_Pin_10, 10},		// 21
	{GPIOB, GPIO_Pin_11, 11},		// 22
	{GPIOB, GPIO_Pin_12, 12},		// 23
	{GPIOB, GPIO_Pin_13, 13},		// 24
	{GPIOB, GPIO_Pin_14, 14},		// 25
	{GPIOB, GPIO_Pin_15, 15},		// 26
	{GPIOC, GPIO_Pin_13, 13},		// 27
	{GPIOC, GPIO_Pin_14, 14},		// 28
	{GPIOC, GPIO_Pin_15, 15},		// 29
};
