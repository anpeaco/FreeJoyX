/**
  ******************************************************************************
  * @file           : stm32f4xx_it.c
  * @brief          : F411 BlackPill interrupt vector bodies.
  *
  * Minimal stubs for Phase 2 (blinky). Cortex-M exception handlers loop
  * forever on a fault so a debugger can locate the cause. Peripheral IRQs
  * (TIM, SPI, I2C, USB, etc.) are added as the matching peripherals get
  * wired in subsequent phases.
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
