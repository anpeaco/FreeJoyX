/**
  ******************************************************************************
  * @file           : spi.h
  * @brief          : Header file for spi.h                 
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __SPI_H__
#define __SPI_H__

#include <stdint.h>
#ifdef BOARD_F103_BLUEPILL
#include "stm32f10x.h"
#include "stm32f10x_conf.h"
#endif
/* Wrapper prototypes only; signatures use uint8_t/uint16_t from stdint.
 * The StdPeriph includes stay gated to F103 because spi.c needs them
 * for the implementation. F411's LL-based replacement lands in Phase 5c. */

void SPI_Start(void);
void SPI_HalfDuplex_Transmit(uint8_t * data, uint16_t length, uint8_t spi_mode);
void SPI_HalfDuplex_Receive(uint8_t * data, uint16_t length, uint8_t spi_mode);
void SPI_FullDuplex_TransmitReceive(uint8_t * tx_data, uint8_t * rx_data, uint16_t length, uint8_t spi_mode);
#endif 	/* __SPI_H__ */

