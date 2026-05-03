/**
  ******************************************************************************
  * @file           : common_defines.h
  * @brief          : This file contains the common defines for the app.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __COMMON_DEFINES_H__
#define __COMMON_DEFINES_H__

//#define DEBUG

#define FIRMWARE_VERSION					0x1760			// v1.7.6 (FreeJoyX: dev_config_t gains saved_breakdown -- a 14-byte configurator-only metadata snapshot of how button slots were divided across categories at the moment of the last save. Lets the configurator detect breakdown drift across save/load cycles and remap stale physical_num references through freejoy::toRef/toAbs. Firmware allocates the bytes but never reads them. 0x1760 trips the &0xFFF0 mask check at main.c:64 to factory-reset across the layout change.)
#define USED_PINS_NUM							30					// constant for BluePill and BlackPill boards
#define MAX_AXIS_NUM							8						// max 8
#define MAX_BUTTONS_NUM						128					// power of 2, max 128
#define MAX_POVS_NUM							4						// max 4
#define MAX_ENCODERS_NUM					16					// max 64
#define MAX_FAST_ENCODER_NUM			2						// hardware-quadrature encoders (Enc 1 = TIM1/PA8/PA9, Enc 2 = TIM4/PB6/PB7).
#define MAX_SHIFT_REG_NUM					4						// max 4
#define MAX_LEDS_NUM							24
#define NUM_RGB_LEDS    					50					// if increase dont forget calc config size CONFIG_PAGE_COUNT
#define NUM_RGB_LEDS_SH						20

#define AXIS_MIN_VALUE						(-32767)
#define AXIS_MAX_VALUE						(32767)
#define AXIS_CENTER_VALUE					(AXIS_MIN_VALUE + (AXIS_MAX_VALUE-AXIS_MIN_VALUE)/2)
#define AXIS_FULLSCALE						(AXIS_MAX_VALUE - AXIS_MIN_VALUE + 1)

// Flash storage layout (MAX_PAGE / FLASH_PAGE_SIZE / CONFIG_ADDR / etc.)
// moved to board/<chip>/Inc/board_config.h as part of the F411 BSP-seam
// refactor. The constants are inherently chip-specific.
#include "board_config.h"


enum
{
	REPORT_ID_JOY = 1,
	REPORT_ID_PARAM,
	REPORT_ID_CONFIG_IN,
	REPORT_ID_CONFIG_OUT,
	REPORT_ID_FIRMWARE,
	REPORT_ID_LED,
};


#endif 	/* __COMMON_DEFINES_H__ */
