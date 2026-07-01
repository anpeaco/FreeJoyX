/* Host-test stub for application/Inc/i2c.h: drops the STM32 peripheral headers
 * so mcp23017.c can be compiled on a desktop toolchain. The test provides the
 * implementations (faking the bus). */
#ifndef __I2C_H__
#define __I2C_H__
#include <stdint.h>
int I2C_WriteBlocking(uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t length);
int I2C_ReadBlocking (uint8_t dev_addr, uint8_t reg_addr, uint8_t *data, uint16_t length, uint8_t nack);
#endif /* __I2C_H__ */
