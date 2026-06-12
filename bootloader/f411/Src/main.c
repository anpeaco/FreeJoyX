/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : F411 BlackPill bootloader -- DFU loop + boot decision.
  *
  * Phase 6 of the F411 port. Replaces the Phase 2 stub. Mirrors the F103
  * bootloader at bootloader/Src/main.c but with Cube USBD (CustomHID) +
  * HAL_PCD instead of F1 USB-FS-Device + StdPeriph.
  *
  * Memory layout (locked in F411_PORT_PLAN.md):
  *   S0 (16 KB)        bootloader        @ 0x08000000
  *   S1..S3 (48 KB)    reserved
  *   S4 (64 KB)        config storage    @ 0x08010000
  *   S5..S7 (384 KB)   application       @ 0x08020000
  *
  * Boot decision flow (mirrors F103):
  *   1. Read RTC->BKP0R for the DFU magic word (0x424C). Clear it
  *      atomically (single-shot semantic -- prevents boot-loop into DFU).
  *   2. Check the application's vector[0] (initial SP) lies in F411 SRAM
  *      range 0x20000000..0x2001FFFF.
  *   3. If magic present OR app invalid: stay in DFU (init clock + USB,
  *      run the DFU receive loop watching flash_finished).
  *   4. Otherwise: VTOR <- 0x08020000, MSR msp <- app SP, BX app reset.
  *
  * F411 differences from F103:
  *   - Backup register is RTC->BKP0R (F4 layout) not BKP->DR4 (F1).
  *   - No BOOT1 pin to sample (F411 only has BOOT0).
  *   - SRAM range mask: 0xFFFE0000 (128 KB SRAM at 0x20000000) vs
  *     F103's 0x2FFE0000.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_pwr.h"

#include "usbd_core.h"
#include "usbd_customhid.h"

/* Provided by board/f411_blackpill/Src/board_init.c (reused into the
 * bootloader build to share the locked 96 MHz HSE-PLL clock recipe). */
extern void Board_ClockInit_F411(void);

/* Application start address. Must match Board_RelocateVectorTable in
 * board/f411_blackpill/Src/board_dfu.c and the app linker script's
 * ORIGIN at armgcc/linker_app_f411.ld. */
#define APP_VTOR_ADDR      0x08020000U

/* DFU magic word. Matches Board_EnterDfu in board_dfu.c. */
#define BOOT_DFU_MAGIC     0x424CU

/* System (ROM) DFU magic. Matches Board_EnterSystemDfu in board_dfu.c. On
 * this value the bootloader hands off to the STM32 factory USB bootloader at
 * SYSTEM_MEMORY_BASE instead of staying in the custom HID DFU or jumping to
 * the app -- a jumper-free full reinstall (anpeaco/FreeJoyX#55). */
#define BOOT_SYSTEM_DFU_MAGIC  0x5344U

/* STM32F411 system memory (factory ROM bootloader) base address. */
#define SYSTEM_MEMORY_BASE 0x1FFF0000U

/* F411CE has 128 KB SRAM at 0x20000000-0x2001FFFF. _estack from the
 * linker is one byte past the end (0x20020000) by Cortex-M convention,
 * so the validity check must accept SRAM_BASE..SRAM_BASE+SRAM_SIZE
 * inclusive, not a bitmask of the 128 KB range. */
#define SRAM_BASE_F411     0x20000000U
#define SRAM_SIZE_F411     0x00020000U

extern USBD_DescriptorsTypeDef     FreeJoy_Desc;       /* board/f411_blackpill/Src/usbd_freejoy_desc.c */
extern USBD_CUSTOM_HID_ItfTypeDef  Boot_HID_fops;      /* bootloader/f411/Src/boot_usb_if.c */
extern volatile uint8_t            flash_finished;     /* bootloader/f411/Src/boot_usb_if.c */

USBD_HandleTypeDef hUsbDeviceFS;

static uint16_t Boot_GetMagicWord(void);
static int Boot_AppValid(void);
static void Boot_EnterApp(void) __attribute__((noreturn));
static void Boot_EnterSystemBootloader(void) __attribute__((noreturn));

int main(void)
{
	HAL_Init();
	Board_ClockInit_F411();

	uint16_t magic    = Boot_GetMagicWord();

	/* System-DFU request: hand off to the STM32 factory USB bootloader for a
	 * jumper-free reinstall. Checked before the app-jump logic so it wins
	 * over a present, valid application. */
	if (magic == BOOT_SYSTEM_DFU_MAGIC) {
		Boot_EnterSystemBootloader();
	}

	int      app_ok   = Boot_AppValid();

	if (magic != BOOT_DFU_MAGIC && app_ok) {
		Boot_EnterApp();
	}

	/* Stay in DFU. Bring up the same USB device the application uses --
	 * same VID:PID 0x0483:0x5750 so the configurator's connect flow
	 * recognises the bootloader by its single-report-ID HID descriptor
	 * (vendor-defined, REPORT_ID 4 only). */
	USBD_Init(&hUsbDeviceFS, &FreeJoy_Desc, 0);
	USBD_RegisterClass(&hUsbDeviceFS, USBD_CUSTOM_HID_CLASS);
	USBD_CUSTOM_HID_RegisterInterface(&hUsbDeviceFS, &Boot_HID_fops);
	USBD_Start(&hUsbDeviceFS);

	while (1) {
		if (flash_finished) {
			HAL_Delay(100);
			USBD_Stop(&hUsbDeviceFS);
			USBD_DeInit(&hUsbDeviceFS);
			HAL_Delay(500);
			/* Full system reset rather than a bare jump into the freshly-written
			 * app. A jump (Boot_EnterApp) leaves the OTG-FS core in the
			 * bootloader's state, and on F411 the soft-disconnect above often
			 * doesn't make the host re-enumerate -- the board sits dark until a
			 * manual power-cycle. NVIC_SystemReset cycles the OTG core and the
			 * D+ line, so the host sees a clean disconnect -> reconnect and
			 * re-enumerates on its own (what the configurator's post-flash
			 * watcher already expects). On the reset the bootloader re-runs,
			 * finds the DFU magic cleared and the app now valid, and jumps to it.
			 * (F103's USB_Shutdown drops D+ properly, so it keeps its bare jump.) */
			NVIC_SystemReset();
		}
	}
}

/* Read RTC->BKP0R for the DFU magic. Clears it atomically so the next
 * boot starts fresh (single-shot semantic, matches F103 GetMagicWord).
 * Uses LL_PWR to match the application's Board_EnterDfu pattern in
 * board/f411_blackpill/Src/board_dfu.c. */
static uint16_t Boot_GetMagicWord(void)
{
	LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_PWR);
	LL_PWR_EnableBkUpAccess();

	/* RTC clock must be enabled for backup-register reads to return real
	 * values; the application's Board_EnterDfu enables it before writing,
	 * but on a cold boot it may not be on yet. Safe to enable here --
	 * a no-op if already enabled. */
	SET_BIT(RCC->BDCR, RCC_BDCR_RTCEN);

	uint32_t value = READ_REG(RTC->BKP0R);
	if (value) {
		WRITE_REG(RTC->BKP0R, 0);
	}

	LL_PWR_DisableBkUpAccess();
	return (uint16_t)value;
}

/* Validate the application image: its first vector-table word must be
 * a stack pointer in F411 SRAM. Catches "blank flash" (all 0xFF) and
 * accidental jumps to garbage data. */
static int Boot_AppValid(void)
{
	uint32_t sp = *(volatile uint32_t *)APP_VTOR_ADDR;
	return ((sp - SRAM_BASE_F411) <= SRAM_SIZE_F411) ? 1 : 0;
}

/* Jump to the application. Set VTOR, switch the main stack pointer to
 * the app's initial SP, and BX into the app's reset handler. The
 * inline asm avoids using the stack across the SP swap. */
typedef void (*funct_ptr)(void);

static void Boot_EnterApp(void)
{
	uint32_t app_sp = *(volatile uint32_t *)APP_VTOR_ADDR;
	uint32_t app_pc = *(volatile uint32_t *)(APP_VTOR_ADDR + 4);

	WRITE_REG(SCB->VTOR, APP_VTOR_ADDR);

	__ASM volatile ("MSR msp, %0" : : "r" (app_sp) : );
	((funct_ptr)app_pc)();

	/* unreachable */
	while (1) { }
}

/* Hand off to the STM32 factory system (ROM) bootloader at SYSTEM_MEMORY_BASE
 * so the host can reinstall bootloader + app over USB DFU without a BOOT0
 * jumper (anpeaco/FreeJoyX#55). Runs before USB is brought up. Returns the
 * chip to its reset clock/peripheral state first, since the ROM bootloader
 * expects HSI / no PLL. */
static void Boot_EnterSystemBootloader(void)
{
	uint32_t sys_sp = *(volatile uint32_t *)SYSTEM_MEMORY_BASE;
	uint32_t sys_pc = *(volatile uint32_t *)(SYSTEM_MEMORY_BASE + 4);

	HAL_RCC_DeInit();
	HAL_DeInit();

	/* Stop SysTick (HAL_Init started it) so no stray exception fires once
	 * control passes to the ROM bootloader. */
	SysTick->CTRL = 0;
	SysTick->LOAD = 0;
	SysTick->VAL  = 0;

	WRITE_REG(SCB->VTOR, SYSTEM_MEMORY_BASE);

	__ASM volatile ("MSR msp, %0" : : "r" (sys_sp) : );
	((funct_ptr)sys_pc)();

	/* unreachable */
	while (1) { }
}
