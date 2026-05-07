/**
  ******************************************************************************
  * @file           : board_flash.c
  * @brief          : F103 implementation of the BSP flash-config wrappers.
  *
  * Thin pass-through to StdPeriph FLASH_*. Erase and program return
  * StdPeriph FLASH_Status (FLASH_COMPLETE on success); we map that to
  * the BSP's int 0/-1 contract. Issue anpeaco/FreeJoyX#3 plumbed status
  * through the API so callers in config.c can abort the write loop on
  * the first failure rather than barrelling on through a half-erased
  * sector.
  ******************************************************************************
  */

#include "board_flash.h"
#include "stm32f10x.h"
#include "stm32f10x_flash.h"

void ConfigFlash_Unlock(void)
{
	FLASH_Unlock();
}

void ConfigFlash_Lock(void)
{
	FLASH_Lock();
}

int ConfigFlash_ErasePage(uint32_t page_addr)
{
	FLASH_Status st = FLASH_ErasePage(page_addr);
	return (st == FLASH_COMPLETE) ? 0 : -1;
}

int ConfigFlash_WriteWord(uint32_t addr, uint32_t value)
{
	FLASH_Status st = FLASH_ProgramWord(addr, value);
	return (st == FLASH_COMPLETE) ? 0 : -1;
}
