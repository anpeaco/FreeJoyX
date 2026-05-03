/**
  ******************************************************************************
  * @file           : board_dfu.c
  * @brief          : F411 BlackPill -- vector-table relocate + DFU trigger.
  *
  * Mirrors board/f103_bluepill/Src/board_dfu.c. The F411 application
  * lives at 0x08020000 (sector S5 start) per the locked layout in
  * F411_PORT_PLAN.md (S0 bootloader, S1..S3 reserved, S4 config @
  * 0x08010000, S5..S7 application). VTOR therefore relocates to
  * 0x08020000.
  *
  * Phase 6 owns the F411 bootloader. Until then Board_EnterDfu writes
  * the DFU magic word into RTC->BKP0R (F4's backup-register-equivalent
  * to F1's BKP_DR4) and resets. The bootloader at sector 0 is currently
  * a stub (bootloader/f411/Src/main.c), so a real DFU jump-back path
  * arrives in Phase 6; for Phase 5c the call shape is in place so
  * main.c links and the runtime path will start working as Phase 6
  * lands.
  ******************************************************************************
  */

#include "board_dfu.h"
#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_pwr.h"

/* F411 application start address. Sector 5 begins at 0x08020000. */
#define BOARD_APP_VTOR_ADDR			0x08020000u

/* DFU magic word -- same constant as F103 so a single configurator
 * code path drives both boards' firmware-update flow. */
#define BOARD_DFU_MAGIC				0x424Cu

void Board_RelocateVectorTable(void)
{
	WRITE_REG(SCB->VTOR, BOARD_APP_VTOR_ADDR);
}

void Board_EnterDfu(void)
{
	/* Enable PWR clock so we can flip DBP, unlocking the RTC backup
	 * registers. F4's RTC->BKP0R is the equivalent of F1's BKP->DR4
	 * for stash-a-magic-word-then-reset DFU triggers. */
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
	LL_PWR_EnableBkUpAccess();

	/* Enable RTC clock so BKP0R is reachable (RTC must be enabled
	 * for the backup register block to respond to writes). */
	SET_BIT(RCC->BDCR, RCC_BDCR_RTCEN);

	WRITE_REG(RTC->BKP0R, (uint32_t)BOARD_DFU_MAGIC);

	LL_PWR_DisableBkUpAccess();
	LL_APB1_GRP1_DisableClock(LL_APB1_GRP1_PERIPH_PWR);

	NVIC_SystemReset();
}
