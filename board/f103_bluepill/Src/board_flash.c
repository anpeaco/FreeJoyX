/**
  ******************************************************************************
  * @file           : board_flash.c
  * @brief          : F103 implementation of the BSP flash-config wrappers.
  *
  * Thin pass-through to StdPeriph FLASH_*. Behaviour is identical to the
  * pre-Phase-1 calls in application/Src/config.c. The wrappers exist so
  * application code can stay board-agnostic; the F411 implementation in
  * Phase 3 swaps these out for HAL_FLASH_* equivalents with sector
  * semantics.
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

void ConfigFlash_ErasePage(uint32_t page_addr)
{
	FLASH_ErasePage(page_addr);
}

void ConfigFlash_WriteWord(uint32_t addr, uint32_t value)
{
	FLASH_ProgramWord(addr, value);
}
