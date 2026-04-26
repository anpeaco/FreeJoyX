/**
  ******************************************************************************
  * @file           : board_ids.h
  * @brief          : Compile-time board identification.
  *
  * Each supported board sets exactly one BOARD_<NAME> macro at compile time
  * (via target_<chip>.mk's TARGET_C_DEFS). This header asserts that exactly
  * one is set so a misconfigured build fails fast rather than silently
  * compiling against the wrong BSP.
  ******************************************************************************
  */

#ifndef BOARD_IDS_H_
#define BOARD_IDS_H_

#include <stdint.h>

/* Stable numeric IDs for the dev_config_t.board_id field that gets reported
 * over USB so the configurator knows which pin map / capability set to apply.
 * Assigned values are part of the wire format -- never reuse, never renumber. */
#define BOARD_ID_F103_BLUEPILL    1u
#define BOARD_ID_F411_BLACKPILL   2u

/* Resolve the active board's numeric ID from the BOARD_<NAME> compile flag. */
#if defined(BOARD_F103_BLUEPILL)
	#define BOARD_ID_CURRENT  BOARD_ID_F103_BLUEPILL
	#define BOARD_NAME_STR    "F103 BluePill"
#elif defined(BOARD_F411_BLACKPILL)
	#define BOARD_ID_CURRENT  BOARD_ID_F411_BLACKPILL
	#define BOARD_NAME_STR    "F411 BlackPill"
#else
	#error "No BOARD_<name> defined. Set one in target_<chip>.mk's TARGET_C_DEFS."
#endif

#endif /* BOARD_IDS_H_ */
