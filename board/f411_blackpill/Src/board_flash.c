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
  * HAL_GetTick is provided by the HAL (__weak) and incremented from
  * SysTick_Handler in stm32f4xx_it.c via HAL_IncTick. The Phase 3 stub
  * here that returned 0 was removed once SysTick was wired up: HAL_PCD_Init
  * uses HAL_Delay() internally and a zero-returning tick made it spin
  * forever, hanging Board_USB_Init on cold boot.
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

int ConfigFlash_ErasePage(uint32_t page_addr)
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
	else return -1;

	FLASH_EraseInitTypeDef erase = {
		.TypeErase    = FLASH_TYPEERASE_SECTORS,
		.Banks        = FLASH_BANK_1,
		.Sector       = sector,
		.NbSectors    = 1,
		.VoltageRange = FLASH_VOLTAGE_RANGE_3,
	};
	uint32_t sector_error = 0;
	HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &sector_error);
	return (st == HAL_OK) ? 0 : -1;
}

int ConfigFlash_WriteWord(uint32_t addr, uint32_t value)
{
	HAL_StatusTypeDef st = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, (uint64_t)value);
	return (st == HAL_OK) ? 0 : -1;
}
