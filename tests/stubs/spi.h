/* Host-test stub for application/Inc/spi.h: drops the STM headers so
 * gpio_expander.c compiles on a desktop toolchain. The test provides fake
 * implementations of these (returning canned MCP23S17 SPI data). */
#ifndef __SPI_H__
#define __SPI_H__
#include <stdint.h>
void     SPI_FullDuplex_TransmitReceive(uint8_t *tx_data, uint8_t *rx_data, uint16_t length, uint8_t spi_mode);
uint16_t SPI_RxBytesRemaining(void);
void     SPI_AbortTransfer(void);
#endif /* __SPI_H__ */
