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

#define FIRMWARE_VERSION					0x1733			// v1.7.3b3 (FreeJoyX: button_t gains LOGIC type, op, src_b; type widened to full byte)
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

// same in usb_hw.h
#define MAX_PAGE									64
#define FLASH_PAGE_SIZE						1024
#define FLASH_PAGE_END_ADDR				(0x8000000 + (MAX_PAGE * FLASH_PAGE_SIZE))
#define CONFIG_PAGE_COUNT					2		// resize config here
#define CONFIG_ADDR								(FLASH_PAGE_END_ADDR - (CONFIG_PAGE_COUNT * FLASH_PAGE_SIZE))
//#define CONFIG_ADDR								(0x0800F800)//(0x0800FC00)


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
