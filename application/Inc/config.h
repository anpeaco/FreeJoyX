/**
  ******************************************************************************
  * @file           : config.h
  * @brief          : Header for config.c file.
  ******************************************************************************
  */
/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __CONFIG_H__
#define __CONFIG_H__
	
	
#include "periphery.h"
#include "common_types.h"


/* Persist dev_config to flash. Returns 0 on success, -1 if any erase
 * or word-program reported failure. The write loop aborts on the first
 * failure, so a -1 return means the on-flash dev_config is partial and
 * the caller should treat the device's persisted state as suspect.
 * Issue anpeaco/FreeJoyX#3. */
int DevConfigSet (dev_config_t * p_dev_config);
void DevConfigGet (dev_config_t * p_dev_config);
void AppConfigInit (dev_config_t * p_dev_config);
void AppConfigGet (app_config_t * p_app_config);
uint8_t IsAppConfigEmpty (app_config_t * p_app_config);

#endif /* __CONFIG_H__ */
