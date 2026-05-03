/**
  ******************************************************************************
  * @file           : board_i2c.c
  * @brief          : F411 BlackPill I2C driver -- compile/link stub (Phase 5c).
  *
  * The I2C-coupled sensors (as5600 / ads1115) link against I2C_Start /
  * I2C_WriteBlocking / I2C_ReadBlocking / I2C_WriteNonBlocking /
  * I2C_ReadNonBlocking declared in application/Inc/i2c.h. F103's
  * implementation lives in board/f103_bluepill/Src/board_i2c.c
  * (StdPeriph + DMA1).
  *
  * F411 will eventually use I2C1 on PB6/PB7 (AF4) -- mutexed against
  * Encoder 2 / TLE5011_GEN which also want PB6/PB7 -- driven by LL_I2C
  * with DMA1 streams. For Phase 5c we ship the function bodies as
  * compile/link stubs so the application's sensor surface compiles;
  * runtime arrives once a BlackPill is on a scope.
  *
  * Stub semantics: all functions succeed (return 0). Sensors that
  * actually depend on real I2C traffic will read back undefined data
  * on F411 today; that's the same trade-off as the SPI stub in
  * board_spi.c.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "i2c.h"

void I2C_Start(void)
{
	/* No-op stub. See file header. */
}

int I2C_WriteBlocking(uint8_t dev_addr, uint8_t reg_addr, uint8_t * data, uint16_t length)
{
	(void)dev_addr; (void)reg_addr; (void)data; (void)length;
	return 0;
}

int I2C_ReadBlocking(uint8_t dev_addr, uint8_t reg_addr, uint8_t * data, uint16_t length, uint8_t nack)
{
	(void)dev_addr; (void)reg_addr; (void)data; (void)length; (void)nack;
	return 0;
}

int I2C_WriteNonBlocking(uint8_t dev_addr, uint8_t * data, uint16_t length)
{
	(void)dev_addr; (void)data; (void)length;
	return 0;
}

int I2C_ReadNonBlocking(uint8_t dev_addr, uint8_t reg_addr, uint8_t * data, uint16_t length, uint8_t nack)
{
	(void)dev_addr; (void)reg_addr; (void)data; (void)length; (void)nack;
	return 0;
}
