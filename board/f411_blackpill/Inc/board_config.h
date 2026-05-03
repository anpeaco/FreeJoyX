/**
  ******************************************************************************
  * @file           : board_config.h
  * @brief          : F411 BlackPill flash storage layout.
  *
  * STM32F411CEU6 has 8 sectors: S0..S3 = 16 KB each, S4 = 64 KB,
  * S5..S7 = 128 KB each (512 KB total). Locked layout
  * (F411_PORT_PLAN.md "Pin map / wire-format compatibility"):
  *   S0           bootloader            16 KB
  *   S1..S3       reserved              48 KB
  *   S4           config storage        64 KB  @ 0x08010000
  *   S5..S7       application          384 KB  @ 0x08020000
  *
  * The constants below are sized to look like 1 "page" of 64 KB so the
  * existing application/Src/config.c erase loop fires once with the
  * right address. Phase 3 will provide the matching ConfigFlash_ErasePage
  * implementation that does an F4 sector erase under the hood.
  ******************************************************************************
  */

#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

/* Phase 7: BOARD_ID tag for this board. See sibling F103 board_config.h
 * for rationale. The constant value (BOARD_ID_F411_BLACKPILL) lives in
 * application/Inc/common_defines.h, which both repos mirror. */
#define BOARD_ID						BOARD_ID_F411_BLACKPILL

#define FLASH_PAGE_SIZE					(64 * 1024)							/* sector 4 size */
#define CONFIG_PAGE_COUNT				1									/* one sector for config */
#define CONFIG_ADDR						0x08010000UL						/* sector 4 start */
#define FLASH_PAGE_END_ADDR				(CONFIG_ADDR + FLASH_PAGE_SIZE)
#define MAX_PAGE						1									/* legacy alias; F411 thinks in sectors */

#endif /* BOARD_CONFIG_H_ */
