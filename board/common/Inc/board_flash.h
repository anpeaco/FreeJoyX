/**
  ******************************************************************************
  * @file           : board_flash.h
  * @brief          : BSP wrappers for the device-config flash region.
  *
  * Wraps the page (or sector, on F4) erase + word program operations the
  * application uses to persist dev_config_t. Each board provides its own
  * implementation under board/<board>/Src/board_flash.c -- F103 currently
  * delegates straight to StdPeriph FLASH_* (unchanged behaviour); the F411
  * port (Phase 3) will implement the same API on top of HAL_FLASH_* with
  * F4 sector semantics.
  *
  * The public API is intentionally page-shaped (page_addr) so the F103
  * caller in application/Src/config.c can keep its existing loop. F4's
  * sector-mode erase will translate the address-space into the right
  * HAL_FLASHEx_Erase call internally.
  ******************************************************************************
  */

#ifndef BOARD_FLASH_H_
#define BOARD_FLASH_H_

#include <stdint.h>

void ConfigFlash_Unlock(void);
void ConfigFlash_Lock(void);
void ConfigFlash_ErasePage(uint32_t page_addr);
void ConfigFlash_WriteWord(uint32_t addr, uint32_t value);

#endif /* BOARD_FLASH_H_ */
