/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : F411 bootloader stub (Phase 2 placeholder).
  *
  * Phase 6 of the F411 port will replace this with a real LL + ST USB
  * Device Library DFU bootloader speaking the existing REPORT_ID_FIRMWARE
  * HID protocol. For now it's a stub that just sits in a loop so the
  * bootloader build link succeeds and we have a non-zero binary in
  * sector 0.
  ******************************************************************************
  */

#include "stm32f4xx.h"

void NMI_Handler(void)        { while (1) { } }
void HardFault_Handler(void)  { while (1) { } }
void MemManage_Handler(void)  { while (1) { } }
void BusFault_Handler(void)   { while (1) { } }
void UsageFault_Handler(void) { while (1) { } }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }
void SysTick_Handler(void)    { }

int main(void)
{
	for (;;) { __asm volatile ("nop"); }
}
