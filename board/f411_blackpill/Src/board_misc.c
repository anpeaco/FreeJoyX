/**
  ******************************************************************************
  * @file           : board_misc.c
  * @brief          : F411 BlackPill stubs for shared application helpers.
  *
  * Both functions are no-ops on F411 today. Rationales in board_misc.h.
  ******************************************************************************
  */

#include "common_types.h"
#include "board_misc.h"

void Board_AdcQuietPeripherals(uint8_t quiet, const app_config_t *cfg)
{
	(void)quiet; (void)cfg;
}

void Board_VersionMismatchBlink(void)
{
}
