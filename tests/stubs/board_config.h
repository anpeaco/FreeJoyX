/* Host-test stub for board/<chip>/Inc/board_config.h. mcp23017.c doesn't use
 * the flash-layout constants, but common_defines.h includes this header
 * unconditionally, so provide compile-able values (F103 BluePill numbers). */
#ifndef BOARD_CONFIG_H_
#define BOARD_CONFIG_H_
#define BOARD_ID						BOARD_ID_F103_BLUEPILL
#define MAX_PAGE						64
#define FLASH_PAGE_SIZE					1024
#define FLASH_PAGE_END_ADDR				(0x08000000 + (MAX_PAGE * FLASH_PAGE_SIZE))
#define CONFIG_PAGE_COUNT				2
#define CONFIG_ADDR						(FLASH_PAGE_END_ADDR - (CONFIG_PAGE_COUNT * FLASH_PAGE_SIZE))
#endif /* BOARD_CONFIG_H_ */
