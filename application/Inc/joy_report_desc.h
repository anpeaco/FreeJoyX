/**
  ******************************************************************************
  * @file           : joy_report_desc.h
  * @brief          : Board-agnostic builder for the joystick HID report
  *                   descriptor (REPORT_ID_JOY, interface 0).
  *
  * Both boards' per-tick HID transmit in usb_app.c packs a variable-length
  * payload (only enabled buttons / axes / POVs). The host parses the payload
  * against the report descriptor it fetched at enumeration -- so the
  * descriptor must match the payload exactly, otherwise Windows ignores
  * the short reports and joy.cpl shows no movement. This helper builds
  * the matching descriptor from the current app_config_t.
  *
  * Capacity: 86 bytes (matches F103's historical JoystickHID_SIZ_REPORT_DESC
  * and F411's FREEJOY_JOY_REPORT_DESC_SIZE). The full-shape descriptor with
  * 128 buttons + 8 axes + 4 POVs fits.
  ******************************************************************************
  */
#ifndef __JOY_REPORT_DESC_H__
#define __JOY_REPORT_DESC_H__

#include <stdint.h>
#include "common_types.h"

#define JOY_REPORT_DESC_MAX_SIZE   86U

/* Build the joystick HID report descriptor into the caller-provided buffer
 * (must be >= JOY_REPORT_DESC_MAX_SIZE) using the enabled-feature counts
 * from `cfg`. Returns the actual descriptor size in bytes. */
uint16_t BuildJoyReportDesc(uint8_t *buf, const app_config_t *cfg);

#endif /* __JOY_REPORT_DESC_H__ */
