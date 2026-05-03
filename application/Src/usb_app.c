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
volatile int64_t  encoder_ticks       = 0;
volatile int64_t  adc_ticks           = 0;
volatile int64_t  sensors_ticks       = 1;
volatile int64_t  buttons_ticks       = 0;
volatile int64_t  uart_ticks          = 0;
volatile int      status              = 0;

extern dev_config_t dev_config;

extern volatile uint8_t bootloader;
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
	uint8_t tmp_buf[64];
	uint8_t config_in_cnt;
	uint8_t config_out_cnt;
	uint8_t reportId = hid_buf[0];

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
				DevConfigGet(&tmp_dev_config);

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

				Board_USB_SendReport(REPORT_ID_CONFIG_IN, tmp_buf, 64);
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
				Board_USB_SendReport(REPORT_ID_CONFIG_OUT, tmp_buf, 2);
			} else {
				/* Last packet received. Check version + board_id. */
				if (((tmp_dev_config.firmware_version & 0xFFF0) != (FIRMWARE_VERSION & 0xFFF0)) ||
				    (tmp_dev_config.board_id != BOARD_ID)) {
					tmp_buf[0] = REPORT_ID_CONFIG_OUT;
					tmp_buf[1] = 0xFE;
					Board_USB_SendReport(REPORT_ID_CONFIG_OUT, tmp_buf, 2);
					Board_VersionMismatchBlink();
				} else {
					tmp_dev_config.firmware_version = FIRMWARE_VERSION;
					tmp_dev_config.board_id = BOARD_ID;
					DevConfigSet(&tmp_dev_config);
					NVIC_SystemReset();
				}
			}
			break;

		case REPORT_ID_FIRMWARE: {
			const char tmp_str[] = "bootloader run";
			if (strcmp(tmp_str, (const char *)&hid_buf[1]) == 0) {
				bootloader = 1;
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
	uint8_t       report_buf[64];
	uint8_t       pos = 0;
	app_config_t  tmp_app_config;

	Ticks++;
	millis = GetMillis();

	AppConfigGet(&tmp_app_config);

	/* Joystick + params transmit on the configurator-defined cadence. */
	if (millis - joy_millis >= dev_config.exchange_period_ms) {
		joy_millis = millis;

		ButtonsGet(joy_report.button_data,
		           params_report.log_button_data,
		           params_report.phy_button_data,
		           &params_report.shift_button_data);
		AnalogGet(joy_report.axis_data, NULL, params_report.raw_axis_data);
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

		Board_USB_SendReport(REPORT_ID_JOY, report_buf, pos);

		/* Params report -- two halves chunked into 62-byte payloads
		 * because params_report_t is larger than one HID report. */
		if (configurator_millis > millis) {
			static uint8_t report = 0;
			report_buf[0] = REPORT_ID_PARAM;
			params_report.firmware_version = FIRMWARE_VERSION;
			params_report.board_id = BOARD_ID;
			params_report.reserved_layout = 0;
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
