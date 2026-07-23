/* Host-test stub for application/Inc/config.h. The real config.h pulls the
 * board's periphery.h (STM peripheral + board_pins.h) purely for the flash /
 * app-config prototypes, which the descriptor builder does not use. The builder
 * (application/Src/joy_report_desc.c) only needs app_config_t, so this stub
 * routes to the stub periphery.h and common_types.h and nothing board-specific.
 * Overrides the real header because tests/stubs is first on the include path. */
#ifndef __CONFIG_H__
#define __CONFIG_H__
#include "periphery.h"
#include "common_types.h"
#endif /* __CONFIG_H__ */
