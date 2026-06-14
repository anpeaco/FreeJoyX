/**
  ******************************************************************************
  * @file           : buttons.h
  * @brief          : Header for buttons.c file.
  ******************************************************************************
  */

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __BUTTONS_H__
#define __BUTTONS_H__

#include "common_types.h"
#include "periphery.h"
#include "shift_registers.h"
#include "axis_to_buttons.h"

extern uint8_t									raw_buttons_data[MAX_BUTTONS_NUM];
extern logical_buttons_state_t 	logical_buttons_state[MAX_BUTTONS_NUM];
extern uint8_t									phy_buttons_data[MAX_BUTTONS_NUM/8];
extern uint8_t									log_buttons_data[MAX_BUTTONS_NUM/8];
extern uint8_t									shifts_state;

typedef uint8_t button_data_t;
typedef uint8_t pov_data_t;

/* True for the four POV-hat DIRECTION types of any hat (Up/Down/Left/Right).
 * Those slots feed a POV hat control, NOT the joystick button bitmap, so the
 * button count (config.c) and the button output (buttons.c) skip them. This
 * replaces the old "is_disabled marks a POV" overload, freeing is_disabled to
 * mean only "muted button" (still counted, but reported as 0).
 *
 * POV*_CENTER is deliberately NOT included -- it is reported as an ordinary
 * button (on while the hat is centered), matching long-standing behaviour. */
static inline uint8_t Button_IsPovDirection(button_type_t type)
{
	switch (type) {
		case POV1_UP: case POV1_DOWN: case POV1_LEFT: case POV1_RIGHT:
		case POV2_UP: case POV2_DOWN: case POV2_LEFT: case POV2_RIGHT:
		case POV3_UP: case POV3_DOWN: case POV3_LEFT: case POV3_RIGHT:
		case POV4_UP: case POV4_DOWN: case POV4_LEFT: case POV4_RIGHT:
			return 1;
		default:
			return 0;
	}
}

void ButtonsDebounceProcess(dev_config_t * p_dev_config);
void LogicalButtonProcessState (logical_buttons_state_t * p_button_state, uint8_t * pov_buf, dev_config_t * p_dev_config, uint8_t pos);
void RadioButtons_Init (dev_config_t * p_dev_config);
void SequentialButtons_Init (dev_config_t * p_dev_config);
void Gestures_Init (dev_config_t * p_dev_config);
uint8_t ButtonsReadPhysical(dev_config_t * p_dev_config, uint8_t * p_buf);
void ButtonsReadLogical (dev_config_t * p_dev_config);
void ButtonsGet (uint8_t * out_data, uint8_t * log_data, uint8_t * phy_data, uint8_t * shift_data);
void POVsGet (pov_data_t * data);


#endif 	/* __BUTTONS_H__ */

