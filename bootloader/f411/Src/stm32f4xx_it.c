/**
  ******************************************************************************
  * @file           : stm32f4xx_it.c
  * @brief          : F411 bootloader interrupt vector bodies.
  *
  * Mirrors board/f411_blackpill/Src/stm32f4xx_it.c (application IT
  * file). Bootloader has its own copy because the file participates in
  * the vector table and gets one strong definition per build.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"

extern PCD_HandleTypeDef hpcd;   /* defined in board/f411_blackpill/Src/usbd_conf.c */

void NMI_Handler(void)        { while (1) { } }
void HardFault_Handler(void)  { while (1) { } }
void MemManage_Handler(void)  { while (1) { } }
void BusFault_Handler(void)   { while (1) { } }
void UsageFault_Handler(void) { while (1) { } }
void SVC_Handler(void)        { }
void DebugMon_Handler(void)   { }
void PendSV_Handler(void)     { }

void SysTick_Handler(void)
{
	HAL_IncTick();
}

void OTG_FS_IRQHandler(void)
{
	HAL_PCD_IRQHandler(&hpcd);
}
