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

#define FIRMWARE_VERSION					0x0040			// FreeJoyX wire-format generation 4: added slow_encoders[MAX_ENCODERS_NUM] ({int8 btn_a, int8 btn_b} explicit slow-encoder pin pairing) appended to the END of dev_config_t, replacing the old positional zip of ENCODER_INPUT_A/_B button slots (encoders.c scan-and-zip removed); direction is set by the Pin A/Pin B order in slow_encoders[] (the configurator's Swap button exchanges the two pins -- no swap flag), detent mode in encoders[i] masked with SLOW_ENC_MODE_MASK. Old shape is the byte-exact prefix, so 0x0030->0x0040 migration is prefix-copy + synthesise pairs via the old positional algorithm; offsetof(dev_config_t, slow_encoders) == the old size (1620). Crosses the &0xFFF0 mask -> factory reset on first flash. Enum values ENCODER_INPUT_A (219) and ENCODER_INPUT_B (220) are both retained (removing 220 would shift RADIO_BUTTON1..); firmware treats both as "encoder line". ENCODER_PAIRING_PLAN.md. --- Gen 3 note (0x0030): added gpio_expanders[MAX_GPIO_EXPANDER_NUM] (MCP23017 I2C + MCP23S17 SPI, 8 slots) appended; offsetof(dev_config_t, gpio_expanders) == old size 1580; forward migrators read 0x0020/0x0010/0x17xx via the same prefix path. MCP23017_PLAN.md. --- Gen 2 note (0x0020): dev_config_t SHAPE unchanged from 0x0010 -- the bump is for SEMANTIC drift: the enum value formerly named LONG_PRESS (hold-style, "fires after threshold") was renamed to TAP and reinterpreted as release-within-cutoff ("fires on release before window expires"). Same integer enum slot, same byte position in config, different gesture behaviour. Bumping the mask group forces factory reset on first flash so a user's existing buttons don't silently change behaviour mid-upgrade. The forward migrator chain covers 0x0010 (FreeJoyX v0.0.x, LONG_PRESS semantics) plus the upstream 0x1700/0x1710/0x1730/0x1770/0x1780 lineage.

/* FREEJOYX_VERSION is the user-facing project version (semver). It's
 * decoupled from FIRMWARE_VERSION above -- FIRMWARE_VERSION is the
 * wire-format compat key (drives the &0xFFF0 mismatch check), while
 * FREEJOYX_VERSION is what appears in the configurator title bar, the
 * device's USB device_name on factory reset, and release artefact
 * filenames. Both bump together in coordinated releases; configurator-
 * only or firmware-only fixes bump the patch number alone.
 *
 * Until the first formal approved release we stay on major 0. Move to
 * 1.0.0 when the project is judged stable. See issue anpeaco/FreeJoyX#18. */
#define FREEJOYX_VERSION_MAJOR              0
#define FREEJOYX_VERSION_MINOR              2
#define FREEJOYX_VERSION_PATCH              0
#define FREEJOYX_VER_STR_HELPER(x)          #x
#define FREEJOYX_VER_STR(x)                 FREEJOYX_VER_STR_HELPER(x)
#define FREEJOYX_VERSION                    FREEJOYX_VER_STR(FREEJOYX_VERSION_MAJOR) "." \
                                            FREEJOYX_VER_STR(FREEJOYX_VERSION_MINOR) "." \
                                            FREEJOYX_VER_STR(FREEJOYX_VERSION_PATCH)

/* Wire-format size pins. Must move in lockstep with FIRMWARE_VERSION on
 * any change to dev_config_t / params_report_t. The static_assert lines
 * at the bottom of common_types.h fail the build if the struct shape
 * drifts without bumping these. Sister rule lives in CLAUDE.md
 * ("Wire-format archival rule"). */
#define FREEJOY_DEV_CONFIG_SIZE				1652			/* 1580 -> 1612: +32 for gpio_expanders[MAX_GPIO_EXPANDER_NUM] (8 x 4B MCP23017/MCP23S17 expander slots); 1612 -> 1620: +8 for saved_per_exp[MAX_GPIO_EXPANDER_NUM] (per-expander remap snapshot); 1620 -> 1652: +32 for slow_encoders[MAX_ENCODERS_NUM] (16 x 2B {int8 btn_a, int8 btn_b} explicit slow-encoder pairs). All appended at the end of dev_config_t, so the old (0x0030) size 1620 still == offsetof(dev_config_t, slow_encoders) and the prefix migration is unchanged. */
/* 72 -> 88: params_report_t gained detect_axis_raw[MAX_AXIS_NUM] (8 * int16)
 * for axis auto-detect (AXIS_DETECT_PLAN.md). params-report-only change --
 * dev_config_t is untouched, so no FIRMWARE_VERSION 0xFFF0 cross / factory
 * reset; the field is appended (prefix-compatible) and gated on
 * freejoyx_version >= 0.1.3. */
#define FREEJOY_PARAMS_REPORT_SIZE			88

/* Maximum number of shift modifiers. v1.7.8: bumped 5 -> 8 to match
 * button_t.shift_modificator's widened :4 field (encodes 0=none, 1..8).
 * shift_config[i] uses bit i in the runtime shifts_state bitmap, so 8
 * slots fits in the existing uint8_t. Issue anpeaco/FreeJoyX#1. */
#define MAX_SHIFTS_NUM						8

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
/* Slow-encoder detent mode in dev_config_t.encoders[i]: bits 0-1 = mode
 * (ENCODER_CONF_1x/2x/4x); high bits reserved. Direction is set purely by the
 * Pin A / Pin B order in slow_encoders[] (the configurator's Swap button
 * exchanges the two pins), so no direction-swap flag is stored. The mask
 * defends against a stray high bit from an interim build. Mirror of
 * FreeJoyXConfiguratorQt/src/common_defines.h. See ENCODER_PAIRING_PLAN.md. */
#define SLOW_ENC_MODE_MASK				0x03
#define MAX_SHIFT_REG_NUM					4						// max 4
#define MAX_GPIO_EXPANDER_NUM				8						// GPIO expanders (MCP23017 I2C / MCP23S17 SPI), any mix, up to 16 buttons each -- MCP23017_PLAN.md
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
/* SYNC_SKIP_BEGIN */
#include "board_config.h"
/* SYNC_SKIP_END */


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
