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

#define FIRMWARE_VERSION					0x1770			// v1.7.7 (FreeJoyX Phase 7: dev_config_t gains uint8_t board_id + uint8_t reserved_layout immediately after firmware_version; params_report_t gains the same. Lets the configurator dispatch per-board pin tables and reject cross-board writes. 0x1770 crosses the &0xFFF0 mask boundary so the version-mismatch check at main.c:67 fires once on first flash and factory-resets across the layout change.)
#define USED_PINS_NUM							30					// constant for BluePill and BlackPill boards

/* Board identity tags. Stored in dev_config_t.board_id (persisted in
 * config flash) and broadcast in params_report_t.board_id (read by the
 * configurator on every params report). The per-target BOARD_ID resolves
 * via board/<chip>/Inc/board_config.h so the firmware tags itself
 * automatically; the configurator only ever reads paramsReport.board_id
 * and never resolves BOARD_ID itself. */
#define BOARD_ID_F103_BLUEPILL				1
#define BOARD_ID_F411_BLACKPILL				2
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
