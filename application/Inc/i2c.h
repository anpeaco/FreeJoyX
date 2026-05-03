/**
  ******************************************************************************
  * @file           : i2c.h
  * @brief          : Header file for i2c.h                 
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __I2C_H__
#define __I2C_H__

#include <stdint.h>
#ifdef BOARD_F103_BLUEPILL
#include "stm32f10x.h"
#include "stm32f10x_conf.h"
#endif
/* Wrapper prototypes only; signatures use uint8_t. The StdPeriph
 * includes stay gated to F103 because i2c.c needs them for the
 * implementation. F411's LL-based replacement lands in Phase 5c. */

#define I2C_TIMEOUT		500

void I2C_Start(void);

int I2C_WriteBlocking(uint8_t dev_addr, uint8_t reg_addr, uint8_t * data, uint16_t length);
int I2C_ReadBlocking(	uint8_t dev_addr, uint8_t reg_addr, uint8_t * data, uint16_t length, uint8_t nack);

int I2C_WriteNonBlocking(uint8_t dev_addr, uint8_t * data, uint16_t length);
int I2C_ReadNonBlocking(uint8_t dev_addr, uint8_t reg_addr, uint8_t * data, uint16_t length, uint8_t nack);

#endif 	/* __I2C_H__ */

