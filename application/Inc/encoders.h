/**
  ******************************************************************************
  * @file           : encoders.h
  * @brief          : Header for encoders.c file.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __ENCODERS_H__
#define __ENCODERS_H__

#include "common_types.h"
#include "periphery.h"
#include "buttons.h"

extern encoder_state_t encoders_state[MAX_ENCODERS_NUM];

void EncoderProcess (logical_buttons_state_t * button_state_buf, dev_config_t * p_dev_config);
void EncodersInit (dev_config_t * p_dev_config);

/* Snapshot of the live encoder-monitor accumulators (most-recently-active slow
 * encoder). Filled into params_report_t so the configurator can measure
 * quarter-steps-per-detent and flag a noisy / faulty signal. */
void EncoderGetMonitor (int16_t * net, uint16_t * valid, uint16_t * invalid, uint8_t * ab, uint8_t * slot);

#endif 	/* __BUTTONS_H__ */

