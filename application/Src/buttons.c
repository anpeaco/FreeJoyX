/**
  ******************************************************************************
  * @file           : buttons.c
  * @brief          : Buttons driver implementation
		
		FreeJoy software for game device controllers
    Copyright (C) 2020  Yury Vostrenkov (yuvostrenkov@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
		
  ******************************************************************************
  */

#include "buttons.h"
#include "string.h"

uint8_t												raw_buttons_data[MAX_BUTTONS_NUM];
physical_buttons_state_t 			physical_buttons_state[MAX_BUTTONS_NUM];
logical_buttons_state_t 			logical_buttons_state[MAX_BUTTONS_NUM];
uint8_t												phy_buttons_data[MAX_BUTTONS_NUM/8];
uint8_t												log_buttons_data[MAX_BUTTONS_NUM/8];
button_data_t 								out_buttons_data[MAX_BUTTONS_NUM/8];
pov_data_t 										pov_data[MAX_POVS_NUM];
uint8_t												pov_pos[MAX_POVS_NUM];
uint8_t												shifts_state = 0;
uint8_t												a2b_first = 0;
uint8_t												a2b_last = 0;
volatile uint8_t							button_mutex = 0;

// Gesture-coordination tables, indexed by physical input number (0..MAX_BUTTONS_NUM-1).
// gesture_delay_ms[p] = resolved gesture window for physical p, 0 if no TAP
//   or DOUBLE_TAP slot is bound to it. Built once at boot by Gestures_Init().
// gesture_claimed[p] = 1 once a TAP or DOUBLE_TAP has fired in the current
//   press cycle of physical p; tells sister BUTTON_NORMAL slots to suppress their
//   delayed press fire.
// gesture_low_since[p] = millis timestamp of when physical p went low; used to
//   delay clearing gesture_claimed until the physical has been low long enough
//   for any pending NORMAL DELAY-to-PRESS transition to have already resolved.
static uint16_t								gesture_delay_ms[MAX_BUTTONS_NUM];
static uint8_t								gesture_claimed[MAX_BUTTONS_NUM];
static int32_t								gesture_low_since[MAX_BUTTONS_NUM];
/* Per-physical record of whether a DOUBLE_TAP sister slot exists, and
 * the configured window. TAP slots use this to decide whether to fire
 * immediately on release-within-cutoff (no DT sister) or defer the
 * pulse by this many ms so a possible second tap can take over
 * (mutually-exclusive TAP vs DT semantics -- see F103_GESTURE_PLAN.md).
 * 0 = no DT sister; non-zero = DT sister exists with this window. */
static uint16_t								dt_window_ms[MAX_BUTTONS_NUM];

static void GestureClaimedSweep (int32_t millis);

/**
  * @brief  Processing debounce for raw buttons input
	* @param  p_dev_config: Pointer to device configuration
  * @retval None
  */
void ButtonsDebounceProcess (dev_config_t * p_dev_config)
{
	int32_t 	millis;
	uint16_t	debounce;
	
	millis = GetMillis();
	
	for (uint8_t i=0; i<MAX_BUTTONS_NUM; i++)
	{
			// set a2b debounce
			if (a2b_first != a2b_last && i > a2b_first && i <= a2b_last)
			{
				debounce = p_dev_config->a2b_debounce_ms;
			}
			else 
			{
				debounce = p_dev_config->button_debounce_ms;
			}
		
			physical_buttons_state[i].prev_pin_state = physical_buttons_state[i].pin_state;
			physical_buttons_state[i].pin_state = raw_buttons_data[i];
		
			// set timestamp if state changed
			if (!physical_buttons_state[i].changed && physical_buttons_state[i].pin_state != physical_buttons_state[i].prev_pin_state)		
			{
				physical_buttons_state[i].time_last = millis;
				physical_buttons_state[i].changed = 1;
			}
			// set state after debounce if state have not changed
			else if (	physical_buttons_state[i].changed && physical_buttons_state[i].pin_state == physical_buttons_state[i].prev_pin_state &&
								millis - physical_buttons_state[i].time_last > debounce)
			{

				physical_buttons_state[i].changed = 0;
				physical_buttons_state[i].current_state = physical_buttons_state[i].pin_state;
				//physical_buttons_state[i].cnt++;
			}
			// reset if state changed during debounce period
			else if (physical_buttons_state[i].changed &&
								millis - physical_buttons_state[i].time_last > debounce)
			{
				physical_buttons_state[i].changed = 0;
			}
	}
	// Decay gesture_claimed[] flags once their physical inputs have been
	// low long enough; sister BUTTON_NORMAL slots use these flags to
	// suppress delayed press fires within the gesture window.
	GestureClaimedSweep(millis);
}

static void LogicalButtonProcessTimer (logical_buttons_state_t * p_button_state, int32_t millis, dev_config_t * p_dev_config, uint8_t num)
{
	uint16_t tmp_press_time;
	uint16_t tmp_delay_time;

	// TAP and DOUBLE_TAP own their own state machines and track timing
	// internally. The standard delay_act ticker below would auto-transition
	// DELAY -> PRESS at tmp_delay_time (= 0 for slots with no per-slot
	// delay timer), clobbering the gesture-specific dwell. Bail before
	// touching delay_act for these types.
	if (p_dev_config->buttons[num].type == TAP ||
	    p_dev_config->buttons[num].type == DOUBLE_TAP)
	{
		return;
	}

	// get toggle press timer
	switch (p_dev_config->buttons[num].press_timer)
	{	
			case BUTTON_TIMER_1:
				tmp_press_time = p_dev_config->button_timer1_ms;
				break;
			case BUTTON_TIMER_2:
				tmp_press_time = p_dev_config->button_timer2_ms;
				break;
			case BUTTON_TIMER_3:
				tmp_press_time = p_dev_config->button_timer3_ms;
				break;
			default:
					tmp_press_time = 100;
				break;
	}
	
	// get delay 	
	if(tmp_press_time <= 0)
	{
		tmp_press_time = 100;
	}
	switch (p_dev_config->buttons[num].delay_timer)
	{	
		case BUTTON_TIMER_1:
				tmp_delay_time = p_dev_config->button_timer1_ms;
				break;
		case BUTTON_TIMER_2:
				tmp_delay_time = p_dev_config->button_timer2_ms;
				break;
		case BUTTON_TIMER_3:
				tmp_delay_time = p_dev_config->button_timer3_ms;
				break;
		default:
				tmp_delay_time = 0;
				break;
	}
	
	// Center POVs has forced delay
	if ( tmp_delay_time == 0 && (p_dev_config->buttons[num].type == POV1_CENTER ||
			p_dev_config->buttons[num].type == POV2_CENTER ||
			p_dev_config->buttons[num].type == POV3_CENTER ||
			p_dev_config->buttons[num].type == POV4_CENTER))
	{
		tmp_delay_time = 100;
	}

	// BUTTON_NORMAL gesture coordination: extend the delay window to the
	// per-physical resolved gesture window when sister TAP / DOUBLE_TAP
	// slots exist. NORMAL must hold off firing until the gesture decision is
	// in. gesture_delay_ms[p] is 0 when no gesture sister exists, leaving
	// existing behaviour untouched for plain NORMAL slots.
	if (p_dev_config->buttons[num].type == BUTTON_NORMAL)
	{
		int8_t p = p_dev_config->buttons[num].physical_num;
		if (p >= 0 && p < MAX_BUTTONS_NUM)
		{
			uint16_t g = gesture_delay_ms[p];
			if (g > tmp_delay_time) tmp_delay_time = g;
		}
	}
		
	// set max delay timer for sequential and radio buttons // heroviy kostil`, need if for check all seq buttons for types of timings
//	if (p_dev_config->buttons[num].delay_timer && 
//		 (p_dev_config->buttons[num].type == SEQUENTIAL_TOGGLE || p_dev_config->buttons[num].type == SEQUENTIAL_BUTTON))
//	{
//		if(p_dev_config->button_timer1_ms > p_dev_config->button_timer2_ms && p_dev_config->button_timer1_ms > p_dev_config->button_timer3_ms)
//				tmp_delay_time = p_dev_config->button_timer1_ms;
//		else if(p_dev_config->button_timer2_ms > p_dev_config->button_timer1_ms && p_dev_config->button_timer2_ms > p_dev_config->button_timer3_ms)
//				tmp_delay_time = p_dev_config->button_timer2_ms;
//		else
//				tmp_delay_time = p_dev_config->button_timer3_ms;
//	}
	
	// check if delay timer elapsed
	if ((p_button_state->delay_act == BUTTON_ACTION_DELAY && millis - p_button_state->time_last > tmp_delay_time &&
						millis - p_button_state->time_last < tmp_press_time + tmp_delay_time))
	{	
		p_button_state->delay_act = BUTTON_ACTION_PRESS;		
	}
	// check if press timer elapsed
	else if ((p_button_state->delay_act == BUTTON_ACTION_PRESS && 
			 millis - p_button_state->time_last > tmp_press_time + tmp_delay_time))
	{
		p_button_state->delay_act = BUTTON_ACTION_IDLE;
	}
	else if (p_button_state->delay_act == BUTTON_ACTION_BLOCK && 			// blocking button needed for Alps hats
			 millis - p_button_state->time_last > 100)
	{
		p_button_state->delay_act = BUTTON_ACTION_IDLE;
	}
}

/**
  * @brief  Getting logical button state accoring to its configuration
  * @param  p_button_state:	Pointer to button state structure
	* @param  pov_buf: Pointer to POV states buffer
	* @param  p_dev_config: Pointer to device configuration
	* @param  num: Button number
  * @retval None
  */
void LogicalButtonProcessState (logical_buttons_state_t * p_button_state, uint8_t * pov_buf, dev_config_t * p_dev_config, uint8_t num)
{	
	
	int32_t millis;
	uint8_t pov_group = 0;
	
	millis = GetMillis();
	LogicalButtonProcessTimer(p_button_state, millis, p_dev_config, num);
	
		switch (p_dev_config->buttons[num].type)
		{				
			case BUTTON_NORMAL:
			case POV1_CENTER:
			case POV2_CENTER:
			case POV3_CENTER:
			case POV4_CENTER:
				if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					// Rising edge while already in DELAY = quick re-press
					// (e.g. second tap of a double-tap on a TAP+NORMAL physical).
					// Re-arm the gesture window from the new rising edge so the
					// new press cycle evaluates independently, and clear
					// gesture_claimed so a TAP that fires this cycle gets
					// observed cleanly. Without this, NORMAL's time_last stayed
					// pinned to the original press, and the standard timer's
					// DELAY->PRESS transition could fire before/around the
					// moment GestureClaimedSweep cleared the previous cycle's
					// claim, causing NORMAL to flash for one tick.
					if (p_dev_config->buttons[num].type == BUTTON_NORMAL &&
					    p_button_state->curr_physical_state > p_button_state->prev_physical_state)
					{
						p_button_state->time_last = millis;
						p_button_state->on_state = p_button_state->curr_physical_state;
						int8_t p = p_dev_config->buttons[num].physical_num;
						if (p >= 0 && p < MAX_BUTTONS_NUM) gesture_claimed[p] = 0;
					}
					// else: nop (still waiting out the gesture window)
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					// Gesture coordination: if a sister TAP or DOUBLE_TAP
					// fired during the delay window, suppress this NORMAL fire and
					// return to IDLE without setting current_state. Only NORMAL
					// participates -- POV*_CENTER share this case body but live on
					// their own physical inputs and aren't affected.
					int8_t p = p_dev_config->buttons[num].physical_num;
					if (p_dev_config->buttons[num].type == BUTTON_NORMAL &&
					    p >= 0 && p < MAX_BUTTONS_NUM &&
					    gesture_claimed[p])
					{
						p_button_state->delay_act = BUTTON_ACTION_IDLE;
						p_button_state->current_state = 0;
					}
					else
					{
						p_button_state->current_state = p_button_state->on_state;
					}
				}
				else if (p_button_state->curr_physical_state > p_button_state->prev_physical_state &&
								p_button_state->delay_act != BUTTON_ACTION_BLOCK)		// triggered in IDLE
				{
					p_button_state->delay_act = BUTTON_ACTION_DELAY;
					p_button_state->time_last = millis;
					p_button_state->on_state = p_button_state->curr_physical_state;
					//p_button_state->off_state = !p_button_state->on_state;
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_BLOCK)
				{
					p_button_state->current_state = 0;
				}
				else	// IDLE state
				{
					p_button_state->current_state = p_button_state->curr_physical_state;
				}
				break;
				
			case BUTTON_TOGGLE:
				if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					// nop
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					p_button_state->current_state = p_button_state->on_state;
				}
				else if (p_button_state->curr_physical_state > p_button_state->prev_physical_state)		// triggered in IDLE
				{
					p_button_state->delay_act = BUTTON_ACTION_DELAY;
					p_button_state->time_last = millis;
					p_button_state->on_state = !p_button_state->current_state;
					p_button_state->off_state = p_button_state->on_state;
				}
				else	// IDLE state
				{
					// nop
				}
				break;
				
			case TOGGLE_SWITCH:
				if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					// nop
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					p_button_state->current_state = p_button_state->on_state;
				}
				else if (p_button_state->curr_physical_state != p_button_state->prev_physical_state)		// triggered in IDLE
				{
					p_button_state->delay_act = BUTTON_ACTION_DELAY;
					p_button_state->time_last = millis;
					p_button_state->on_state = 1;				// check when inverted!
					p_button_state->off_state = 0;
				}
				else	// IDLE state
				{
					p_button_state->current_state = p_button_state->off_state;
				}
				break;
				
			case TOGGLE_SWITCH_ON:
				if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					// nop
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					p_button_state->current_state = p_button_state->on_state;
				}
				else if (p_button_state->curr_physical_state > p_button_state->prev_physical_state)		// triggered in IDLE
				{
					p_button_state->delay_act = BUTTON_ACTION_DELAY;
					p_button_state->time_last = millis;
					p_button_state->on_state = 1;				// check when inverted!
					p_button_state->off_state = 0;
				}
				else	// IDLE state
				{
					p_button_state->current_state = p_button_state->off_state;
				}
				break;
			 
			case TOGGLE_SWITCH_OFF:
				if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					// nop
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					p_button_state->current_state = p_button_state->on_state;
				}
				else if (p_button_state->curr_physical_state < p_button_state->prev_physical_state)		// triggered in IDLE
				{
					p_button_state->delay_act = BUTTON_ACTION_DELAY;
					p_button_state->time_last = millis;
					p_button_state->on_state = 1;				// check when inverted!
					p_button_state->off_state = 0;
				}
				else	// IDLE state
				{
					p_button_state->current_state = p_button_state->off_state;
				}
				break;
				
			case POV4_UP:
			case POV4_RIGHT:
			case POV4_DOWN:
			case POV4_LEFT:
				pov_group = 3;
			
			case POV3_UP:
			case POV3_RIGHT:
			case POV3_DOWN:
			case POV3_LEFT:	
				if (!pov_group) pov_group = 2;
			
			case POV2_UP:
			case POV2_RIGHT:
			case POV2_DOWN:
			case POV2_LEFT:		
				if (!pov_group) pov_group = 1;
			
			case POV1_UP:
			case POV1_RIGHT:
			case POV1_DOWN:
			case POV1_LEFT:
				if (pov_group<=0) pov_group = 0;
				
			// block center button on direction state change
				if (p_button_state->curr_physical_state != p_button_state->prev_physical_state)
				{
					button_type_t center_type;
					switch (pov_group) {
						case 0: center_type = POV1_CENTER; break;
						case 1: center_type = POV2_CENTER; break;
						case 2: center_type = POV3_CENTER; break;
						case 3: center_type = POV4_CENTER; break;
						default: center_type = 0; break;
					}
					if (center_type != 0) {
						for (uint8_t i = 0; i < MAX_BUTTONS_NUM; i++)
						{
							if (p_dev_config->buttons[i].type == center_type)
							{
								logical_buttons_state[i].delay_act = BUTTON_ACTION_BLOCK;
								logical_buttons_state[i].current_state = 0;
								logical_buttons_state[i].time_last = millis;
							}
						}
					}
				}
			
				if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					// nop
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					p_button_state->current_state = p_button_state->on_state;
				}
				else if (p_button_state->curr_physical_state > p_button_state->prev_physical_state)		// triggered in IDLE
				{
					p_button_state->delay_act = BUTTON_ACTION_DELAY;
					p_button_state->time_last = millis;
					p_button_state->on_state = p_button_state->curr_physical_state;
					p_button_state->off_state = !p_button_state->on_state;
				}
				else	// IDLE state
				{
					p_button_state->current_state = p_button_state->curr_physical_state;
				}
					
				// set bit in povs data
				if (p_dev_config->buttons[num].type == POV1_UP || p_dev_config->buttons[num].type == POV2_UP ||
						p_dev_config->buttons[num].type == POV3_UP || p_dev_config->buttons[num].type == POV4_UP)
				{
					pov_buf[pov_group] &= ~(1 << 3);
					pov_buf[pov_group] |= (p_button_state->current_state << 3);
				}
				else if (p_dev_config->buttons[num].type == POV1_RIGHT || p_dev_config->buttons[num].type == POV2_RIGHT ||
								 p_dev_config->buttons[num].type == POV3_RIGHT || p_dev_config->buttons[num].type == POV4_RIGHT)
				{
					pov_buf[pov_group] &= ~(1 << 2);
					pov_buf[pov_group] |= (p_button_state->current_state << 2);
				}
				else if (p_dev_config->buttons[num].type == POV1_DOWN || p_dev_config->buttons[num].type == POV2_DOWN ||
								 p_dev_config->buttons[num].type == POV3_DOWN || p_dev_config->buttons[num].type == POV4_DOWN)
				{
					pov_buf[pov_group] &= ~(1 << 1);
					pov_buf[pov_group] |= (p_button_state->current_state << 1);
				}
				else
				{
					pov_buf[pov_group] &= ~(1 << 0);
					pov_buf[pov_group] |= (p_button_state->current_state << 0);
				}
				
				// turn off POV center button if one of directions is pressed
				if (pov_buf[pov_group] != 0)
				{
					button_type_t center_type;
					switch (pov_group) {
						case 0: center_type = POV1_CENTER; break;
						case 1: center_type = POV2_CENTER; break;
						case 2: center_type = POV3_CENTER; break;
						case 3: center_type = POV4_CENTER; break;
						default: center_type = 0; break;
					}
					if (center_type != 0) {
						for (uint8_t i = 0; i < MAX_BUTTONS_NUM; i++)
						{
							if (p_dev_config->buttons[i].type == center_type)
							{
								logical_buttons_state[i].delay_act = BUTTON_ACTION_BLOCK;
								logical_buttons_state[i].current_state = 0;
								logical_buttons_state[i].time_last = millis;
							}
						}
					}
				}
				
				break;
							
			case RADIO_BUTTON1:
			case RADIO_BUTTON2:
			case RADIO_BUTTON3:
			case RADIO_BUTTON4:
				if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					// nop
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					p_button_state->current_state = p_button_state->on_state;
					
					for (uint8_t i=0; i<MAX_BUTTONS_NUM; i++)
					{
						if (p_dev_config->buttons[i].type == p_dev_config->buttons[num].type && i != num)
						{
							logical_buttons_state[i].current_state = logical_buttons_state[i].off_state;
						}
					}
				}
				else if (p_button_state->curr_physical_state)// > p_button_state->prev_physical_state)		// triggered in IDLE
				{
					p_button_state->delay_act = BUTTON_ACTION_DELAY;
					p_button_state->time_last = millis;
					p_button_state->on_state = 1;
					p_button_state->off_state = 0;
				}
				else	// IDLE state
				{
					// nop
				}			
				break;
				
			case SEQUENTIAL_TOGGLE:
					if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					// nop
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					p_button_state->current_state = p_button_state->on_state;
				}
				else if (p_button_state->curr_physical_state > p_button_state->prev_physical_state)		// triggered in IDLE
				{
					// searching for enabled button
					uint8_t is_last = 1;
					uint8_t is_set_found = 0;
					for (uint8_t i=0; i<MAX_BUTTONS_NUM; i++)
					{
						if (p_dev_config->buttons[i].physical_num == p_dev_config->buttons[num].physical_num &&
							p_dev_config->buttons[i].type == SEQUENTIAL_TOGGLE)
						{
							//disable enabled button
							if (logical_buttons_state[i].on_state == 1 && 
									logical_buttons_state[i].delay_act == BUTTON_ACTION_IDLE)	// prevent multiple enabling
							{
								logical_buttons_state[i].on_state = 0;
								logical_buttons_state[i].off_state = 0;
								logical_buttons_state[i].current_state = 0;
								is_set_found = 1;
							}
							else if (is_set_found)	// enable next button in list
							{
								logical_buttons_state[i].delay_act = BUTTON_ACTION_DELAY;
								logical_buttons_state[i].time_last = millis;
								
								logical_buttons_state[i].on_state = 1;
								logical_buttons_state[i].off_state = 0;
								is_last = 0;
								break;
							}
						}
					}
					
					// previously enabled button was last in list
					// finding first in list and enable it
					if (is_last && is_set_found)
					{
						for (uint8_t i=0; i<MAX_BUTTONS_NUM; i++)
						{
							if (p_dev_config->buttons[i].physical_num == p_dev_config->buttons[num].physical_num &&
								p_dev_config->buttons[i].type == SEQUENTIAL_TOGGLE)
							{
								logical_buttons_state[i].delay_act = BUTTON_ACTION_DELAY;
								logical_buttons_state[i].time_last = millis;
								
								logical_buttons_state[i].on_state = 1;
								logical_buttons_state[i].off_state = 0;
								break;
							}
						}
					}
					
				}
				else if (!p_button_state->curr_physical_state)	// IDLE state
				{
					// nop
				}	
				
				break;

			case SEQUENTIAL_BUTTON:
				if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					// nop
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					p_button_state->current_state = p_button_state->on_state;
				}
				else if (p_button_state->curr_physical_state > p_button_state->prev_physical_state)		// triggered in IDLE
				{
					// searching for enabled button
					uint8_t is_last = 1;
					uint8_t is_set_found = 0;
					for (uint8_t i=0; i<MAX_BUTTONS_NUM; i++)
					{
						if (p_dev_config->buttons[i].physical_num == p_dev_config->buttons[num].physical_num &&
							p_dev_config->buttons[i].type == SEQUENTIAL_BUTTON)
						{
							//disable enabled button
							if (logical_buttons_state[i].on_state == 1 && 
									logical_buttons_state[i].delay_act == BUTTON_ACTION_IDLE)	// prevent multiple enabling
							{
								logical_buttons_state[i].on_state = 0;
								logical_buttons_state[i].off_state = 0;
								is_set_found = 1;
							}
							else if (is_set_found)	// enable next button in list
							{
								logical_buttons_state[i].delay_act = BUTTON_ACTION_DELAY;
								logical_buttons_state[i].time_last = millis;
								
								logical_buttons_state[i].on_state = 1;
								logical_buttons_state[i].off_state = 0;
								is_last = 0;
								break;
							}
						}
					}
					
					// previously enabled button was last in list
					// finding first in list and enable it
					if (is_last && is_set_found)
					{
						for (uint8_t i=0; i<MAX_BUTTONS_NUM; i++)
						{
							if (p_dev_config->buttons[i].physical_num == p_dev_config->buttons[num].physical_num &&
								p_dev_config->buttons[i].type == SEQUENTIAL_BUTTON)
							{
								logical_buttons_state[i].delay_act = BUTTON_ACTION_DELAY;
								logical_buttons_state[i].time_last = millis;
								
								logical_buttons_state[i].on_state = 1;
								logical_buttons_state[i].off_state = 0;
								break;
							}
						}
					}
					
				}
				else if (!p_button_state->curr_physical_state)	// IDLE state
				{
					p_button_state->current_state = p_button_state->off_state;
				}
				break;

			case LOGIC:
			{
				// Boolean expression over physical button inputs. Source A
				// is carried in the slot's physical_num (reused field);
				// Source B is in src_b (binary ops) and ignored for NOT.
				// Debounce duration comes from the slot's delay_timer
				// field, mapped onto button_timerN_ms; BUTTON_TIMER_OFF
				// means commit immediately each tick.
				int8_t a_idx = p_dev_config->buttons[num].physical_num;
				int8_t b_idx = p_dev_config->buttons[num].src_b;

				// Out-of-range / unassigned source -> treat as not pressed.
				uint8_t a = (a_idx >= 0 && a_idx < MAX_BUTTONS_NUM) ? (raw_buttons_data[a_idx] != 0) : 0;
				uint8_t b = (b_idx >= 0 && b_idx < MAX_BUTTONS_NUM) ? (raw_buttons_data[b_idx] != 0) : 0;

				uint8_t computed = 0;
				switch (p_dev_config->buttons[num].op)
				{
					case LOGIC_OP_AND:         computed =  (a &&  b); break;
					case LOGIC_OP_OR:          computed =  (a ||  b); break;
					case LOGIC_OP_NOT:         computed =  !a;        break;
					case LOGIC_OP_NOR:         computed = !(a ||  b); break;
					case LOGIC_OP_NAND:        computed = !(a &&  b); break;
					case LOGIC_OP_XOR:         computed =   a ^   b;  break;
					case LOGIC_OP_A_AND_NOT_B: computed =  (a && !b); break;
					default:                   computed = 0;          break;
				}

				// Resolve the debounce window from delay_timer, mirroring
				// the pattern other button types use for press_timer.
				uint16_t debounce_ms = 0;
				switch (p_dev_config->buttons[num].delay_timer)
				{
					case BUTTON_TIMER_1: debounce_ms = p_dev_config->button_timer1_ms; break;
					case BUTTON_TIMER_2: debounce_ms = p_dev_config->button_timer2_ms; break;
					case BUTTON_TIMER_3: debounce_ms = p_dev_config->button_timer3_ms; break;
					default:             debounce_ms = 0;                              break;	// BUTTON_TIMER_OFF
				}

				if (debounce_ms == 0)
				{
					// No debounce configured -- commit immediately.
					p_button_state->current_state = computed;
					p_button_state->delay_act = BUTTON_ACTION_IDLE;
				}
				else if (computed == p_button_state->current_state)
				{
					// Output already matches; cancel any pending transition.
					p_button_state->delay_act = BUTTON_ACTION_IDLE;
				}
				else if (p_button_state->delay_act != BUTTON_ACTION_DELAY ||
				         p_button_state->on_state != computed)
				{
					// Start (or re-start) the debounce window toward the new
					// computed value. on_state holds the pending output.
					p_button_state->delay_act = BUTTON_ACTION_DELAY;
					p_button_state->on_state  = computed;
					p_button_state->time_last = millis;
				}
				else if (millis - p_button_state->time_last >= debounce_ms)
				{
					// Stable for long enough -- commit the change.
					p_button_state->current_state = p_button_state->on_state;
					p_button_state->delay_act     = BUTTON_ACTION_IDLE;
				}
				break;
			}

			case TAP:
			{
				// Tap gesture: fires a short pulse if the physical is pressed
				// AND released within tap_cutoff_ms. Holding past the cutoff
				// aborts without firing -- letting the sister BUTTON_NORMAL slot
				// take the hold. On fire, gesture_claimed[physical] is set so
				// the sister NORMAL slot (if any) suppresses its delayed press.
				//
				// State held in delay_act:
				//   IDLE   - waiting for press
				//   DELAY  - press observed, waiting for release-within-cutoff
				//            or cutoff expiry
				//   PRESS  - TAP fired; pulse window active (TAP_PULSE_MS)
				//   BLOCK  - aborted (held past cutoff); waiting for release
				//
				// TAP_PULSE_MS is the duration current_state stays high after
				// fire. Long enough that the host samples it at least once;
				// short enough that quick re-taps stay responsive.
				#define TAP_PULSE_MS 100
				int8_t p = p_dev_config->buttons[num].physical_num;
				uint16_t cutoff_ms = p_dev_config->tap_cutoff_ms;
				uint8_t falling = (p_button_state->curr_physical_state < p_button_state->prev_physical_state);

				if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					// Pulse window. Drop after TAP_PULSE_MS.
					if (millis - p_button_state->time_last >= TAP_PULSE_MS)
					{
						/* If the user has already pressed for a re-tap
						 * during the pulse window, the rising edge was
						 * swallowed (PRESS branch doesn't track edges,
						 * and by the time we transition to IDLE both
						 * prev_physical_state and curr_physical_state
						 * are 1 so the IDLE rising-edge check below
						 * would fail). Detect that case here and arm a
						 * fresh DELAY directly. Conservative: we don't
						 * know the actual rising-edge time during the
						 * pulse, so cutoff_ms is measured from now. */
						if (p_button_state->curr_physical_state)
						{
							p_button_state->delay_act = BUTTON_ACTION_DELAY;
							p_button_state->time_last = millis;
							p_button_state->current_state = 0;
						}
						else
						{
							p_button_state->delay_act = BUTTON_ACTION_IDLE;
							p_button_state->current_state = 0;
						}
					}
					else
					{
						p_button_state->current_state = 1;
					}
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					if (falling)
					{
						/* Released while armed -- inside cutoff window by
						 * definition (cutoff-expiry branch below transitions
						 * out of DELAY before this can fire late). Two paths
						 * depending on whether a DOUBLE_TAP sister slot exists
						 * on this physical:
						 *   1. No DT sister  -> fire pulse immediately.
						 *   2. DT sister     -> defer the fire by dt_window_ms
						 *                       so a possible second rising
						 *                       edge can be interpreted as a
						 *                       double-tap (DT takes over,
						 *                       this TAP cancels). Set
						 *                       gesture_claimed optimistically
						 *                       so the NORMAL sister suppresses
						 *                       regardless of which way the
						 *                       decision goes -- DT will also
						 *                       claim on its own fire, and if
						 *                       TAP cancels then DT firing
						 *                       takes over the claim. */
						if (p >= 0 && p < MAX_BUTTONS_NUM && dt_window_ms[p] > 0)
						{
							p_button_state->delay_act = BUTTON_ACTION_TAP_PENDING;
							p_button_state->time_last = millis;
							p_button_state->current_state = 0;
							gesture_claimed[p] = 1;
						}
						else
						{
							p_button_state->delay_act = BUTTON_ACTION_PRESS;
							p_button_state->time_last = millis;
							p_button_state->current_state = 1;
							if (p >= 0 && p < MAX_BUTTONS_NUM) gesture_claimed[p] = 1;
						}
					}
					else if (millis - p_button_state->time_last >= cutoff_ms)
					{
						// Cutoff expired while still held -- abort.
						p_button_state->delay_act = BUTTON_ACTION_BLOCK;
						p_button_state->current_state = 0;
					}
					else
					{
						p_button_state->current_state = 0;
					}
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_TAP_PENDING)
				{
					/* Deferred fire: a release-within-cutoff happened with a
					 * DT sister present, so we're waiting one
					 * double_tap_window_ms to see if a second rising edge
					 * arrives. Three outcomes:
					 *   - Rising edge in window  -> double-tap pattern. DT
					 *                               handles it from here.
					 *                               Cancel this TAP into
					 *                               BLOCK so we wait for the
					 *                               release of the second tap
					 *                               before re-arming.
					 *   - Window elapses         -> single-tap confirmed.
					 *                               Fire the pulse now.
					 *   - Otherwise              -> hold off. */
					if (p_button_state->curr_physical_state > p_button_state->prev_physical_state)
					{
						p_button_state->delay_act = BUTTON_ACTION_BLOCK;
						p_button_state->current_state = 0;
					}
					else if (millis - p_button_state->time_last >= dt_window_ms[p])
					{
						p_button_state->delay_act = BUTTON_ACTION_PRESS;
						p_button_state->time_last = millis;
						p_button_state->current_state = 1;
						/* gesture_claimed already set optimistically when we
						 * entered TAP_PENDING; idempotent re-set here keeps
						 * the invariant readable. */
						if (p >= 0 && p < MAX_BUTTONS_NUM) gesture_claimed[p] = 1;
					}
					else
					{
						p_button_state->current_state = 0;
					}
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_BLOCK)
				{
					// Aborted; wait for release before re-arming so the next
					// rising edge starts a fresh attempt.
					if (p_button_state->curr_physical_state == 0)
					{
						p_button_state->delay_act = BUTTON_ACTION_IDLE;
					}
					p_button_state->current_state = 0;
				}
				else if (p_button_state->curr_physical_state > p_button_state->prev_physical_state)
				{
					// Rising edge in IDLE -- arm the cutoff timer.
					p_button_state->delay_act = BUTTON_ACTION_DELAY;
					p_button_state->time_last = millis;
					p_button_state->current_state = 0;
				}
				else
				{
					p_button_state->current_state = 0;
				}
				break;
			}

			case DOUBLE_TAP:
			{
				// Hold-while-second-tap-held gesture. Three states tracked in
				// tap_count: 0 = idle, 1 = first tap observed (window open),
				// 2 = second tap captured (mirroring physical until release).
				// Window measured as time from first rising edge to second rising
				// edge (double_tap_window_ms).
				int8_t p = p_dev_config->buttons[num].physical_num;
				uint16_t window_ms = p_dev_config->double_tap_window_ms;

				if (p_button_state->tap_count == 2)
				{
					// Captured -- mirror physical until release.
					p_button_state->current_state = p_button_state->curr_physical_state;
					if (p_button_state->curr_physical_state == 0)
					{
						p_button_state->tap_count = 0;
					}
				}
				else if (p_button_state->tap_count == 1)
				{
					if (p_button_state->curr_physical_state > p_button_state->prev_physical_state)
					{
						// Second rising edge within window -- capture and claim.
						p_button_state->tap_count = 2;
						p_button_state->current_state = 1;
						if (p >= 0 && p < MAX_BUTTONS_NUM) gesture_claimed[p] = 1;
					}
					else if ((millis - p_button_state->first_tap_ms) > window_ms)
					{
						// Window expired -- reset.
						p_button_state->tap_count = 0;
						p_button_state->current_state = 0;
					}
				}
				else if (p_button_state->curr_physical_state > p_button_state->prev_physical_state)
				{
					// First rising edge -- arm the window.
					p_button_state->tap_count = 1;
					p_button_state->first_tap_ms = millis;
					p_button_state->current_state = 0;
				}
				else
				{
					p_button_state->current_state = 0;
				}
				break;
			}

			default:
				break;
		}
}

/**
  * @brief  Set initial states for radio buttons
	* @param  p_dev_config: Pointer to device configuration
  * @retval None
  */
void RadioButtons_Init (dev_config_t * p_dev_config)
{
	for (uint8_t i=0; i<4; i++)
	{
		for (uint8_t j=0; j<MAX_BUTTONS_NUM; j++)
		{
			if (p_dev_config->buttons[j].type == (RADIO_BUTTON1 + i))
			{			
				logical_buttons_state[j].on_state = 1;
				logical_buttons_state[j].off_state = 0;
				logical_buttons_state[j].current_state = logical_buttons_state[j].on_state;
				break;
			}
		}
	}
}

/**
  * @brief  Set initial states for sequential buttons
	* @param  p_dev_config: Pointer to device configuration
  * @retval None
  */
void SequentialButtons_Init (dev_config_t * p_dev_config)
{
	// enable first
	for (uint8_t physical_num=0; physical_num<MAX_BUTTONS_NUM; physical_num++)
	{
		for (uint8_t i=0; i<MAX_BUTTONS_NUM; i++)
		{
			if (p_dev_config->buttons[i].type == SEQUENTIAL_TOGGLE &&
					p_dev_config->buttons[i].physical_num == physical_num)
			{
				logical_buttons_state[i].on_state = 1;
				logical_buttons_state[i].current_state = 1;
				break;
			}
		}
	}
	// enable last
//	for (uint8_t physical_num=0; physical_num<MAX_BUTTONS_NUM; physical_num++)
//	{
//		uint8_t k=0;
//		for (uint8_t i=0; i<MAX_BUTTONS_NUM; i++)
//		{
//			if (p_dev_config->buttons[i].type == SEQUENTIAL_BUTTON &&
//					p_dev_config->buttons[i].physical_num == physical_num)
//			{
//				k++;
//			}
//		}
//		if (k>0) logical_buttons_state[k-1].on_state = 1;
//		
//	}
	
		for (int8_t physical_num = MAX_BUTTONS_NUM - 1; physical_num > -1; physical_num--)
	{
		for (int8_t i = MAX_BUTTONS_NUM - 1; i > -1; i--)
		{
			if (p_dev_config->buttons[i].type == SEQUENTIAL_BUTTON &&
					p_dev_config->buttons[i].physical_num != physical_num)
			{
				logical_buttons_state[i].on_state = 1;
				//buttons_state[i].current_state = 1;
				//buttons_state[i].prev_state = 1;
				break;
			}
		}
	}
}

/**
  * @brief  Build per-physical gesture lookup tables. Called once at boot.
  *         Configurator save triggers NVIC_SystemReset(), so a single
  *         boot-time rebuild covers every config-change path.
  * @param  p_dev_config: Pointer to device configuration
  * @retval None
  */
void Gestures_Init (dev_config_t * p_dev_config)
{
	for (uint8_t p = 0; p < MAX_BUTTONS_NUM; p++)
	{
		gesture_delay_ms[p] = 0;
		gesture_claimed[p] = 0;
		gesture_low_since[p] = 0;
		dt_window_ms[p] = 0;
	}
	/* Two-pass build:
	 *   pass 1 -- flag which physicals host a TAP and/or DT sister,
	 *             record dt_window_ms per physical for the TAP case to
	 *             consult at fire time.
	 *   pass 2 -- compute gesture_delay_ms per physical from the flags
	 *             so NORMAL sister slots wait the full gesture-decision
	 *             window before transitioning to PRESS.
	 *
	 * Per-physical decision-window math:
	 *   - TAP only:    cutoff_ms (TAP fires immediately at release)
	 *   - DT only:     dt_window_ms (DT fires at second rising or aborts)
	 *   - TAP+DT both: cutoff_ms + dt_window_ms (TAP defers up to
	 *                  dt_window_ms after a release-at-cutoff to confirm
	 *                  no second tap; NORMAL must wait that long before
	 *                  transitioning to PRESS to suppress correctly) */
	uint8_t has_tap[MAX_BUTTONS_NUM];
	uint8_t has_dt[MAX_BUTTONS_NUM];
	for (uint8_t p = 0; p < MAX_BUTTONS_NUM; p++) {
		has_tap[p] = 0;
		has_dt[p] = 0;
	}
	for (uint8_t s = 0; s < MAX_BUTTONS_NUM; s++)
	{
		button_type_t t = p_dev_config->buttons[s].type;
		int8_t p = p_dev_config->buttons[s].physical_num;
		if (p < 0 || p >= MAX_BUTTONS_NUM) continue;

		if (t == TAP) {
			has_tap[p] = 1;
		}
		else if (t == DOUBLE_TAP) {
			has_dt[p] = 1;
			dt_window_ms[p] = p_dev_config->double_tap_window_ms;
		}
	}
	for (uint8_t p = 0; p < MAX_BUTTONS_NUM; p++) {
		uint16_t total = 0;
		if (has_tap[p]) total += p_dev_config->tap_cutoff_ms;
		if (has_dt[p])  total += p_dev_config->double_tap_window_ms;
		gesture_delay_ms[p] = total;
	}
}

/**
  * @brief  Per-tick sweep: clear gesture_claimed[] entries whose physical
  *         has been low long enough that any pending sister NORMAL slot's
  *         DELAY-to-PRESS transition has already resolved. Called from
  *         ButtonsDebounceProcess once per tick.
  * @param  millis: current millisecond timestamp (from GetMillis())
  * @retval None
  */
static void GestureClaimedSweep (int32_t millis)
{
	for (uint8_t p = 0; p < MAX_BUTTONS_NUM; p++)
	{
		if (physical_buttons_state[p].current_state == 0)
		{
			if (gesture_low_since[p] == 0) gesture_low_since[p] = millis;
			if (gesture_claimed[p] && gesture_delay_ms[p] > 0 &&
			    (millis - gesture_low_since[p]) > gesture_delay_ms[p])
			{
				gesture_claimed[p] = 0;
			}
		}
		else
		{
			gesture_low_since[p] = 0;
		}
	}
}

/**
  * @brief  Checking single button state
  * @param  pin_num:	Number of pin where button is connected
	* @param  p_dev_config: Pointer to device configuration
  * @retval Buttons state
  */
uint8_t DirectButtonGet (uint8_t pin_num,  dev_config_t * p_dev_config)
{
	if (p_dev_config->pins[pin_num] == BUTTON_VCC)
	{
		return Board_PinRead(pin_num);
	}
	else
	{
		return !Board_PinRead(pin_num);
	}
}

/**
  * @brief  Getting buttons states of matrix buttons
	* @param  raw_button_data_buf: Pointer to raw buttons data buffer
	* @param  p_dev_config: Pointer to device configuration
	* @param  pos: Pointer to button position counter
  * @retval None
  */
void MaxtrixButtonsGet (uint8_t * raw_button_data_buf, dev_config_t * p_dev_config, uint8_t * pos)
{
	// get matrix buttons
	for (int i=0; i<USED_PINS_NUM; i++)
	{
		if ((p_dev_config->pins[i] == BUTTON_ROW) && ((*pos) < MAX_BUTTONS_NUM))
		{
			// tie Row pin to ground
			Board_PinWrite(i, 0);

			// get states at Columns
			for (int k=0; k<USED_PINS_NUM; k++)
			{
				if (p_dev_config->pins[k] == BUTTON_COLUMN && (*pos) < MAX_BUTTONS_NUM)
				{
					raw_button_data_buf[*pos] = DirectButtonGet(k, p_dev_config);
					(*pos)++;
				}
			}
			// return Row pin to Hi-Z state
			Board_PinWrite(i, 1);
		}
	}
}

/**
  * @brief  Getting buttons states of single buttons
	* @param  raw_button_data_buf: Pointer to raw buttons data buffer
	* @param  p_dev_config: Pointer to device configuration
	* @param  pos: Pointer to button position counter
  * @retval None
  */
void SingleButtonsGet (uint8_t * raw_button_data_buf, dev_config_t * p_dev_config, uint8_t * pos)
{
	for (int i=0; i<USED_PINS_NUM; i++)
	{
		if (p_dev_config->pins[i] == BUTTON_GND || 
				p_dev_config->pins[i] == BUTTON_VCC)
		{
			if ((*pos) < MAX_BUTTONS_NUM)
			{
				raw_button_data_buf[*pos] = DirectButtonGet(i, p_dev_config);
				(*pos)++;
			}
			else break;
		}
	}
}


uint8_t ButtonsReadPhysical(dev_config_t * p_dev_config, uint8_t * p_buf)
{
	uint8_t pos = 0;
	
	// Getting physical buttons states
	MaxtrixButtonsGet(p_buf, p_dev_config, &pos);
	ShiftRegistersGet(p_buf, p_dev_config, &pos);
	a2b_first = pos;
	AxisToButtonsGet(p_buf, p_dev_config, &pos);
	a2b_last = pos;
	SingleButtonsGet(p_buf, p_dev_config, &pos);
	
	return pos;
}

/**
  * @brief  Checking all buttons routine
	* @param  p_dev_config: Pointer to device configuration
  * @retval None
  */
void ButtonsReadLogical (dev_config_t * p_dev_config)
{
	// Process regular buttons
	for (uint8_t i=0; i<MAX_BUTTONS_NUM; i++)
	{
		uint8_t shift_num = 0;
		
		// check logical buttons to have shift modificators							// disable if no shift?
		uint8_t any_shift_configured = 0;
		for (uint8_t s = 0; s < MAX_SHIFTS_NUM; s++) {
			if (p_dev_config->shift_config[s].button >= 0) { any_shift_configured = 1; break; }
		}
		if (any_shift_configured)
		{
			for (uint8_t j=0; j<MAX_BUTTONS_NUM; j++)
			{
				int8_t btn = p_dev_config->buttons[j].physical_num;
				
				if (btn == i && (p_dev_config->buttons[j].shift_modificator))				// we found button with shift modificator 
				{
					shift_num = p_dev_config->buttons[j].shift_modificator;
					if (shifts_state & 1<<(shift_num-1))											// shift pressed for this button
					{
						logical_buttons_state[j].prev_physical_state = logical_buttons_state[j].curr_physical_state;
						logical_buttons_state[j].curr_physical_state = physical_buttons_state[p_dev_config->buttons[j].physical_num].current_state;
						
						LogicalButtonProcessState(&logical_buttons_state[j], pov_pos, p_dev_config, j);
					}
					else if (logical_buttons_state[j].current_state)	// shift released for this button
					{
						// disable button
						logical_buttons_state[j].delay_act = BUTTON_ACTION_IDLE;
						logical_buttons_state[j].on_state = 0;
						logical_buttons_state[j].off_state = 0;
						logical_buttons_state[j].current_state = 0;
						logical_buttons_state[j].curr_physical_state = 0;
						logical_buttons_state[j].time_last = 0;			
						LogicalButtonProcessState(&logical_buttons_state[j], pov_pos, p_dev_config, j);
					}
				}
			}
		}
		
		if (shift_num == 0)		// we found not shift modificated physical button
		{
			for (uint8_t j=0; j<MAX_BUTTONS_NUM; j++)
			{
				if (p_dev_config->buttons[j].physical_num == i)		// we found corresponding logical button
				{
					logical_buttons_state[j].prev_physical_state = logical_buttons_state[j].curr_physical_state;
					logical_buttons_state[j].curr_physical_state = physical_buttons_state[p_dev_config->buttons[j].physical_num].current_state;		
					
					LogicalButtonProcessState(&logical_buttons_state[j], pov_pos, p_dev_config, j);
				}
			}
		}		
		else	// check if shift is released for modificated physical button
		{
			/* For unmodified logical buttons (shift_modificator == 0): if no
			 * shift is currently active, mirror the physical state through.
			 * If any shift is active, suppress -- and clear residual state if
			 * the button was on. Issue anpeaco/FreeJoyX#1 collapsed the prior
			 * 5-way unrolled if/else cascade (one branch per shift bit) into
			 * this single check; each branch was identical bookkeeping. The
			 * cascade was also off-by-shift-count, since extending the array
			 * past 5 left bits 5..7 silently un-handled. */
			for (uint8_t j=0; j<MAX_BUTTONS_NUM; j++)
			{
				if (p_dev_config->buttons[j].physical_num != i ||
				    p_dev_config->buttons[j].shift_modificator != 0)
					continue;

				if (shifts_state == 0)
				{
					logical_buttons_state[j].prev_physical_state = logical_buttons_state[j].curr_physical_state;
					logical_buttons_state[j].curr_physical_state = physical_buttons_state[p_dev_config->buttons[j].physical_num].current_state;
					LogicalButtonProcessState(&logical_buttons_state[j], pov_pos, p_dev_config, j);
				}
				else if (logical_buttons_state[j].current_state)
				{
					logical_buttons_state[j].delay_act = BUTTON_ACTION_IDLE;
					logical_buttons_state[j].on_state = 0;
					logical_buttons_state[j].off_state = 0;
					logical_buttons_state[j].current_state = 0;
					logical_buttons_state[j].curr_physical_state = 0;
					logical_buttons_state[j].time_last = 0;
					LogicalButtonProcessState(&logical_buttons_state[j], pov_pos, p_dev_config, j);
				}
			}
		}
	}	
	
	shifts_state = 0;
	for (uint8_t i=0; i<MAX_SHIFTS_NUM; i++)
	{
		if (p_dev_config->shift_config[i].button >= 0)
		{
			shifts_state |= (logical_buttons_state[p_dev_config->shift_config[i].button].current_state << i);
		}
	}
	
	// convert data to report format	
	uint8_t k = 0;
	
	// buttons read is permitted
	button_mutex = 1;
	
	memset(out_buttons_data, 0, sizeof(out_buttons_data));
	memset(log_buttons_data, 0, sizeof(log_buttons_data));
	memset(phy_buttons_data, 0, sizeof(phy_buttons_data));
	
	for (int i=0;i<MAX_BUTTONS_NUM;i++)
	{
			uint8_t is_enabled = !p_dev_config->buttons[i].is_disabled && (p_dev_config->buttons[i].physical_num >= 0);
			// joy buttons
			if (is_enabled)
			{
				//out_buttons_data[(k & 0xF8)>>3] &= ~(1 << (k & 0x07));
				if (!p_dev_config->buttons[i].is_inverted)
				{					
					out_buttons_data[(k & 0xF8)>>3] |= (logical_buttons_state[i].current_state << (k & 0x07));
				}
				else
				{
					out_buttons_data[(k & 0xF8)>>3] |= (!logical_buttons_state[i].current_state << (k & 0x07));
				}
				k++;				
			}
			// logical buttons
			if (!p_dev_config->buttons[i].is_inverted)
			{
				log_buttons_data[(i & 0xF8)>>3] |= (logical_buttons_state[i].current_state << (i & 0x07));
			}
			else
			{
				log_buttons_data[(i & 0xF8)>>3] |= (!logical_buttons_state[i].current_state << (i & 0x07));
			}
			// physical buttons
			phy_buttons_data[(i & 0xF8)>>3] |= (physical_buttons_state[i].current_state << (i & 0x07));			
	}
	
	// buttons read is allowed
	button_mutex = 0;
	
	// convert POV data to report format
	for (int i=0; i<MAX_POVS_NUM; i++)
	{
		switch (pov_pos[i])
		{
			case 1:
				pov_data[i] = 0x06;
				break;
			case 2:
				pov_data[i] = 0x04;
				break;
			case 3:
				pov_data[i] = 0x05;
				break;
			case 4:
				pov_data[i] = 0x02;
				break;
			case 6:
				pov_data[i] = 0x03;
				break;
			case 8:
				pov_data[i] = 0x00;
				break;
			case 9:
				pov_data[i] = 0x07;
				break;
			case 12:
				pov_data[i] = 0x01;
				break;
			default:
				pov_data[i] = 0xFF;
				break;
		}
	}
}

/**
  * @brief  Getting buttons data in report format
	* @param  raw_data: Pointer to target buffer of physical buttons
	* @param  data: Pointer to target buffer of logical buttons
  * @retval None
  */
void ButtonsGet (uint8_t * out_data, uint8_t * log_data, uint8_t * phy_data, uint8_t * shift_data)
{
	if (out_data != NULL && !button_mutex)
	{
		memcpy(out_data, out_buttons_data, sizeof(out_buttons_data));
	}
	if (log_data != NULL  && !button_mutex)
	{
		memcpy(log_data, log_buttons_data, sizeof(log_buttons_data));
	}
	if (phy_data != NULL  && !button_mutex)
	{
		memcpy(phy_data, &phy_buttons_data, sizeof(phy_buttons_data));
	}
	if (shift_data != NULL  && !button_mutex)
	{
		memcpy(shift_data, &shifts_state, sizeof(shifts_state));
	}
}
/**
  * @brief  Getting POV data in report format
	* @param  data: Pointer to target buffer
  * @retval None
  */
void POVsGet (pov_data_t * data)
{
	if (data != NULL)
	{
		memcpy(data, pov_data, sizeof(pov_data));
	}
}



