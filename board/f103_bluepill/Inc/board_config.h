/**
  ******************************************************************************
  * @file           : board_config.h
  * @brief          : F103 BluePill flash storage layout.
  *
  * STM32F103C8 has 64 pages of 1 KB each (64 KB total). The application's
  * dev_config_t persists in the last 2 pages (so 0x0800F800 .. 0x0800FFFF on
  * an F103C8 / 64 KB part). The F411 port (Phase 3) provides its own
  * board_config.h with sector-based addressing reflecting the F4 flash
  * layout (S0..S3 16 KB, S4 64 KB, S5..S7 128 KB).
  *
  * The address constants formerly lived in application/Inc/common_defines.h
  * but they're inherently chip-specific -- moved here as part of the
  * Phase 1 BSP-seam refactor.
  ******************************************************************************
  */

#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_

/* Phase 7: BOARD_ID tag for this board. Goes into dev_config_t.board_id
 * and params_report_t.board_id so the configurator can dispatch the
 * right pin table and reject cross-board writes. The value itself
 * (BOARD_ID_F103_BLUEPILL) is defined in application/Inc/common_defines.h
 * which both repos mirror. */
#define BOARD_ID						BOARD_ID_F103_BLUEPILL

#define MAX_PAGE						64				/* number of 1-KB pages on STM32F103C8 */
#define FLASH_PAGE_SIZE					1024
#define FLASH_PAGE_END_ADDR				(0x08000000 + (MAX_PAGE * FLASH_PAGE_SIZE))
#define CONFIG_PAGE_COUNT				2				/* resize stored-config window here */
#define CONFIG_ADDR						(FLASH_PAGE_END_ADDR - (CONFIG_PAGE_COUNT * FLASH_PAGE_SIZE))

#endif /* BOARD_CONFIG_H_ */
