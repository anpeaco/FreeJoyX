/**
  ******************************************************************************
  * @file           : board_flash.h
  * @brief          : BSP wrappers for the device-config flash region.
  *
  * Wraps the page (or sector, on F4) erase + word program operations the
  * application uses to persist dev_config_t. Each board provides its own
  * implementation under board/<board>/Src/board_flash.c.
  *
  * The public API is intentionally page-shaped (page_addr) so the F103
  * caller in application/Src/config.c can keep its existing loop. F411
  * sector-mode erase translates the address-space into the right
  * HAL_FLASHEx_Erase call internally.
  *
  * ErasePage and WriteWord return 0 on success and -1 on any HAL or
  * StdPeriph error. Callers are expected to abort their write loop on
  * the first failure -- continuing past an erase failure programs into
  * non-erased flash and only flips 1->0, producing silent partial-write
  * corruption (issue anpeaco/FreeJoyX#3).
  ******************************************************************************
  */

#ifndef BOARD_FLASH_H_
#define BOARD_FLASH_H_

#include <stdint.h>

void ConfigFlash_Unlock(void);
void ConfigFlash_Lock(void);
int  ConfigFlash_ErasePage(uint32_t page_addr);
int  ConfigFlash_WriteWord(uint32_t addr, uint32_t value);

#endif /* BOARD_FLASH_H_ */
