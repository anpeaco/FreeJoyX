/**
  ******************************************************************************
  * @file           : usb_app.c
  * @brief          : Board-agnostic USB report flow + main-tick body.
  *
  * Phase 4D extraction. The per-tick HID transmit and the configurator
  * OUT dispatch used to live in F103-only files (stm32f10x_it.c +
  * usb_endp.c). Moving the chip-agnostic logic here lets both boards
  * run the same code:
  *
  *   - Board_TickISR is overridden by this strong symbol so F411's
  *     weak stub in board/f411_blackpill/Src/board_tick.c falls away
  *     and the F103 stm32f10x_it.c shim now just calls into Board_TickISR
  *     transitively (the IRQ handler in F103's stm32f10x_it.c calls
  *     this function -- it's still board-side glue, but the body
  *     is shared).
  *
  *   - App_HidOutDispatch is invoked when a HID OUT report arrives:
  *       - F103 usb_endp.c::EP2_OUT_Callback shrinks to App_HidOutDispatch(buf)
  *         + the F1-only EP-rearm.
  *       - F411 usbd_freejoy_if.c::FreeJoy_HID_OutEvent calls
  *         App_HidOutDispatch(report_buffer) directly.
  *
  * Two BSP shims absorb the F1-specific bits that used to be inline:
  *
  *   - Board_AdcQuietPeripherals(quiet): F103 toggles RCC clock gates
  *     to silence SPI1/I2C2/TIM3/TIM4 briefly across an ADC sample
  *     window (reduces injected noise on ADC1's analog inputs). F411
  *     no-ops -- different ADC noise considerations + the F411 ADC
  *     runtime path isn't wired yet anyway.
  *
  *   - Board_VersionMismatchBlink(): F103 blinks PB12/PC13 6x at ~3 Hz
  *     when a config write arrives with a stale firmware_version
  *     (stamps the visible failure mode on the BluePill's onboard LED
  *     and a debug pin). F411 stub for now.
  ******************************************************************************
  */

#include <string.h>

#include "common_defines.h"
#include "common_types.h"
#include "build_info.h"
#include "main.h"

#include "periphery.h"
#include "analog.h"
#include "encoders.h"
#include "buttons.h"
#include "config.h"
#include "uart.h"

#include "tle5011.h"
#include "tle5012.h"
#include "mcp320x.h"
#include "mlx90363.h"
#include "mlx90393.h"
#include "as5048a.h"
#include "ads1115.h"
#include "as5600.h"

#include "board_usb.h"
#include "board_misc.h"

/* Tick scheduling cadences -- 1 tick = 500 us at 2 kHz Board_TickInit. */
#define ADC_PERIOD_TICKS              4
#define SENSORS_PERIOD_TICKS          4
#define UART_PERIOD_TICKS             20

/* Per-tick report buffers. Accumulate joystick / params / UART data
 * across one tick window before transmitting. Static so they don't
 * eat 200+ bytes of stack on every tick. */
static joy_report_t       joy_report;
static params_report_t    params_report;
static uart_report_t      uart_report;
static uint8_t            uart_message_code = 0;

volatile int32_t  millis              = 0;
volatile int32_t  joy_millis          = 0;
volatile int32_t  configurator_millis = 0;

#ifdef BOARD_F411_BLACKPILL
/* F411 single-interface: all REPORT_ID INs share EP1 IN, so the
 * App_HidOutDispatch path can't reliably send a response from USB IRQ
 * context (the previous tick's joy report is still in flight). Queue
 * the response in this one-deep buffer and let Board_TickISR drain it
 * on a free tick. */
static uint8_t  pending_in_buf[64];
static uint16_t pending_in_len = 0;
static volatile uint8_t pending_in_active = 0;

/* Diagnostic counters so we can tell which step is firing on each
 * Read-Config attempt. Surface them by toggling PC13 in distinct
 * patterns from Board_TickISR. */
volatile uint16_t diag_out_count    = 0;  /* CONFIG_IN OUT received from host */
volatile uint16_t diag_queue_count  = 0;  /* response queued (first-try BUSY) */
volatile uint16_t diag_drain_ok     = 0;  /* drain succeeded */
volatile uint16_t diag_drain_busy   = 0;  /* drain returned BUSY this tick */

static void App_QueueOrSendInReport(uint8_t report_id, const uint8_t *data, uint16_t length)
{
	/* Try once -- if the EP is idle (rare in IRQ context but possible)
	 * the response goes out immediately and we skip the queue. */
	if (Board_USB_SendReport(report_id, (uint8_t *)data, length) == 0) {
		return;
	}
	/* Otherwise stash for Board_TickISR to drain. Overwrites any
	 * previously-pending response -- the protocol is request/response
	 * sequential so this shouldn't lose anything in practice. */
	if (length > sizeof(pending_in_buf)) length = sizeof(pending_in_buf);
	memcpy(pending_in_buf, data, length);
	pending_in_len    = length;
	pending_in_active = 1;
	diag_queue_count++;
}
#endif
volatile int64_t  encoder_ticks       = 0;
volatile int64_t  adc_ticks           = 0;
volatile int64_t  sensors_ticks       = 1;
volatile int64_t  buttons_ticks       = 0;
volatile int64_t  uart_ticks          = 0;
volatile int      status              = 0;

extern dev_config_t dev_config;

extern volatile uint8_t bootloader;
extern volatile uint8_t system_dfu;
extern external_led_data_t external_led_data;

/* HID OUT report dispatch. Called once per OUT report from either:
 *   - F103 application/Src/usb_endp.c::EP2_OUT_Callback (after F1 EP rearm)
 *   - F411 board/f411_blackpill/Src/usbd_freejoy_if.c::FreeJoy_HID_OutEvent
 *     (after Cube USBD class processes the OUT)
 *
 * Dispatches by report ID (buf[0]) into config-receive, firmware-update
 * trigger, or LED-state update. Body is the F103 EP2_OUT_Callback
 * extracted verbatim minus the F1-specific EP rearm + GPIO blink (the
 * latter goes through Board_VersionMismatchBlink). */
void App_HidOutDispatch(const uint8_t *hid_buf)
{
	static dev_config_t tmp_dev_config;
	/* 16-bit byte-sum of the config snapshot served in the current config-in
	 * transfer (set when fragment 1 is served, returned on a fragment
	 * cfg_count+1 request). Lets the host validate a read in one pass. */
	static uint16_t snapshot_checksum = 0;
	/* tmp_buf MUST be static (was stack-local). F411's OTG-FS in
	 * non-DMA mode latches the buffer pointer in HAL_PCD_EP_Transmit
	 * and reads from it later, when the TXFE IRQ fires to load the
	 * FIFO. By then this function has returned and the stack frame's
	 * been reused -- the late bytes of the IN report came from
	 * whatever ran on the stack after we returned, not from the
	 * memcpy we just did. Static storage gives the buffer permanent
	 * lifetime so the deferred FIFO load reads our actual data. */
	static uint8_t tmp_buf[64];
	uint8_t config_in_cnt;
	uint8_t config_out_cnt;
	uint8_t reportId = hid_buf[0];

#ifdef BOARD_F411_BLACKPILL
	/* Any OUT report received -- light PC13 to confirm OUT path
	 * is alive on F411. diag_out_count stops at first OUT;
	 * pending_in_active still reflects queued config response. */
	diag_out_count++;
#endif

	if (reportId == REPORT_ID_PARAM) {
		configurator_millis = GetMillis() + 30000;
		return;
	} else if (reportId == REPORT_ID_CONFIG_IN ||
	           reportId == REPORT_ID_CONFIG_OUT ||
	           reportId == REPORT_ID_FIRMWARE) {
		/* 2-second pause on joy reports while configurator activity is
		 * in flight -- avoids the per-tick joy report stomping the
		 * config-out IN response on the same EP. */
		uint64_t delay = GetMillis() + 2000;
		joy_millis = delay;
		adc_ticks = buttons_ticks = sensors_ticks = encoder_ticks =
		    delay * TICKS_IN_MILLISECOND;
	}

	uint8_t cfg_count = sizeof(dev_config_t) / 62;
	uint8_t last_cfg_size = sizeof(dev_config_t) % 62;
	if (last_cfg_size > 0) cfg_count++;

	switch (reportId) {
		case REPORT_ID_CONFIG_IN:
			config_in_cnt = hid_buf[1];	/* requested config packet number */

			if ((config_in_cnt > 0) && (config_in_cnt <= cfg_count)) {
				/* Snapshot the whole config from flash ONCE, on the first
				 * fragment of a transfer, and serve every fragment of that
				 * transfer from this stable copy. The host always starts a
				 * read at fragment 1, so the snapshot is in place before any
				 * later fragment is asked for.
				 *
				 * Previously DevConfigGet() re-read all of flash on EVERY
				 * fragment -- 26 full flash reads per transfer, in USB-IRQ
				 * context, while the device's own report stream contends for
				 * the same IN endpoint. Under rapid / immediate reads (e.g. a
				 * configurator that reads the instant it attaches, or fast
				 * multi-device switching) a single transfer could mix bytes
				 * from different moments and ship a stale/partial fragment the
				 * host accepts as valid -- the "loaded the wrong config, re-read
				 * and it was fine" bug. One atomic snapshot makes the whole
				 * transfer self-consistent and cuts the in-IRQ flash work 26x. */
				if (config_in_cnt == 1) {
					DevConfigGet(&tmp_dev_config);
					/* Checksum the exact bytes we're about to serve so the host
					 * can validate its read in a single pass (it fetches this by
					 * requesting fragment cfg_count+1, below). A plain 16-bit
					 * byte-sum -- trivial to reproduce host-side and enough to
					 * catch the partial / stale-fragment corruption. */
					snapshot_checksum = 0;
					for (uint32_t i = 0; i < sizeof(tmp_dev_config); ++i) {
						snapshot_checksum += ((const uint8_t *)&tmp_dev_config)[i];
					}
				}

				memset(tmp_buf, 0, sizeof(tmp_buf));
				tmp_buf[0] = REPORT_ID_CONFIG_IN;
				tmp_buf[1] = config_in_cnt;

				if (config_in_cnt == cfg_count && last_cfg_size > 0) {
					memcpy(&tmp_buf[2],
					       (uint8_t *)&tmp_dev_config + 62 * (config_in_cnt - 1),
					       last_cfg_size);
				} else {
					memcpy(&tmp_buf[2],
					       (uint8_t *)&tmp_dev_config + 62 * (config_in_cnt - 1),
					       62);
				}

#ifdef BOARD_F411_BLACKPILL
				App_QueueOrSendInReport(REPORT_ID_CONFIG_IN, tmp_buf, 64);
#else
				Board_USB_SendReport(REPORT_ID_CONFIG_IN, tmp_buf, 64);
#endif
			} else if (config_in_cnt == cfg_count + 1) {
				/* Checksum request: return the 16-bit byte-sum of the config
				 * snapshot served this transfer, so the host can confirm its
				 * read in ONE pass instead of reading twice and comparing.
				 * snapshot_checksum was set when fragment 1 was served above.
				 * Older firmware never enters this branch -- the host times out
				 * waiting and falls back to the read-twice-and-compare path. */
				memset(tmp_buf, 0, sizeof(tmp_buf));
				tmp_buf[0] = REPORT_ID_CONFIG_IN;
				tmp_buf[1] = config_in_cnt;	/* echo cfg_count+1 so the host matches it */
				tmp_buf[2] = (uint8_t)(snapshot_checksum & 0xFF);
				tmp_buf[3] = (uint8_t)(snapshot_checksum >> 8);

#ifdef BOARD_F411_BLACKPILL
				App_QueueOrSendInReport(REPORT_ID_CONFIG_IN, tmp_buf, 64);
#else
				Board_USB_SendReport(REPORT_ID_CONFIG_IN, tmp_buf, 64);
#endif
			}
			break;

		case REPORT_ID_CONFIG_OUT:
			if (hid_buf[1] == cfg_count && last_cfg_size > 0) {
				memcpy((uint8_t *)&tmp_dev_config + 62 * (hid_buf[1] - 1),
				       &hid_buf[2], last_cfg_size);
			} else if (hid_buf[1] > 0) {
				memcpy((uint8_t *)&tmp_dev_config + 62 * (hid_buf[1] - 1),
				       &hid_buf[2], 62);
			}

			if (hid_buf[1] < cfg_count) {	/* request next packet */
				config_out_cnt = hid_buf[1] + 1;
				tmp_buf[0] = REPORT_ID_CONFIG_OUT;
				tmp_buf[1] = config_out_cnt;
#ifdef BOARD_F411_BLACKPILL
				App_QueueOrSendInReport(REPORT_ID_CONFIG_OUT, tmp_buf, 2);
#else
				Board_USB_SendReport(REPORT_ID_CONFIG_OUT, tmp_buf, 2);
#endif
			} else {
				/* Last packet received. Check version + board_id.
				 * Issue #27: split the two rejection cases so the
				 * configurator can distinguish them. 0xFE = wire-
				 * format generation mismatch (preserves backwards
				 * compat with older configurators). 0xFD = board_id
				 * mismatch with version check otherwise passing.
				 * Older configurators only check for 0xFE and
				 * silently ignore 0xFD -- minor UX regression on
				 * that combination, no functional break. */
				const uint8_t version_mismatch =
					(tmp_dev_config.firmware_version & 0xFFF0) != (FIRMWARE_VERSION & 0xFFF0);
				const uint8_t board_mismatch =
					tmp_dev_config.board_id != BOARD_ID;
				if (version_mismatch || board_mismatch) {
					tmp_buf[0] = REPORT_ID_CONFIG_OUT;
					tmp_buf[1] = version_mismatch ? 0xFE : 0xFD;
#ifdef BOARD_F411_BLACKPILL
					App_QueueOrSendInReport(REPORT_ID_CONFIG_OUT, tmp_buf, 2);
#else
					Board_USB_SendReport(REPORT_ID_CONFIG_OUT, tmp_buf, 2);
#endif
					Board_VersionMismatchBlink();
				} else {
					tmp_dev_config.firmware_version = FIRMWARE_VERSION;
					tmp_dev_config.board_id = BOARD_ID;
					/* If the flash write failed (issue anpeaco/FreeJoyX#3),
					 * skip the system reset and leave the device alive so
					 * the user can retry without it cycling into the
					 * version-mismatch path on the next boot. The
					 * configurator's host-side loop times out waiting for
					 * the post-reset reconnect and the user sees a
					 * stalled Write -- better than a brick + reboot loop. */
					if (DevConfigSet(&tmp_dev_config) == 0) {
						NVIC_SystemReset();
					}
				}
			}
			break;

		case REPORT_ID_FIRMWARE: {
			/* "bootloader run" -> custom HID DFU (both boards).
			 * "system dfu"     -> STM32 factory USB DFU, jumper-free
			 *                     reinstall (F411; no-op on F103). */
			if (strcmp("bootloader run", (const char *)&hid_buf[1]) == 0) {
				bootloader = 1;
			} else if (strcmp("system dfu", (const char *)&hid_buf[1]) == 0) {
				system_dfu = 1;
			}
			break;
		}

		case REPORT_ID_LED:
			memcpy(&external_led_data, &hid_buf[1], sizeof(external_led_data_t));
			break;

		default:
			break;
	}
}

/* Per-tick body. Runs at 2 kHz from the board's TIM2 IRQ handler
 * (board_tick.c). */
void Board_TickISR(void)
{
	/* report_buf MUST be static (was stack-local). F411's OTG-FS in
	 * non-DMA mode reads from this buffer asynchronously when TXFE
	 * IRQ fires to load the FIFO -- by which time Board_TickISR has
	 * returned and the stack frame is gone. Static storage keeps the
	 * data alive across the deferred FIFO load. Same fix as the one
	 * in App_HidOutDispatch. Tick is single-instance (no reentrancy
	 * for same-priority IRQ) so static is safe here. */
	static uint8_t report_buf[64];
	uint8_t       pos = 0;
	app_config_t  tmp_app_config;

	Ticks++;
	millis = GetMillis();

	AppConfigGet(&tmp_app_config);

#ifdef BOARD_F411_BLACKPILL
	/* Drain a deferred config-response fragment queued by App_HidOutDispatch
	 * when EP2 IN was busy at OUT-IRQ time (F411's OTG-FS Transmit is async, so
	 * SendCfgReport can return BUSY mid-transfer -- unlike F103's synchronous PMA
	 * write, hence the queue). One fragment per tick. No early return: joystick
	 * (EP1) and configurator (EP2) are independent endpoints since the Phase-4F
	 * split, so draining a fragment on EP2 doesn't contend with the joy send on
	 * EP1 -- suppressing joy/params for the whole transfer was a single-EP-era
	 * leftover that needlessly stalled the joystick during config reads/writes. */
	if (pending_in_active) {
		if (Board_USB_SendReport(0, pending_in_buf, pending_in_len) == 0) {
			pending_in_active = 0;
			diag_drain_ok++;
		} else {
			diag_drain_busy++;
		}
	}

#ifdef FREEJOY_F411_USB_DIAG
	/* Bring-up diagnostic, OFF by default (build with -DFREEJOY_F411_USB_DIAG).
	 * Kept out of production because PC13 is a user-assignable pin (slot 27) and
	 * this rewrites its MODER/ODR every tick, fighting any user mapping. PC13:
	 *   Solid ON  -> some OUT report received since boot (diag_out_count > 0)
	 *   Blinking  -> pending_in_active currently set (queue not draining)
	 *   ~1 Hz heartbeat otherwise -- proves TIM2 ISR running. */
	{
		GPIOC->MODER &= ~GPIO_MODER_MODER13;
		GPIOC->MODER |= GPIO_MODER_MODER13_0;
		if (diag_out_count == 0) {
			/* No OUT yet -- 1 Hz heartbeat from TIM2. */
			static uint32_t hb_tick = 0;
			if (++hb_tick >= 1000) {
				hb_tick = 0;
				GPIOC->ODR ^= (1U << 13);
			}
		} else {
			/* OUT received: pin LED on. If we ever queued a
			 * response that's still pending, it stays on
			 * indefinitely (drain failing). If drain_ok keeps
			 * up, the LED briefly blips off. */
			if (pending_in_active) {
				GPIOC->BSRR = GPIO_BSRR_BR_13;  /* on (active low) */
			} else {
				GPIOC->BSRR = GPIO_BSRR_BS_13;  /* off */
			}
		}
	}
#endif /* FREEJOY_F411_USB_DIAG */
#endif

	/* Joystick + params transmit on the configurator-defined cadence. */
	if (millis - joy_millis >= dev_config.exchange_period_ms) {
		joy_millis = millis;

		ButtonsGet(joy_report.button_data,
		           params_report.log_button_data,
		           params_report.phy_button_data,
		           &params_report.shift_button_data);
		AnalogGet(joy_report.axis_data, NULL, params_report.raw_axis_data);
		AnalogGetDetect(params_report.detect_axis_raw);
		POVsGet(joy_report.pov_data);

		report_buf[pos++] = REPORT_ID_JOY;
		if (tmp_app_config.buttons_cnt > 0) {
			memcpy(&report_buf[pos], joy_report.button_data, MAX_BUTTONS_NUM/8);
			pos += (tmp_app_config.buttons_cnt - 1) / 8 + 1;
		}
		for (uint8_t i = 0; i < MAX_AXIS_NUM; i++) {
			if (tmp_app_config.axis & (1 << i)) {
				report_buf[pos++] = (uint8_t)(joy_report.axis_data[i] & 0xFF);
				report_buf[pos++] = (uint8_t)(joy_report.axis_data[i] >> 8);
			}
		}
		for (uint8_t i = 0; i < MAX_POVS_NUM; i++) {
			if (tmp_app_config.pov & (1 << i)) {
				report_buf[pos++] = joy_report.pov_data[i];
			}
		}

		/* Joystick on REPORT_ID_JOY, every qualifying tick.
		 *
		 * F103 sends to its dedicated joystick HID interface (EP1 IN of
		 * the multi-interface composite). F411 with Phase 4F is now
		 * also dual-HID composite: joystick to EP1 IN, configurator
		 * (params/config_in/config_out/firmware/led) to EP2 IN/OUT --
		 * see board/f411_blackpill/Src/usbd_freejoy_class.c. Joy and
		 * params no longer share an endpoint, so the F411-specific
		 * per-tick alternation that pre-Phase-4F shipped is no longer
		 * needed; both boards run the same path here. */
		Board_USB_SendReport(REPORT_ID_JOY, report_buf, pos);

		/* Params report -- two halves chunked into 62-byte payloads
		 * because params_report_t is larger than one HID report. Only
		 * sent during active configurator sessions (configurator pings
		 * extend configurator_millis by 30 s in App_HidOutDispatch). */
		if (configurator_millis > millis) {
			static uint8_t report = 0;
			report_buf[0] = REPORT_ID_PARAM;
			params_report.firmware_version = FIRMWARE_VERSION;
			params_report.board_id = BOARD_ID;
			/* Repurposes the alignment-pad byte as a wraparound-counter
			 * build ID so the configurator's sidebar can show which
			 * firmware bin is actually flashed. Auto-incremented by the
			 * armgcc Makefile via build_info.h; 0..255 wrap. Wire-format
			 * compatible -- same offset, same width as before. */
			params_report.reserved_layout = (uint8_t)(FIRMWARE_BUILD_ID & 0xFF);
			/* Surface the project semver so the configurator's sidebar
			 * can show "Version: X.Y.Z" alongside the wire-format token.
			 * Issue anpeaco/FreeJoyX#18 follow-on. */
			params_report.freejoyx_version_major = FREEJOYX_VERSION_MAJOR;
			params_report.freejoyx_version_minor = FREEJOYX_VERSION_MINOR;
			params_report.freejoyx_version_patch = FREEJOYX_VERSION_PATCH;
			memcpy(params_report.axis_data, joy_report.axis_data,
			       sizeof(params_report.axis_data));

			if (report == 0) {
				report_buf[1] = 0;
				memcpy(&report_buf[2], (uint8_t *)&params_report, 62);
			} else {
				report_buf[1] = 1;
				memcpy(&report_buf[2], (uint8_t *)&params_report + 62,
				       sizeof(params_report_t) - 62);
			}

			if (Board_USB_SendReport(REPORT_ID_PARAM, report_buf, 64) == 0) {
				report = !report;
			}
		}
	}

	/* UART telemetry (simhub) on its own cadence. */
	if (tmp_app_config.uart_tx_used && Ticks - uart_ticks >= UART_PERIOD_TICKS) {
		uart_ticks = Ticks;

		AnalogGet(uart_report.axis_data, NULL, NULL);

		uart_report.header       = 'H';
		uart_report.separator    = '-';
		uart_report.message_code = uart_message_code;

		ButtonsGet(joy_report.button_data, NULL, NULL, NULL);
		memcpy((uint8_t *)&uart_report.buttons_data, joy_report.button_data,
		       MAX_BUTTONS_NUM / 8);

		uart_report.crc = gen_crc16((uint8_t *)&uart_report, sizeof(uart_report) - 2);

		UART_WriteNonBlocking((uint8_t *)&uart_report, sizeof(uart_report));

		uart_message_code++;
	}

	/* Digital input polling. */
	if (Ticks - buttons_ticks >= dev_config.button_polling_interval_ticks) {
		buttons_ticks = Ticks;
		ButtonsReadPhysical(&dev_config, raw_buttons_data);
	}
	if (Ticks - encoder_ticks >= dev_config.encoder_polling_interval_ticks) {
		encoder_ticks = Ticks;
		EncoderProcess(logical_buttons_state, &dev_config);
	}

	/* Internal ADC conversion. F103 silences neighbouring peripherals
	 * across the ADC sample window via Board_AdcQuietPeripherals (RCC
	 * clock-gate toggles for SPI1/I2C2/TIM3/TIM4). F411 no-ops the
	 * shim until the F411 ADC runtime path lands. */
	if (Ticks - adc_ticks >= ADC_PERIOD_TICKS) {
		adc_ticks = Ticks;

		AxesProcess(&dev_config);

		Board_AdcQuietPeripherals(1, &tmp_app_config);
		ADC_Conversion();
		Board_AdcQuietPeripherals(0, &tmp_app_config);
	}

	/* External sensor DMA kickoff. Note the +1 guard: avoids overlapping
	 * an ADC conversion with a SPI/I2C DMA start in the same tick. */
	if (Ticks - sensors_ticks >= SENSORS_PERIOD_TICKS && Ticks > adc_ticks + 1) {
		sensors_ticks = Ticks;

		/* SPI sensors (single-channel-at-a-time -- they share DMA
		 * channels and the bus). */
		for (uint8_t i = 0; i < MAX_AXIS_NUM; i++) {
			if (sensors[i].source >= 0 && sensors[i].tx_complete && sensors[i].rx_complete) {
				if (sensors[i].type == TLE5011) {
					TLE5011_StartDMA(&sensors[i]); break;
				} else if (sensors[i].type == TLE5012) {
					TLE5012_StartDMA(&sensors[i]); break;
				} else if (sensors[i].type == MCP3201 ||
				           sensors[i].type == MCP3202 ||
				           sensors[i].type == MCP3204 ||
				           sensors[i].type == MCP3208) {
					MCP320x_StartDMA(&sensors[i], 0); break;
				} else if (sensors[i].type == MLX90363) {
					MLX90363_StartDMA(&sensors[i]); break;
				} else if (sensors[i].type == MLX90393_SPI) {
					MLX90393_StartDMA(MLX_SPI, &sensors[i]); break;
				} else if (sensors[i].type == AS5048A_SPI) {
					AS5048A_StartDMA(&sensors[i]); break;
				}
			}
		}

		/* I2C sensors. */
		for (uint8_t i = 0; i < MAX_AXIS_NUM; i++) {
			if (sensors[i].source == (pin_t)SOURCE_I2C &&
			    sensors[i].rx_complete && sensors[i].tx_complete) {
				if (sensors[i].type == AS5600) {
					status = AS5600_StartDMA(&sensors[i]);
					if (status != 0) continue;
					else break;
				} else if (sensors[i].type == ADS1115) {
					status = ADS1115_StartDMA(&sensors[i], sensors[i].curr_channel);
					if (status != 0) continue;
					else break;
				}
			}
		}
	}
}
