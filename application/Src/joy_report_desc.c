/**
  ******************************************************************************
  * @file           : joy_report_desc.c
  * @brief          : Board-agnostic builder for the joystick HID report
  *                   descriptor. See joy_report_desc.h for the contract.
  *
  * Logic ported verbatim from F103's Get_ReportDesc (application/Src/usb_hw.c)
  * so both boards declare the same dynamic shape:
  *
  *   - buttons section sized to app_config.buttons_cnt
  *   - axis section sized to app_config.axis_cnt, with the enabled axes
  *     pinned to their fixed USAGE order (X, Y, Z, Rx, Ry, Rz, Slider×2)
  *   - POV section sized to app_config.pov_cnt
  *
  * When no features are configured, emits a dummy 8-button collection so
  * the descriptor is still valid (Windows rejects empty collections).
  ******************************************************************************
  */
#include "joy_report_desc.h"
#include "common_defines.h"
#include "config.h"

uint16_t BuildJoyReportDesc(uint8_t *buf, const app_config_t *cfg)
{
	uint16_t i = 0;

	/* Header */
	buf[i++] = 0x05; buf[i++] = 0x01;            /* USAGE_PAGE (Generic Desktop) */
	buf[i++] = 0x09; buf[i++] = 0x04;            /* USAGE (Joystick) */
	buf[i++] = 0xA1; buf[i++] = 0x01;            /* COLLECTION (Application) */
	buf[i++] = 0x85; buf[i++] = REPORT_ID_JOY;   /* REPORT_ID */

	/* IsAppConfigEmpty mirrors F103's special-case branch -- a config with
	 * no enabled features still needs a non-empty collection to be a valid
	 * HID descriptor, so we emit a placeholder 8-button input. The matching
	 * payload from usb_app.c is empty (no buttons/axes/POVs sent), but the
	 * dummy buttons read as 0 on the host and joy.cpl shows a 0-axis
	 * 8-button controller -- consistent with F103. */
	if (cfg->buttons_cnt == 0 && cfg->axis_cnt == 0 && cfg->pov_cnt == 0) {
		buf[i++] = 0x05; buf[i++] = 0x09;        /* USAGE_PAGE (Button) */
		buf[i++] = 0x19; buf[i++] = 0x01;        /* USAGE_MINIMUM (Button 1) */
		buf[i++] = 0x29; buf[i++] = 0x01;        /* USAGE_MAXIMUM (Button 1) -- matches F103 */
		buf[i++] = 0x15; buf[i++] = 0x00;        /* LOGICAL_MINIMUM (0) */
		buf[i++] = 0x25; buf[i++] = 0x01;        /* LOGICAL_MAXIMUM (1) */
		buf[i++] = 0x75; buf[i++] = 0x01;        /* REPORT_SIZE (1) */
		buf[i++] = 0x95; buf[i++] = 0x08;        /* REPORT_COUNT (8) -- matches F103 */
		buf[i++] = 0x81; buf[i++] = 0x02;        /* INPUT (Data,Var,Abs) */
	} else {
		/* Buttons */
		if (cfg->buttons_cnt > 0) {
			buf[i++] = 0x05; buf[i++] = 0x09;
			buf[i++] = 0x19; buf[i++] = 0x01;
			buf[i++] = 0x29; buf[i++] = cfg->buttons_cnt;
			buf[i++] = 0x15; buf[i++] = 0x00;
			buf[i++] = 0x25; buf[i++] = 0x01;
			buf[i++] = 0x75; buf[i++] = 0x01;
			buf[i++] = 0x95;
			buf[i++] = (uint8_t)(((cfg->buttons_cnt - 1) / 8 + 1) * 8);
			buf[i++] = 0x81; buf[i++] = 0x02;
		}

		/* Axes */
		if (cfg->axis_cnt > 0) {
			buf[i++] = 0x05; buf[i++] = 0x01;
			for (uint8_t axis = 0; axis < 6; axis++) {
				if (cfg->axis & (1U << axis)) {
					buf[i++] = 0x09;
					buf[i++] = (uint8_t)(0x30 + axis);
				}
			}
			for (uint8_t axis = 6; axis < 8; axis++) {
				if (cfg->axis & (1U << axis)) {
					buf[i++] = 0x09;
					buf[i++] = 0x36;                 /* Slider */
				}
			}
			buf[i++] = 0x16; buf[i++] = 0x01; buf[i++] = 0x80;   /* LOG_MIN -32767 */
			buf[i++] = 0x26; buf[i++] = 0xFF; buf[i++] = 0x7F;   /* LOG_MAX  32767 */
			buf[i++] = 0x75; buf[i++] = 0x10;                    /* REPORT_SIZE 16 */
			buf[i++] = 0x95; buf[i++] = cfg->axis_cnt;
			buf[i++] = 0x81; buf[i++] = 0x02;
		}

		/* POVs */
		if (cfg->pov_cnt > 0) {
			buf[i++] = 0x09; buf[i++] = 0x39;
			buf[i++] = 0x15; buf[i++] = 0x00;
			buf[i++] = 0x25; buf[i++] = 0x07;
			buf[i++] = 0x35; buf[i++] = 0x00;
			buf[i++] = 0x46; buf[i++] = 0x3B; buf[i++] = 0x01;
			buf[i++] = 0x65; buf[i++] = 0x12;
			buf[i++] = 0x75; buf[i++] = 0x08;
			buf[i++] = 0x95; buf[i++] = 0x01;
			buf[i++] = 0x81; buf[i++] = 0x02;
			for (uint8_t j = 1; j < cfg->pov_cnt; j++) {
				buf[i++] = 0x09; buf[i++] = 0x39;
				buf[i++] = 0x81; buf[i++] = 0x02;
			}
		}
	}

	buf[i++] = 0xC0;                             /* END_COLLECTION */
	return i;
}
