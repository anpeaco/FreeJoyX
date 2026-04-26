/**
  ******************************************************************************
  * @file           : board_dfu.c
  * @brief          : F103 BluePill -- vector-table relocate + DFU trigger.
  *
  * Pre-Phase-1, this code lived inline at the top of application/Src/main.c
  * (VTOR write) and as the EnterBootloader() function in the same file. The
  * BSP-seam refactor moved both behind Board_* wrappers so the F411 port
  * (Phase 6) can provide an HAL/USBD-shaped equivalent without touching
  * application code.
  *
  * Behaviour matches pre-Phase-1 byte-for-byte:
  *   - VTOR <- 0x08002000 (F103 application starts after the 8 KB bootloader)
  *   - DFU trigger writes 0x424C ('LB' little-endian) to BKP_DR4 in the
  *     backup domain (gated by RCC PWR/BKP clocks + PWR_CR DBP), then
  *     issues NVIC_SystemReset(). The bootloader's main.c reads BKP_DR4
  *     on boot and stays in DFU mode if the magic is present.
  ******************************************************************************
  */

#include "board_dfu.h"
#include "stm32f10x.h"

/* F103 application start address. Bootloader occupies the first 8 KB
 * (0x08000000 .. 0x08001FFF), application starts at 0x08002000. */
#define BOARD_APP_VTOR_ADDR			0x08002000u

/* DFU magic word. Must match the bootloader's check (see
 * bootloader/Src/main.c). */
#define BOARD_DFU_MAGIC				0x424Cu

void Board_RelocateVectorTable(void)
{
	WRITE_REG(SCB->VTOR, BOARD_APP_VTOR_ADDR);
}

void Board_EnterDfu(void)
{
	/* Enable the power and backup interface clocks by setting the
	 * PWREN and BKPEN bits in the RCC_APB1ENR register. */
	SET_BIT(RCC->APB1ENR, RCC_APB1ENR_BKPEN | RCC_APB1ENR_PWREN);

	/* Enable write access to the backup registers and the RTC. */
	SET_BIT(PWR->CR, PWR_CR_DBP);
	WRITE_REG(BKP->DR4, BOARD_DFU_MAGIC);
	CLEAR_BIT(PWR->CR, PWR_CR_DBP);

	CLEAR_BIT(RCC->APB1ENR, RCC_APB1ENR_BKPEN | RCC_APB1ENR_PWREN);

	NVIC_SystemReset();
}
