/**
  ******************************************************************************
  * @file           : usbd_freejoy_if.c
  * @brief          : F411 CustomHID interface (report descriptor + OutEvent dispatch).
  *
  * Provides the USBD_CUSTOM_HID_ItfTypeDef the CubeF4 USBD_CUSTOM_HID
  * class calls during enumeration (pReport pointer + Init/DeInit) and
  * on every HID OUT report (OutEvent with the 64-byte buffer).
  *
  * The report descriptor merges F103's separate JoystickHID + CustomHID
  * report descriptors into a single Vendor-Defined collection holding
  * all 7 report IDs (JOY=1, PARAM=2, CONFIG_IN=3, CONFIG_OUT=4,
  * FIRMWARE=5, LED=6 -- matches application/Inc/common_defines.h
  * REPORT_ID_* enum). Single-interface device avoids the F103 composite
  * (Joystick HID + Custom HID + CDC) which is overkill for the
  * configurator's wire-format protocol -- the host doesn't care which
  * interface a report arrives on once it's parsed by report ID.
  *
  * OutEvent dispatches by report ID (byte[0]) into the existing F103
  * EP1_OUT_Callback handlers in application/Src/usb_endp.c. F411 calls
  * those same functions; the report-routing logic stays board-agnostic.
  ******************************************************************************
  */

#include "usbd_customhid.h"
#include "common_defines.h"
#include <stdint.h>

/* Forward declarations for the application-layer config-out / firmware /
 * LED handlers. Defined in application/Src/usb_endp.c on F103 today;
 * Phase 4 step 7 pulls usb_endp.c into the F411 build with CDC-related
 * paths gated. Until then these resolve via the link-time stubs in
 * board_phase_stubs.c. The dispatch below is intentionally written
 * against the application-layer surface, not the F103 EP slot names. */
extern void EP1_OUT_Callback(void);
extern uint8_t out_buffer[];   /* 64-byte HID OUT scratch in usb_endp.c */

/* HID report descriptor. Layout matches the configurator's parsing
 * (FreeJoyConfiguratorQtX/src/reportconverter.cpp). MAX_BUTTONS_NUM=128
 * and MAX_AXIS_NUM=8 from common_defines.h drive the joystick report
 * sizing. */
__ALIGN_BEGIN static uint8_t FreeJoy_ReportDesc[USBD_CUSTOM_HID_REPORT_DESC_SIZE] __ALIGN_END = {
	/* === Joystick collection (REPORT_ID 1 = JOY) === */
	0x05, 0x01,                     /* USAGE_PAGE (Generic Desktop) */
	0x09, 0x04,                     /* USAGE (Joystick) */
	0xA1, 0x01,                     /* COLLECTION (Application) */

	0x85, REPORT_ID_JOY,            /*   REPORT_ID (1) */
	0x05, 0x09,                     /*   USAGE_PAGE (Button) */
	0x19, 0x01,                     /*   USAGE_MINIMUM (Button 1) */
	0x29, MAX_BUTTONS_NUM,          /*   USAGE_MAXIMUM (Button 128) */
	0x15, 0x00,                     /*   LOGICAL_MINIMUM (0) */
	0x25, 0x01,                     /*   LOGICAL_MAXIMUM (1) */
	0x75, 0x01,                     /*   REPORT_SIZE (1) */
	0x95, MAX_BUTTONS_NUM,          /*   REPORT_COUNT (128) */
	0x81, 0x00,                     /*   INPUT (Data,Ary,Abs) */

	0x05, 0x01,                     /*   USAGE_PAGE (Generic Desktop) */
	0x09, 0x30,                     /*   USAGE (X) */
	0x09, 0x31,                     /*   USAGE (Y) */
	0x09, 0x32,                     /*   USAGE (Z) */
	0x09, 0x33,                     /*   USAGE (Rx) */
	0x09, 0x34,                     /*   USAGE (Ry) */
	0x09, 0x35,                     /*   USAGE (Rz) */
	0x09, 0x36,                     /*   USAGE (Slider) */
	0x09, 0x36,                     /*   USAGE (Slider) */
	0x16, 0x01, 0x80,               /*   LOGICAL_MINIMUM (-32767) */
	0x26, 0xFF, 0x7F,               /*   LOGICAL_MAXIMUM (32767) */
	0x75, 0x10,                     /*   REPORT_SIZE (16) */
	0x95, MAX_AXIS_NUM,             /*   REPORT_COUNT (8) */
	0x81, 0x02,                     /*   INPUT (Data,Var,Abs) */

	0x09, 0x39,                     /*   USAGE (Hat switch) */
	0x15, 0x00,                     /*   LOGICAL_MINIMUM (0) */
	0x25, 0x07,                     /*   LOGICAL_MAXIMUM (7) */
	0x35, 0x00,                     /*   PHYSICAL_MINIMUM (0) */
	0x46, 0x3B, 0x01,               /*   PHYSICAL_MAXIMUM (315) */
	0x65, 0x12,                     /*   UNIT (SI Rot:Angular Pos) */
	0x75, 0x08,                     /*   REPORT_SIZE (8) */
	0x95, 0x01,                     /*   REPORT_COUNT (1) */
	0x81, 0x02,                     /*   INPUT */
	0x09, 0x39, 0x81, 0x02,         /*   USAGE (Hat switch) + INPUT */
	0x09, 0x39, 0x81, 0x02,
	0x09, 0x39, 0x81, 0x02,

	/* === Vendor-defined configurator reports (PARAM/CONFIG_IN/OUT/FIRMWARE/LED) === */
	0x06, 0x00, 0xFF,               /*   USAGE_PAGE (Vendor Defined 1) */

	0x85, REPORT_ID_PARAM,          /*   REPORT_ID (2) */
	0x09, 0x02,                     /*   USAGE (Vendor 2) */
	0x15, 0x00, 0x26, 0xFF, 0x00,   /*   LOGICAL_MIN/MAX 0..255 */
	0x75, 0x08, 0x95, 0x3F,         /*   REPORT_SIZE 8 / COUNT 63 */
	0x81, 0x00,                     /*   INPUT */
	0x09, 0x03, 0x75, 0x08, 0x95, 0x01, 0x91, 0x00, /*   OUTPUT 1 byte */

	0x85, REPORT_ID_CONFIG_IN,      /*   REPORT_ID (3) */
	0x09, 0x04, 0x15, 0x00, 0x26, 0xFF, 0x00,
	0x75, 0x08, 0x95, 0x3F, 0x81, 0x00,
	0x09, 0x05, 0x75, 0x08, 0x95, 0x01, 0x91, 0x00,

	0x85, REPORT_ID_CONFIG_OUT,     /*   REPORT_ID (4) */
	0x09, 0x06, 0x75, 0x08, 0x95, 0x01, 0x81, 0x00,
	0x09, 0x07, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x00,

	0x85, REPORT_ID_FIRMWARE,       /*   REPORT_ID (5) */
	0x09, 0x08, 0x75, 0x08, 0x95, 0x02, 0x81, 0x00,
	0x09, 0x09, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x00,

	0x85, REPORT_ID_LED,            /*   REPORT_ID (6) */
	0x09, 0x0A, 0x75, 0x08, 0x95, 0x04, 0x91, 0x00,

	0xC0,                           /* END_COLLECTION */
};

/* Initialize anything class-specific. Called by USBD_CUSTOM_HID class
 * once per USBD_Init -> USBD_Start cycle. F103 used this to set the
 * initial protocol; on F411 the class core handles that, and the
 * dispatch into application code happens later via EP1_OUT_Callback. */
static int8_t FreeJoy_HID_Init(void)
{
	return USBD_OK;
}

static int8_t FreeJoy_HID_DeInit(void)
{
	return USBD_OK;
}

/* Called when host sends a HID OUT report on EP1 OUT. report_buffer is
 * a 64-byte array; report_buffer[0] is the report ID, [1..62] is the
 * payload. The application's existing usb_endp.c::EP1_OUT_Callback
 * reads from a global out_buffer[] populated by the F103 USB stack
 * before the callback fires. Phase 4 mirrors that: copy the buffer to
 * the same global, then invoke the callback. usb_endp.c becomes
 * board-agnostic. */
static int8_t FreeJoy_HID_OutEvent(uint8_t *report_buffer)
{
	memcpy(out_buffer, report_buffer, USBD_CUSTOMHID_OUTREPORT_BUF_SIZE);
	EP1_OUT_Callback();
	return USBD_OK;
}

USBD_CUSTOM_HID_ItfTypeDef FreeJoy_HID_fops = {
	FreeJoy_ReportDesc,
	FreeJoy_HID_Init,
	FreeJoy_HID_DeInit,
	FreeJoy_HID_OutEvent,
};
