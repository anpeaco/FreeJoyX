/**
  ******************************************************************************
  * @file           : board_flash.c
  * @brief          : F411 implementation of the BSP flash-config wrappers.
  *
  * Mirrors the F103 BSP API (board/common/Inc/board_flash.h) on top of the
  * STM32 HAL FLASH driver. F411 stores the device config in sector 4
  * (0x08010000, 64 KB) -- see board_config.h. Sector erase + word program
  * are the only HAL ops needed; everything else is LL.
  *
  * HAL_GetTick stub lives at the bottom of this file. The HAL flash polls
  * busy via HAL_GetTick-driven timeouts; with the stub returning 0, the
  * elapsed-tick check never trips, and the loop polls FLASH_SR_BSY until
  * it clears (microseconds for valid ops). Acceptable for Phase 3 scope
  * (acceptance criterion is a clean build); a real tick source lands in
  * Phase 5 alongside TIM2 main tick wiring.
  ******************************************************************************
  */

#include "board_flash.h"
#include "board_config.h"
#include "stm32f4xx_hal.h"

void ConfigFlash_Unlock(void)
{
	HAL_FLASH_Unlock();
}

void ConfigFlash_Lock(void)
{
	HAL_FLASH_Lock();
}

void ConfigFlash_ErasePage(uint32_t page_addr)
{
	/* The shared application/Src/config.c uses page-based erase loops;
	 * F411 collapses each "page" to a single 64 KB sector erase since
	 * the config region is sector 4 only. The sector index is derived
	 * from the address rather than hardcoded so this stays correct if
	 * board_config.h is ever retargeted to a different sector. */
	uint32_t sector;
	if      (page_addr >= 0x08000000U && page_addr < 0x08004000U) sector = FLASH_SECTOR_0;
	else if (page_addr >= 0x08004000U && page_addr < 0x08008000U) sector = FLASH_SECTOR_1;
	else if (page_addr >= 0x08008000U && page_addr < 0x0800C000U) sector = FLASH_SECTOR_2;
	else if (page_addr >= 0x0800C000U && page_addr < 0x08010000U) sector = FLASH_SECTOR_3;
	else if (page_addr >= 0x08010000U && page_addr < 0x08020000U) sector = FLASH_SECTOR_4;
	else if (page_addr >= 0x08020000U && page_addr < 0x08040000U) sector = FLASH_SECTOR_5;
	else if (page_addr >= 0x08040000U && page_addr < 0x08060000U) sector = FLASH_SECTOR_6;
	else if (page_addr >= 0x08060000U && page_addr < 0x08080000U) sector = FLASH_SECTOR_7;
	else return;

	FLASH_EraseInitTypeDef erase = {
		.TypeErase    = FLASH_TYPEERASE_SECTORS,
		.Banks        = FLASH_BANK_1,
		.Sector       = sector,
		.NbSectors    = 1,
		.VoltageRange = FLASH_VOLTAGE_RANGE_3,
	};
	uint32_t sector_error = 0;
	HAL_FLASHEx_Erase(&erase, &sector_error);
}

void ConfigFlash_WriteWord(uint32_t addr, uint32_t value)
{
	HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, (uint64_t)value);
}

/* HAL_GetTick stub. The HAL FLASH driver only calls it from busy-wait
 * timeouts; returning 0 makes the elapsed-tick check never trip, so the
 * loop polls FLASH_SR_BSY indefinitely (which clears in microseconds for
 * valid ops). Phase 5 replaces this with the real tick once TIM2 is up. */
uint32_t HAL_GetTick(void)
{
	return 0U;
}
