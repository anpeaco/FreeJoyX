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

/* Sensor-driver hooks added in Phase 5c so as5048a / mcp320x / mlx90363 /
 * mlx90393 / tle5011 / tle5012 can poll for transfer completion and stop
 * the in-flight DMA without touching DMA1_Channel{2,3} directly. F103
 * impl wraps StdPeriph DMA_GetCurrDataCounter / DMA_Cmd; F411 stub
 * returns 0 / no-op so polling loops exit immediately on the no-op SPI
 * stub (real DMA2-based impl arrives once a BlackPill is on a scope). */
uint16_t SPI_RxBytesRemaining(void);
uint16_t SPI_TxBytesRemaining(void);
void     SPI_AbortTransfer(void);
#endif 	/* __SPI_H__ */

