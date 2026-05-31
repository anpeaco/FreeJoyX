/**
  ******************************************************************************
  * @file           : board_dfu.h
  * @brief          : BSP wrappers for vector-table relocation + DFU entry.
  *
  * Two pieces of board-specific machinery the application needs near the
  * top of main():
  *
  * 1. Board_RelocateVectorTable() -- points SCB->VTOR at the application's
  *    load address so interrupt vectors are fetched from the right offset
  *    (post-bootloader). The address differs per chip and per memory map:
  *    F103 BluePill = 0x08002000 (8 KB bootloader); F411 BlackPill =
  *    0x08020000 (S0..S3 reserved for the larger HAL/USBD bootloader,
  *    config in S4, application starts at S5).
  *
  * 2. Board_EnterDfu() -- writes a magic value into a non-volatile
  *    register and resets the MCU. The bootloader reads that register on
  *    boot and stays in DFU mode rather than jumping to the application.
  *    F103 uses BKP_DR4 in the backup domain; F411 will use RTC->BKP0R
  *    on its own backup domain.
  ******************************************************************************
  */

#ifndef BOARD_DFU_H_
#define BOARD_DFU_H_

void Board_RelocateVectorTable(void);
void Board_EnterDfu(void);

/* Hand off to the chip's factory system (ROM) USB DFU bootloader for a
 * jumper-free full reinstall (anpeaco/FreeJoyX#55). The F411 writes a magic
 * distinct from Board_EnterDfu's and resets; the FreeJoy bootloader then
 * jumps to 0x1FFF0000 instead of the app. The F103 ROM bootloader is
 * USART-only (no USB DFU), so its implementation is a no-op. */
void Board_EnterSystemDfu(void);

#endif /* BOARD_DFU_H_ */
