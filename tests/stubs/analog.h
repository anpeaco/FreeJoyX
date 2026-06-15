/* Host-test stub for application/Inc/analog.h: exposes just the sensors[] array
 * that mcp23017.c's I2C bus-idle gate reads (drops periphery.h / STM headers).
 * The test defines sensors[]. */
#ifndef __ANALOG_H__
#define __ANALOG_H__
#include "common_types.h"
extern sensor_t sensors[MAX_AXIS_NUM];
#endif /* __ANALOG_H__ */
