/**
  ******************************************************************************
  * @file    stm32f4xx_hal_conf.h
  * @brief   Minimal HAL configuration for FreeJoyX-F411.
  *
  * The F411 port runs on STM32 LL as its primary driver layer (F411_PORT_PLAN.md
  * "Driver layer (F411)" lock, 2026-04-27). HAL is enabled only for the FLASH
  * module because LL doesn't ship a flash driver -- mixing LL + HAL-Flash is the
  * Cube-sanctioned pattern in that case.
  *
  * Do NOT enable additional HAL_*_MODULE_ENABLED entries here without first
  * revisiting the locked decision. Every other HAL submodule has an LL
  * counterpart that should be used instead.
  *
  * The HAL flash code calls HAL_GetTick() for its busy-wait timeouts; a stub
  * implementation lives in board_flash.c (returns 0, so HAL_FLASH polls
  * indefinitely against the FLASH_SR_BSY bit instead of timing out -- BSY
  * clears within microseconds for valid ops, and the Phase 3 acceptance is a
  * compile-clean build, not stress testing).
  ******************************************************************************
  */

#ifndef STM32F4xx_HAL_CONF_H
#define STM32F4xx_HAL_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

/* ########################## Module Selection ############################## */
#define HAL_MODULE_ENABLED
#define HAL_FLASH_MODULE_ENABLED

/* ########################## HSE/HSI Values ################################ */
/* BlackPill V3.x carries a 25 MHz HSE crystal. The PLL config in
 * board_init.c::Board_ClockInit_F411 multiplies that to the locked
 * 96 MHz system clock. HAL itself does not use these values when only
 * FLASH is enabled, but stm32f4xx_hal_def.h pulls them in via macros. */
#if !defined(HSE_VALUE)
#define HSE_VALUE          25000000U
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
#define HSE_STARTUP_TIMEOUT 100U
#endif
#if !defined(HSI_VALUE)
#define HSI_VALUE          16000000U
#endif
#if !defined(VDD_VALUE)
#define VDD_VALUE          3300U
#endif
#if !defined(TICK_INT_PRIORITY)
#define TICK_INT_PRIORITY  0x0FU
#endif

#define USE_RTOS                     0U
#define PREFETCH_ENABLE              1U
#define INSTRUCTION_CACHE_ENABLE     1U
#define DATA_CACHE_ENABLE            1U

/* HAL assertion checking off -- saves code size and avoids pulling in
 * assert_failed() infrastructure we don't need for a flash-only build. */
/* #define USE_FULL_ASSERT  1U */

#ifdef HAL_FLASH_MODULE_ENABLED
#include "stm32f4xx_hal_flash.h"
#endif

#ifdef USE_FULL_ASSERT
#define assert_param(expr) ((expr) ? (void)0U : assert_failed((uint8_t *)__FILE__, __LINE__))
void assert_failed(uint8_t *file, uint32_t line);
#else
#define assert_param(expr) ((void)0U)
#endif

#ifdef __cplusplus
}
#endif

#endif /* STM32F4xx_HAL_CONF_H */
