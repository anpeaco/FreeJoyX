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

/* Per-physical gesture state machine (issue anpeaco/FreeJoyX#21).
 *
 * Replaces the previous per-slot TAP / DOUBLE_TAP / NORMAL state machines
 * + gesture_claimed coordination. One state machine per physical input
 * observes the rising/falling edges and tracks which gesture the user is
 * performing. The TAP / DT / NORMAL cases in LogicalButtonProcessState
 * become passive consumers that derive their output from gesture[p].state
 * without writing to any shared coordination flags. Eliminates the
 * slot-iteration-order races that plagued the earlier coordination model.
 *
 * State transitions:
 *   IDLE             + rising            -> ARMED (arm_time = now)
 *   ARMED            + falling (hold <= cutoff):
 *                                        -> WAIT_DT (if has_dt[p]; release_time = now)
 *                                        -> IDLE + fire TAP pulse (if has_tap[p] and !has_dt[p])
 *                                        -> IDLE (otherwise)
 *   ARMED            + tick (held > cutoff):
 *                                        -> NORMAL_HELD (if has_normal[p])
 *                                        -> ABORTED     (otherwise)
 *   NORMAL_HELD      + falling           -> IDLE
 *   ABORTED          + falling           -> IDLE
 *   WAIT_DT          + rising            -> DT_HELD
 *   WAIT_DT          + tick (dt_window elapsed since release):
 *                                        -> IDLE + fire TAP pulse (if has_tap[p])
 *                                        -> IDLE                  (otherwise)
 *   DT_HELD          + falling           -> IDLE
 *
 * Pure-NORMAL physicals (no TAP/DT sister) are not driven by this state
 * machine -- they keep the existing standard-timer-based NORMAL logic so
 * per-slot delay_timer / press_timer settings still work. */
enum {
	GESTURE_STATE_IDLE = 0,
	GESTURE_STATE_ARMED,
	GESTURE_STATE_NORMAL_HELD,
	GESTURE_STATE_ABORTED,
	GESTURE_STATE_WAIT_DT,
	GESTURE_STATE_DT_HELD,
};

typedef struct {
	uint8_t  state;            /* GESTURE_STATE_* */
	uint8_t  prev_physical;    /* previous tick's debounced physical state */
	int32_t  arm_time;         /* timestamp of the current press cycle's rising edge */
	int32_t  release_time;     /* timestamp of falling that entered WAIT_DT */
	int32_t  tap_pulse_until;  /* end-of-pulse timestamp for TAP slot output */
} gesture_t;

static gesture_t              gesture[MAX_BUTTONS_NUM];
static uint8_t                has_tap[MAX_BUTTONS_NUM];
static uint8_t                has_dt[MAX_BUTTONS_NUM];
static uint8_t                has_normal[MAX_BUTTONS_NUM];

/* MIN_PULSE_MS: absolute minimum logical-button pulse width on
 * gesture-managed slots (TAP / DOUBLE_TAP / gesture-coexisting NORMAL).
 * Guarantees a host polling at 125 Hz samples the press at least twice
 * even with poll-window skew. The per-slot press_timer can extend
 * beyond this; nothing can shorten it. Issue anpeaco/FreeJoyX#22. */
#define MIN_PULSE_MS 20

/**
  * @brief  Resolve a gesture-managed slot's effective press-floor in ms.
  *         Reads the slot's press_timer selector, looks up the global
  *         button_timerN_ms value, and applies the MIN_PULSE_MS floor.
  *         Slots configured with BUTTON_TIMER_NONE get exactly
  *         MIN_PULSE_MS.
  * @retval Floor duration in ms (>= MIN_PULSE_MS).
  */
static uint16_t ResolvePressFloorMs (dev_config_t * p_dev_config, uint8_t num)
{
	uint16_t t;
	switch (p_dev_config->buttons[num].press_timer)
	{
		case BUTTON_TIMER_1: t = p_dev_config->button_timer1_ms; break;
		case BUTTON_TIMER_2: t = p_dev_config->button_timer2_ms; break;
		case BUTTON_TIMER_3: t = p_dev_config->button_timer3_ms; break;
		default:             t = 0; break;
	}
	return (t < MIN_PULSE_MS) ? MIN_PULSE_MS : t;
}

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

	/* Issue anpeaco/FreeJoyX#21: BUTTON_NORMAL slots whose physical hosts a
	 * TAP or DOUBLE_TAP sister are driven directly by the per-physical
	 * gesture state machine in LogicalButtonProcessState and never enter
	 * delay_act, so no delay-window extension is needed here. Pure-NORMAL
	 * physicals continue to use the configured per-slot delay/press timers. */


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
			{
				/* Issue anpeaco/FreeJoyX#21: when a NORMAL slot's physical
				 * also hosts a TAP or DOUBLE_TAP sister, output is derived
				 * from the per-physical gesture state machine instead of the
				 * per-slot delay/press timers. NORMAL only fires while the
				 * state machine is in NORMAL_HELD (hold-past-cutoff). This
				 * eliminates the slot-iteration-order races that drove the
				 * gesture_claimed bug iterations. POV*_CENTER share this case
				 * body but live on their own physical inputs and are not
				 * affected (has_tap / has_dt only get set for actual TAP /
				 * DOUBLE_TAP slots, so the predicate is false for them). */
				if (p_dev_config->buttons[num].type == BUTTON_NORMAL)
				{
					int8_t p = p_dev_config->buttons[num].physical_num;
					if (p >= 0 && p < MAX_BUTTONS_NUM && (has_tap[p] || has_dt[p]))
					{
						/* Issue #22: re-arm release_floor each tick while
						 * NORMAL_HELD; on exit, output stays high until the
						 * floor expires. */
						if (gesture[p].state == GESTURE_STATE_NORMAL_HELD)
						{
							p_button_state->release_floor =
							    millis + ResolvePressFloorMs(p_dev_config, num);
							p_button_state->current_state = 1;
						}
						else
						{
							p_button_state->current_state =
							    (p_button_state->release_floor > millis) ? 1 : 0;
						}
						break;
					}
				}
				if (p_button_state->delay_act == BUTTON_ACTION_DELAY)
				{
					// Rising edge while already in DELAY = quick re-press
					// (e.g. second tap of a double-tap on a TAP+NORMAL physical).
					// Re-arm the gesture window from the new rising edge so the
					// new press cycle evaluates independently. Without this,
					// NORMAL's time_last stayed pinned to the original press,
					// and the standard timer's DELAY->PRESS transition could
					// fire before/around the moment GestureClaimedSweep cleared
					// the previous cycle's claim, causing NORMAL to flash for
					// one tick.
					//
					// gesture_claimed handling: do NOT clear it here. The flag's
					// lifecycle is owned by GestureClaimedSweep, which clears
					// it after the physical has been low for gesture_delay_ms.
					// Clearing on re-arm causes a slot-iteration race in the
					// TAP+DT+NORMAL case: NORMAL processed AFTER DT clobbers
					// the claim that DT just set on the second-tap rising
					// edge, defeating mutual exclusion. The cost is that for
					// TAP+NORMAL setups, a quick re-press within the gesture
					// window won't fire NORMAL on the hold (gesture_claimed
					// is still set from the prior tap). Acceptable: that's
					// double-tap-cadence territory and the user opted into
					// it by not configuring a DT slot.
					if (p_dev_config->buttons[num].type == BUTTON_NORMAL &&
					    p_button_state->curr_physical_state > p_button_state->prev_physical_state)
					{
						p_button_state->time_last = millis;
						p_button_state->on_state = p_button_state->curr_physical_state;
					}
					// else: nop (still waiting out the gesture window)
				}
				else if (p_button_state->delay_act == BUTTON_ACTION_PRESS)
				{
					p_button_state->current_state = p_button_state->on_state;
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
			}

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
				//
				// Sources read the DEBOUNCED physical state
				// (physical_buttons_state[].current_state) -- the SAME value
				// BUTTON_NORMAL slots consume -- so a LOGIC slot and a NORMAL
				// slot on the same physical change in lockstep, both inheriting
				// button_debounce_ms. (These previously read raw_buttons_data
				// directly with no debounce, so LOGIC fired on the raw edge
				// ~button_debounce_ms before a NORMAL sister settled.) The
				// optional per-slot debounce below (delay_timer ->
				// button_timerN_ms; BUTTON_TIMER_OFF = commit immediately) still
				// applies on top, as an additional debounce of the computed
				// RESULT -- useful for binary-encoded rotary switches that flash
				// adjacent codes mid-throw.
				int8_t a_idx = p_dev_config->buttons[num].physical_num;
				int8_t b_idx = p_dev_config->buttons[num].src_b;

				// Out-of-range / unassigned source -> treat as not pressed.
				uint8_t a = (a_idx >= 0 && a_idx < MAX_BUTTONS_NUM) ? (physical_buttons_state[a_idx].current_state != 0) : 0;
				uint8_t b = (b_idx >= 0 && b_idx < MAX_BUTTONS_NUM) ? (physical_buttons_state[b_idx].current_state != 0) : 0;

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
					case LOGIC_OP_XNOR:        computed = !(a ^   b); break;
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
				/* Issue anpeaco/FreeJoyX#21: TAP slot reads the per-physical
				 * gesture state machine's tap_pulse_until marker (which is
				 * set MIN_PULSE_MS in the future on fire). Issue #22 adds
				 * the per-slot press_timer floor: arm release_floor to
				 * fire_time + max(press_timer, MIN_PULSE_MS) so the host
				 * sees a press of at least that duration. */
				int8_t p = p_dev_config->buttons[num].physical_num;
				if (p >= 0 && p < MAX_BUTTONS_NUM &&
				    gesture[p].tap_pulse_until > millis)
				{
					/* Fire window currently open in the state machine.
					 * Slide release_floor out to (fire-time + slot floor).
					 * fire-time == tap_pulse_until - MIN_PULSE_MS by
					 * construction (set by GestureProcessAll). */
					int32_t fire_time = gesture[p].tap_pulse_until - MIN_PULSE_MS;
					int32_t deadline  = fire_time + ResolvePressFloorMs(p_dev_config, num);
					if (deadline > p_button_state->release_floor)
					{
						p_button_state->release_floor = deadline;
					}
				}
				p_button_state->current_state =
				    (p_button_state->release_floor > millis) ? 1 : 0;
				break;
			}

			case DOUBLE_TAP:
			{
				/* Issue anpeaco/FreeJoyX#21: DT slot reads gesture[p].state.
				 * Issue #22: while DT_HELD and physical pressed, re-arm
				 * release_floor each tick. After release (state -> IDLE or
				 * curr_physical == 0), output stays high until the floor
				 * expires. */
				int8_t p = p_dev_config->buttons[num].physical_num;
				uint8_t high =
				    (p >= 0 && p < MAX_BUTTONS_NUM &&
				     gesture[p].state == GESTURE_STATE_DT_HELD &&
				     p_button_state->curr_physical_state);
				if (high)
				{
					p_button_state->release_floor =
					    millis + ResolvePressFloorMs(p_dev_config, num);
					p_button_state->current_state = 1;
				}
				else
				{
					p_button_state->current_state =
					    (p_button_state->release_floor > millis) ? 1 : 0;
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
		has_tap[p] = 0;
		has_dt[p] = 0;
		has_normal[p] = 0;
		gesture[p].state = GESTURE_STATE_IDLE;
		gesture[p].prev_physical = 0;
		gesture[p].arm_time = 0;
		gesture[p].release_time = 0;
		gesture[p].tap_pulse_until = 0;
	}
	for (uint8_t s = 0; s < MAX_BUTTONS_NUM; s++)
	{
		button_type_t t = p_dev_config->buttons[s].type;
		int8_t p = p_dev_config->buttons[s].physical_num;
		if (p < 0 || p >= MAX_BUTTONS_NUM) continue;

		if (t == TAP)             has_tap[p] = 1;
		else if (t == DOUBLE_TAP) has_dt[p] = 1;
		else if (t == BUTTON_NORMAL) has_normal[p] = 1;
	}
}

/**
  * @brief  Run the per-physical gesture state machine for every physical
  *         input that hosts at least one TAP or DOUBLE_TAP slot.
  *         Issue anpeaco/FreeJoyX#21.
  *
  *         Called once per main-loop tick from ButtonsReadLogical, BEFORE
  *         the per-slot processing. Slot output code in
  *         LogicalButtonProcessState reads gesture[p].state and
  *         gesture[p].tap_pulse_until to derive its current_state.
  * @param  p_dev_config: Pointer to device configuration
  * @param  millis: current millisecond timestamp
  * @retval None
  */
static void GestureProcessAll (dev_config_t * p_dev_config, int32_t millis)
{
	const uint16_t tap_cutoff = p_dev_config->tap_cutoff_ms;
	const uint16_t dt_window = p_dev_config->double_tap_window_ms;

	for (uint8_t p = 0; p < MAX_BUTTONS_NUM; p++)
	{
		/* Skip physicals with no TAP or DT sister -- pure-NORMAL inputs
		 * keep their existing standard-timer logic (so per-slot
		 * delay_timer / press_timer settings still work). */
		if (!has_tap[p] && !has_dt[p]) continue;

		uint8_t curr = physical_buttons_state[p].current_state;
		uint8_t rising  = (curr > gesture[p].prev_physical);
		uint8_t falling = (curr < gesture[p].prev_physical);
		gesture[p].prev_physical = curr;

		switch (gesture[p].state)
		{
			case GESTURE_STATE_IDLE:
				if (rising)
				{
					gesture[p].state = GESTURE_STATE_ARMED;
					gesture[p].arm_time = millis;
				}
				break;

			case GESTURE_STATE_ARMED:
				if (falling)
				{
					if (millis - gesture[p].arm_time <= tap_cutoff)
					{
						if (has_dt[p])
						{
							gesture[p].state = GESTURE_STATE_WAIT_DT;
							gesture[p].release_time = millis;
						}
						else if (has_tap[p])
						{
							gesture[p].tap_pulse_until = millis + MIN_PULSE_MS;
							gesture[p].state = GESTURE_STATE_IDLE;
						}
						else
						{
							gesture[p].state = GESTURE_STATE_IDLE;
						}
					}
					else
					{
						/* Cutoff timeout already transitioned us out of ARMED
						 * before this falling could be observed -- but
						 * defensively fall back to IDLE. */
						gesture[p].state = GESTURE_STATE_IDLE;
					}
				}
				else if (millis - gesture[p].arm_time > tap_cutoff)
				{
					gesture[p].state = has_normal[p]
					                   ? GESTURE_STATE_NORMAL_HELD
					                   : GESTURE_STATE_ABORTED;
				}
				break;

			case GESTURE_STATE_NORMAL_HELD:
				if (falling) gesture[p].state = GESTURE_STATE_IDLE;
				break;

			case GESTURE_STATE_ABORTED:
				if (falling) gesture[p].state = GESTURE_STATE_IDLE;
				break;

			case GESTURE_STATE_WAIT_DT:
				if (rising)
				{
					gesture[p].state = GESTURE_STATE_DT_HELD;
				}
				else if (millis - gesture[p].release_time > dt_window)
				{
					if (has_tap[p])
					{
						gesture[p].tap_pulse_until = millis + MIN_PULSE_MS;
					}
					gesture[p].state = GESTURE_STATE_IDLE;
				}
				break;

			case GESTURE_STATE_DT_HELD:
				if (falling) gesture[p].state = GESTURE_STATE_IDLE;
				break;
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
	/* Run the per-physical gesture state machine once before any
	 * per-slot processing. TAP / DT / NORMAL cases below derive their
	 * output from gesture[p].state. Issue anpeaco/FreeJoyX#21. */
	GestureProcessAll(p_dev_config, GetMillis());

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



