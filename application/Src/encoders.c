/**
  ******************************************************************************
  * @file           : encoders.c
  * @brief          : Encoders driver implementation
		
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

#include "encoders.h"
#include "board_encoder.h"
#include "buttons.h"		// raw_buttons_data, DirectButtonGet, a2b_last


/* Quarter-step transition table indexed by (prev << 2 | curr), where each
 * 2-bit half is (Pin B << 1 | Pin A). Every valid single-step Gray transition
 * maps to +1 (CW) or -1 (CCW); no-change and invalid 2-bit jumps map to 0. The
 * decoder accumulates these quarter-steps and emits a detent every N of them
 * (N = 4/2/1 for the 1x/2x/4x modes), which makes it immune to contact bounce
 * and tolerant of slow / partial detents. Swapping Pin A / Pin B (in the
 * configurator) reverses direction by flipping the low bit of each half. */
const int8_t enc_array_4 [16] =
{
	0,  1, -1,  0,
	-1,  0,  0,  1,
	1,  0,  0, -1,
	0, -1,  1,  0
};

/* Ben Buxton full-step state machine -- the standard decoder for DETENTED
 * mechanical encoders (1 detent = 1 full quadrature cycle). Used for
 * ENCODER_CONF_1x. Unlike the count-and-divide path (used for 2x/4x), it is
 * LOCKED to the detent: it walks the exact 00->01->11->10->00 (CW) or
 * 00->10->11->01->00 (CCW) sequence and emits exactly one step at the instant a
 * click settles back to rest, so it cannot drift out of phase and settling
 * wobble at a detent bounces harmlessly at R_START. Any invalid (both-channel)
 * transition also falls back to R_START, so EMI / skipped states self-heal.
 * Index: [fsm state][2-bit input], input = (Pin B << 1 | Pin A). High nibble of
 * the result carries the emit flag. Direction matches enc_array_4: Pin A leads
 * -> CW (fires Pin A), Pin B leads -> CCW (fires Pin B); swap the pins to reverse.
 *
 * NOTE: this is the REST-AT-00 mirror of Buxton's published table, NOT a verbatim
 * copy -- do not "fix" it to match his source or it will abort on our encoders.
 * Buxton reads the pins raw, so a pull-up encoder rests at 11 and his table emits
 * on the ...->11 return. FreeJoy's DirectButtonGet NORMALISES every input to
 * active-high ("1 = pressed"), and a detent = both contacts open = both inactive,
 * so the decoder always sees rest at 00 regardless of Button GND/VCC wiring.
 * This table is the corresponding rest-at-00 variant (emits on the ...->00
 * return); it is what makes 1x read exactly one step per detent on real hardware
 * (verified via the encoder monitor: 10 detents -> net +/-40, 40 valid, 0 jumps). */
#define ENC_R_START      0
#define ENC_R_CW_BEGIN   1
#define ENC_R_CW_NEXT    2
#define ENC_R_CW_FINAL   3
#define ENC_R_CCW_BEGIN  4
#define ENC_R_CCW_NEXT   5
#define ENC_R_CCW_FINAL  6
#define ENC_DIR_CW       0x10
#define ENC_DIR_CCW      0x20
static const uint8_t enc_fsm_full[7][4] =
{
	/*                       00                      01(A)        10(B)        11    */
	/* R_START     */ { ENC_R_START,            ENC_R_CW_BEGIN,  ENC_R_CCW_BEGIN, ENC_R_START     },
	/* R_CW_BEGIN  */ { ENC_R_START,            ENC_R_CW_BEGIN,  ENC_R_START,     ENC_R_CW_NEXT   },
	/* R_CW_NEXT   */ { ENC_R_START,            ENC_R_CW_BEGIN,  ENC_R_CW_FINAL,  ENC_R_CW_NEXT   },
	/* R_CW_FINAL  */ { ENC_R_START|ENC_DIR_CW, ENC_R_START,     ENC_R_CW_FINAL,  ENC_R_CW_NEXT   },
	/* R_CCW_BEGIN */ { ENC_R_START,            ENC_R_START,     ENC_R_CCW_BEGIN, ENC_R_CCW_NEXT  },
	/* R_CCW_NEXT  */ { ENC_R_START,            ENC_R_CCW_FINAL, ENC_R_CCW_BEGIN, ENC_R_CCW_NEXT  },
	/* R_CCW_FINAL */ { ENC_R_START|ENC_DIR_CCW,ENC_R_CCW_FINAL, ENC_R_START,     ENC_R_CCW_NEXT  },
};

/* Ben Buxton HALF-step state machine -- for ENCODER_CONF_2x, where the encoder
 * detents at BOTH 00 and 11 (2 detents per quadrature cycle). Same drift-free,
 * detent-locked property as the full-step table: it emits one step each time a
 * click settles into a rest state (00 or 11). Index: [fsm state][2-bit input],
 * input = (Pin B << 1 | Pin A); high nibble carries the emit flag. 4x needs no
 * table -- at full resolution every valid transition emits immediately, so there
 * is no accumulator to drift. */
#define ENC_HR_00      0
#define ENC_HR_00_CW   1
#define ENC_HR_00_CCW  2
#define ENC_HR_11      3
#define ENC_HR_11_CW   4
#define ENC_HR_11_CCW  5
static const uint8_t enc_fsm_half[6][4] =
{
	/*                    00                     01(A)          10(B)          11        */
	/* rest 00   */ { ENC_HR_00,             ENC_HR_00_CW,  ENC_HR_00_CCW, ENC_HR_11              },
	/* 00_CW     */ { ENC_HR_00,             ENC_HR_00_CW,  ENC_HR_00,     ENC_HR_11|ENC_DIR_CW   },
	/* 00_CCW    */ { ENC_HR_00,             ENC_HR_00,     ENC_HR_00_CCW, ENC_HR_11|ENC_DIR_CCW  },
	/* rest 11   */ { ENC_HR_00,             ENC_HR_11_CCW, ENC_HR_11_CW,  ENC_HR_11              },
	/* 11_CW     */ { ENC_HR_00|ENC_DIR_CW,  ENC_HR_11,     ENC_HR_11_CW,  ENC_HR_11              },
	/* 11_CCW    */ { ENC_HR_00|ENC_DIR_CCW, ENC_HR_11_CCW, ENC_HR_11,     ENC_HR_11              },
};

encoder_state_t encoders_state[MAX_ENCODERS_NUM];

/* Live encoder-monitor accumulators for the most-recently-active slow encoder.
 * Surfaced in params_report_t (enc_mon_*) so the configurator's encoder monitor
 * can measure quarter-steps-per-detent (to choose 1x/2x/4x) and flag a bad
 * signal: enc_mon_valid counts decodable single-step transitions; enc_mon_invalid
 * counts un-decodable both-channel jumps (heavy bounce, or a stuck / swapped /
 * non-quadrature channel). Free-running totals -> the configurator reads them as
 * deltas, which is immune to the params-report sample rate. Not persisted. */
static int16_t  enc_mon_net;
static uint16_t enc_mon_valid;
static uint16_t enc_mon_invalid;
static uint8_t  enc_mon_ab;
static uint8_t  enc_mon_slot = 0xFF;

void EncoderGetMonitor(int16_t * net, uint16_t * valid, uint16_t * invalid, uint8_t * ab, uint8_t * slot)
{
	*net     = enc_mon_net;
	*valid   = enc_mon_valid;
	*invalid = enc_mon_invalid;
	*ab      = enc_mon_ab;
	*slot    = enc_mon_slot;
}

// Slot-to-pin mapping for fast (hardware-quadrature) encoders. The pin slot
// indices are part of the wire format (shared across both boards via
// USED_PINS_NUM = 30). The actual timer / clock / AF setup is board-specific
// and lives behind Board_FastEncoderInit / Board_FastEncoderGetCount in
// board/<chip>/Src/board_encoder.c. Adding another fast encoder means adding
// an entry here and bumping MAX_FAST_ENCODER_NUM (and adding a slot to each
// board's hw table).
typedef struct {
	uint8_t pin_a_idx;
	uint8_t pin_b_idx;
} fast_encoder_pins_t;

static const fast_encoder_pins_t fast_encoder_pins[MAX_FAST_ENCODER_NUM] = {
	{  8,  9 },	// Encoder 1: PA8/PA9 (TIM1 on F103/F411)
	{ 17, 18 },	// Encoder 2: PB6/PB7 (TIM4 on F103/F411)
};

/* Per-encoder step-queue runtime state (SLOW_ENC_QUEUE mode). Firmware-local --
 * not part of the wire format. Each detent enqueues one capped pulse; the
 * playout in EncoderProcess plays them out as ON/OFF pulses so N fast detents
 * become N discrete presses instead of one held button. */
#define ENC_QUEUE_CAP 16		// max pending steps -> bounds the post-spin tail
#define ENC_QUEUE_GAP_MS 20		// OFF gap between queued pulses (capped short so a fast spin drains quickly; the ON pulse stays = encoder_press_time_ms for reliable sampling)
static uint8_t enc_pend_a[MAX_ENCODERS_NUM];		// pending CW pulses (fire pin_a)
static uint8_t enc_pend_b[MAX_ENCODERS_NUM];		// pending CCW pulses (fire pin_b)
static int32_t enc_prev_cnt[MAX_ENCODERS_NUM];		// last-seen encoders_state.cnt
static int32_t enc_pulse_end[MAX_ENCODERS_NUM];		// millis the active pulse ends (0 = idle)
static int32_t enc_gap_end[MAX_ENCODERS_NUM];		// millis the OFF gap ends
static uint8_t enc_active[MAX_ENCODERS_NUM];		// 0 none, 1 pin_a pulsing, 2 pin_b pulsing

/* Read a slow-encoder input's CURRENT state. If the input is a direct-wired
 * single button (a BUTTON_GND/VCC pin), read the GPIO live via DirectButtonGet
 * so the encoder samples at the encoder-poll rate instead of waiting for the
 * next full button scan -- this is what lets a fast spin not drop steps. For an
 * input behind a matrix / shift register / GPIO expander / axis-to-buttons
 * (which can't be single-pin read), fall back to the scan buffer.
 *
 * Single buttons occupy raw_buttons_data indices [a2b_last, ...) in scan order,
 * so physical_num >= a2b_last identifies a direct button and (physical_num -
 * a2b_last) is its position within the BUTTON_GND/VCC pins. a2b_last == 0 before
 * the first scan -> fall back (safe). */
static uint8_t EncoderInputRead(dev_config_t * p_dev_config, int8_t btn_slot, uint8_t * is_direct)
{
	if (is_direct) *is_direct = 0;
	int16_t phys = p_dev_config->buttons[btn_slot].physical_num;
	if (phys < 0 || phys >= MAX_BUTTONS_NUM) return 0;		// unmapped -> not pressed

	if (a2b_last != 0 && phys >= a2b_last)
	{
		int16_t k = phys - a2b_last;			// k-th direct single button
		int16_t count = 0;
		for (int i = 0; i < USED_PINS_NUM; i++)
		{
			if (p_dev_config->pins[i] == BUTTON_GND ||
					p_dev_config->pins[i] == BUTTON_VCC)
			{
				if (count == k)
				{
					if (is_direct) *is_direct = 1;		// live 2 kHz GPIO read -- no extra filtering needed
					return DirectButtonGet((uint8_t)i, p_dev_config);
				}
				count++;
			}
		}
	}
	// Not a direct pin (or no scan yet) -- use the last full-scan value.
	return raw_buttons_data[phys];
}

/* Per-encoder, per-channel debounce state for SCAN-sourced inputs (shift
 * register / matrix / GPIO expander). Those channels reach us via
 * raw_buttons_data, which is refreshed only at the button-scan rate and is
 * NOT debounced (buttons.c debounces into a separate buffer the encoder never
 * sees). At slow rotation the contacts dwell in the make/break region and the
 * coarse, unfiltered sampling aliases the bounce into phantom / missed /
 * reversed transitions -- erratic in every 1x/2x/4x mode because the corruption
 * happens before the decode. A short time-based filter (accept a new level only
 * after it holds for ENC_SCAN_DEBOUNCE_MS) cleans the channel before decoding.
 * Direct-GPIO channels bypass this entirely so fast spins keep their 2 kHz read. */
#define ENC_SCAN_DEBOUNCE_MS 4
static uint8_t enc_db_accepted[MAX_ENCODERS_NUM][2];	// [enc][0=A,1=B] currently accepted level
static uint8_t enc_db_cand[MAX_ENCODERS_NUM][2];		// candidate level awaiting stability
static int32_t enc_db_since[MAX_ENCODERS_NUM][2];		// millis the candidate first appeared

// Read channel ch (0=A, 1=B) of encoder i, applying scan-source debounce.
static uint8_t EncoderChannelLevel(dev_config_t * p_dev_config, int i, int ch, int8_t btn_slot, int32_t millis)
{
	uint8_t is_direct = 0;
	uint8_t raw = EncoderInputRead(p_dev_config, btn_slot, &is_direct) ? 1 : 0;

	if (is_direct)
		return raw;			// direct GPIO: already sampled live at the encoder-poll rate

	if (raw != enc_db_accepted[i][ch])
	{
		if (raw != enc_db_cand[i][ch])
		{
			enc_db_cand[i][ch]  = raw;
			enc_db_since[i][ch] = millis;
		}
		else if (millis - enc_db_since[i][ch] >= ENC_SCAN_DEBOUNCE_MS)
		{
			enc_db_accepted[i][ch] = raw;	// level held long enough -> accept it
		}
	}
	else
	{
		enc_db_cand[i][ch] = raw;			// matches accepted -> clear any pending candidate
	}
	return enc_db_accepted[i][ch];
}

/* Fire one decoded detent onto its logical button slot, honouring the button's
 * shift modifier and the per-tick shift-ignore list. Same rules the old inline
 * CW/CCW branches used: with a shift modifier assigned, the button fires only
 * while that shift is held; with no modifier it fires unless the physical input
 * is in the ignore list (an encoder line that another shift layer owns). */
static void EncoderFireButton(logical_buttons_state_t * buf, dev_config_t * cfg,
                              int8_t pin, const int8_t * ignore, uint8_t shifts_state)
{
	uint8_t sm = cfg->buttons[pin].shift_modificator;
	if (sm > 0)
	{
		if (shifts_state & (1 << (sm - 1)))
			buf[pin].current_state = 1;
		return;
	}
	for (int k = 0; k < MAX_ENCODERS_NUM; k++)
	{
		if (cfg->buttons[pin].physical_num == ignore[k])
			return;
	}
	buf[pin].current_state = 1;
}

void EncoderProcess (logical_buttons_state_t * button_state_buf, dev_config_t * p_dev_config)
{	
	
	uint8_t encoders_present = 0;

	// read counters from any configured fast (hardware-quadrature) encoders
	for (uint8_t fi = 0; fi < MAX_FAST_ENCODER_NUM; fi++)
	{
		if (encoders_state[fi].pin_a >= 0 && encoders_state[fi].pin_b >= 0)
		{
			encoders_state[fi].cnt = Board_FastEncoderGetCount(fi);
		}
	}


	// search if there is at least one polling encoder present
	for (int i = MAX_FAST_ENCODER_NUM; i < MAX_ENCODERS_NUM; i++)
	{
		if (encoders_state[i].pin_a >=0 && encoders_state[i].pin_b >=0)
		{
			encoders_present = 1;
			break;
		}
	}
	if (!encoders_present) return;		// dont waste time if no encoders connected
	
	
	int8_t ignore_a[MAX_ENCODERS_NUM]={};
	int8_t ignore_b[MAX_ENCODERS_NUM]={};
	// because physical_num = -1 - no button
	for (int k=0; k<MAX_ENCODERS_NUM; k++)
	{
		ignore_a[k] = -1;
		ignore_b[k] = -1;
	}	
	// search encoder phys number with shift mod enabled
	uint8_t tmp_a = 0;
	uint8_t tmp_b = 0;
	
	int32_t millis = GetMillis();
	
	for (int k = 0; k < MAX_ENCODERS_NUM; k++)
	{
		// Unwired slots hold pin_a/pin_b == -1; guard before indexing buttons[]
		// so we don't read buttons[-1] (out of bounds) for every empty slot.
		// Pin A
		if (encoders_state[k].pin_a >= 0 &&
			p_dev_config->buttons[encoders_state[k].pin_a].shift_modificator > 0 &&
		 shifts_state & 1<<(p_dev_config->buttons[encoders_state[k].pin_a].shift_modificator-1))
		{
			ignore_a[tmp_a++] = p_dev_config->buttons[encoders_state[k].pin_a].physical_num;
		}
		// Pin B
		if (encoders_state[k].pin_b >= 0 &&
			p_dev_config->buttons[encoders_state[k].pin_b].shift_modificator > 0 &&
		 shifts_state & 1<<(p_dev_config->buttons[encoders_state[k].pin_b].shift_modificator-1))
		{
			ignore_b[tmp_b++] = p_dev_config->buttons[encoders_state[k].pin_b].physical_num;
		}
	}
	
	for (int i = MAX_FAST_ENCODER_NUM; i < MAX_ENCODERS_NUM; i++)
	{
		if (encoders_state[i].pin_a >=0 && encoders_state[i].pin_b >=0)
		{
			// Sample the current 2-bit quadrature state. Direct-GPIO channels
			// read live at the encoder-poll rate; scan-sourced channels are
			// debounced first (see EncoderChannelLevel) to reject aliased bounce.
			uint8_t curr = 0;
			if (EncoderChannelLevel(p_dev_config, i, 0, encoders_state[i].pin_a, millis))	curr |= 0x01;	// Pin A
			if (EncoderChannelLevel(p_dev_config, i, 1, encoders_state[i].pin_b, millis))	curr |= 0x02;	// Pin B

			if (encoders_state[i].state == 0xFF)
			{
				// First sample after init / pin change -- prime prev so we don't
				// decode a phantom transition. sub is already 0 (== ENC_R_START for
				// the 1x/2x state machines, empty accumulator otherwise). Mode is
				// fixed for the session (a config change re-runs EncodersInit), so
				// sub is never ambiguous between its two uses.
				encoders_state[i].state = curr;
			}
			else
			{
				const uint8_t prev2 = encoders_state[i].state & 0x03;
				const uint8_t mode  = p_dev_config->encoders[i] & SLOW_ENC_MODE_MASK;
				int8_t  step = 0;
				uint8_t nxt  = 0;

				// Detented modes use a Ben Buxton state machine: drift-free and locked
				// to the detent (emits exactly one step as a click settles to rest).
				// full-step for 1x (1 detent = 1 cycle, rest at 00), half-step for 2x
				// (rests at 00 and 11). Both run every poll and idle at rest when the
				// input is unchanged; sub holds the FSM state.
				if (mode == ENCODER_CONF_1x)
				{
					nxt = enc_fsm_full[(uint8_t)encoders_state[i].sub & 0x0F][curr & 0x03];
					encoders_state[i].sub = (int8_t)(nxt & 0x0F);
				}
				else if (mode == ENCODER_CONF_2x)
				{
					nxt = enc_fsm_half[(uint8_t)encoders_state[i].sub & 0x0F][curr & 0x03];
					encoders_state[i].sub = (int8_t)(nxt & 0x0F);
				}
				if      ((nxt & 0x30) == ENC_DIR_CW)  step =  1;
				else if ((nxt & 0x30) == ENC_DIR_CCW) step = -1;

				if (curr != prev2)		// a real transition happened
				{
					int8_t q = enc_array_4[((prev2 << 2) | curr) & 0x0F];

					// Encoder monitor: classify every transition. q!=0 is a clean
					// single-step; q==0 (state DID change) means both channels moved
					// between samples -- an un-decodable jump flagging heavy bounce or a
					// bad channel.
					enc_mon_slot = (uint8_t)i;
					enc_mon_ab   = curr;
					if (q != 0) { enc_mon_valid++; enc_mon_net += q; }
					else        { enc_mon_invalid++; }

					// 4x = full resolution: emit on every valid transition. No
					// accumulator, so nothing to drift.
					if (mode == ENCODER_CONF_4x && q != 0)
						step = (q > 0) ? 1 : -1;
				}

				encoders_state[i].state = curr;

				if (step != 0)
				{
					if (step > 0)
						EncoderFireButton(button_state_buf, p_dev_config, encoders_state[i].pin_a, ignore_a, shifts_state);	// CW
					else
						EncoderFireButton(button_state_buf, p_dev_config, encoders_state[i].pin_b, ignore_b, shifts_state);	// CCW

					encoders_state[i].time_last = millis;
					encoders_state[i].cnt += step;
					if (encoders_state[i].cnt > AXIS_MAX_VALUE) encoders_state[i].cnt = AXIS_MAX_VALUE;
					if (encoders_state[i].cnt < AXIS_MIN_VALUE) encoders_state[i].cnt = AXIS_MIN_VALUE;
				}
			}
		}
		// unpress encoder button
		if (encoders_state[i].pin_a >=0 && encoders_state[i].pin_b >=0)
		{		
			if (p_dev_config->encoders[i] & SLOW_ENC_QUEUE)
			{
				// QUEUE MODE: emit each detent as a discrete, capped pulse so a
				// fast spin yields N clean presses instead of one held button.
				// New detents this tick == the change in the running count.
				int32_t delta = encoders_state[i].cnt - enc_prev_cnt[i];
				while (delta > 0) { if (enc_pend_a[i] < ENC_QUEUE_CAP) enc_pend_a[i]++; delta--; }
				while (delta < 0) { if (enc_pend_b[i] < ENC_QUEUE_CAP) enc_pend_b[i]++; delta++; }
				enc_prev_cnt[i] = encoders_state[i].cnt;

				// Net out opposite-direction pending steps. If the user reverses,
				// the not-yet-played forward steps annihilate the new backward ones
				// (and vice-versa) instead of both draining in full -- so a
				// spin-then-reverse jumps straight to the other direction rather than
				// playing out a long, self-cancelling sequence. Only the net
				// direction is ever left pending. (Already-played pulses stay
				// accounted for: cnt tracks the true net, so pending == net-not-yet-
				// sent, and the consumer still lands on the correct final value.)
				uint8_t cancel = (enc_pend_a[i] < enc_pend_b[i]) ? enc_pend_a[i] : enc_pend_b[i];
				enc_pend_a[i] -= cancel;
				enc_pend_b[i] -= cancel;

				uint16_t pulse = p_dev_config->encoder_press_time_ms;
				if (pulse == 0) pulse = 1;
				// OFF gap is DECOUPLED from the ON pulse. The pulse must be wide
				// enough for the consumer to sample the press (why ~50 ms reports
				// reliably), but the gap only has to be long enough to register a
				// release between presses, so keeping it short lets a fast spin's
				// queue drain far quicker. User-tunable via encoder_gap_ms; 0 falls
				// back to the ENC_QUEUE_GAP_MS default.
				uint16_t gap = p_dev_config->encoder_gap_ms;
				if (gap == 0) gap = ENC_QUEUE_GAP_MS;

				if (enc_pulse_end[i] != 0)
				{
					// mid-pulse: end it after its width, then start an OFF gap
					if (millis >= enc_pulse_end[i])
					{
						enc_pulse_end[i] = 0;
						enc_active[i] = 0;
						enc_gap_end[i] = millis + gap;		// short OFF gap (decoupled from pulse)
					}
				}
				else if (millis >= enc_gap_end[i])
				{
					// idle + gap elapsed: launch the next queued pulse
					if (enc_pend_a[i] > 0)      { enc_pend_a[i]--; enc_active[i] = 1; enc_pulse_end[i] = millis + pulse; }
					else if (enc_pend_b[i] > 0) { enc_pend_b[i]--; enc_active[i] = 2; enc_pulse_end[i] = millis + pulse; }
				}

				// Own the buttons: on only while their pulse is active (this also
				// overrides the hold the detent block above may have applied).
				button_state_buf[encoders_state[i].pin_a].current_state = (enc_active[i] == 1) ? 1 : 0;
				button_state_buf[encoders_state[i].pin_b].current_state = (enc_active[i] == 2) ? 1 : 0;
			}
			else
			{
			uint16_t a_press_time;
			uint16_t b_press_time;

			// check if press time is redefined
			switch (p_dev_config->buttons[encoders_state[i].pin_a].press_timer)
			{	
					case BUTTON_TIMER_1:
						a_press_time = p_dev_config->button_timer1_ms;
						break;
					case BUTTON_TIMER_2:
						a_press_time = p_dev_config->button_timer2_ms;
						break;
					case BUTTON_TIMER_3:
						a_press_time = p_dev_config->button_timer3_ms;
						break;
					default:
						a_press_time = p_dev_config->encoder_press_time_ms;
						break;
			};
			
			switch (p_dev_config->buttons[encoders_state[i].pin_b].press_timer)
			{	
					case BUTTON_TIMER_1:
						b_press_time = p_dev_config->button_timer1_ms;
						break;
					case BUTTON_TIMER_2:
						b_press_time = p_dev_config->button_timer2_ms;
						break;
					case BUTTON_TIMER_3:
						b_press_time = p_dev_config->button_timer3_ms;
						break;
					default:
						b_press_time = p_dev_config->encoder_press_time_ms;
						break;
			};
					
		
			if (millis - encoders_state[i].time_last > a_press_time)
			{	
				button_state_buf[encoders_state[i].pin_a].current_state = 0;
			}
			if (millis - encoders_state[i].time_last > b_press_time)
			{
				button_state_buf[encoders_state[i].pin_b].current_state = 0;
			}
			}
		}
	}
}

void EncodersInit(dev_config_t * p_dev_config)
{
	for (int i=0; i<MAX_ENCODERS_NUM; i++)
	{
		encoders_state[i].pin_a = -1;
		encoders_state[i].pin_b = -1;
		encoders_state[i].state = 0xFF;		// unprimed -> first sample seeds prev, no phantom step
		encoders_state[i].sub = 0;
		encoders_state[i].time_last = 0;
		// step-queue runtime state
		enc_pend_a[i] = 0;
		enc_pend_b[i] = 0;
		enc_prev_cnt[i] = 0;
		enc_pulse_end[i] = 0;
		enc_gap_end[i] = 0;
		enc_active[i] = 0;
		// scan-source channel debounce state
		enc_db_accepted[i][0] = enc_db_accepted[i][1] = 0;
		enc_db_cand[i][0]     = enc_db_cand[i][1]     = 0;
		enc_db_since[i][0]    = enc_db_since[i][1]    = 0;
	}

	// Bring up each fast encoder whose enabled flag is set in dev_config.
	// Fast encoders occupy encoders_state[0..MAX_FAST_ENCODER_NUM-1]. The
	// configurator UI is responsible for keeping pins[] role assignments in
	// sync with the .enabled flag (it sets pins[pa] = FAST_ENCODER (or
	// FAST_ENCODER_2_A/B for slot 1) atomically with .enabled = 1) so
	// periphery.c configures the GPIOs as alternate-function inputs for the
	// timer.
	for (uint8_t fi = 0; fi < MAX_FAST_ENCODER_NUM; fi++)
	{
		if (p_dev_config->fast_encoders[fi].enabled)
		{
			encoders_state[fi].pin_a = fast_encoder_pins[fi].pin_a_idx;
			encoders_state[fi].pin_b = fast_encoder_pins[fi].pin_b_idx;

			board_encoder_mode_t mode =
				(p_dev_config->fast_encoders[fi].mode == ENCODER_CONF_4x)
					? BOARD_ENCODER_MODE_4X
					: BOARD_ENCODER_MODE_2X;
			Board_FastEncoderInit(fi, mode);
		}
	}
	
	// Slow encoders now carry EXPLICIT pin pairs in dev_config.slow_encoders[]
	// (wire gen 0x0040) -- no more positional zip of ENCODER_INPUT_A/_B button
	// slots. Each entry holds button-slot indices {btn_a, btn_b}; -1 = unwired.
	// Fast slots (0..MAX_FAST_ENCODER_NUM-1) are handled above and left as -1
	// here. The configurator writes the pairs; the legacy migrator synthesises
	// them from the old positional layout so upgraded boards keep their encoders.
	for (int i = MAX_FAST_ENCODER_NUM; i < MAX_ENCODERS_NUM; i++)
	{
		int8_t a = p_dev_config->slow_encoders[i].btn_a;
		int8_t b = p_dev_config->slow_encoders[i].btn_b;
		if (a >= 0 && b >= 0)
		{
			encoders_state[i].pin_a = a;
			encoders_state[i].pin_b = b;
		}
	}
}
