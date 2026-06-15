/* Host-test stub for application/Inc/periphery.h: just the pin_config[] table
 * that gpio_expander.c uses to drive an MCP23S17 CS pin (port->ODR / pin),
 * minus the STM peripheral headers. The test defines pin_config[]. */
#ifndef __PERIPHERY_H__
#define __PERIPHERY_H__
#include <stdint.h>
#include "common_defines.h"   /* USED_PINS_NUM */

typedef struct { volatile uint32_t ODR; } GPIO_TypeDef;

typedef struct {
    GPIO_TypeDef *port;
    uint16_t      pin;
    uint16_t      caps;
} pin_config_t;

extern pin_config_t pin_config[USED_PINS_NUM];

#endif /* __PERIPHERY_H__ */
