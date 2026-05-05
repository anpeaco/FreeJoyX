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

/* Phase 4D: OutEvent now calls App_HidOutDispatch directly (in
 * application/Src/usb_app.c) instead of the indirect EP1_OUT_Callback +
 * out_buffer global the Phase 4 stubs used. */
extern void App_HidOutDispatch(const uint8_t *hid_buf);

/* HID report descriptor. Layout matches the configurator's parsing
 * (FreeJoyConfiguratorQtX/src/reportconverter.cpp). MAX_BUTTONS_NUM=128
 * and MAX_AXIS_NUM=8 from common_defines.h drive the joystick report
 * sizing. */
__ALIGN_BEGIN static uint8_t FreeJoy_ReportDesc[] __ALIGN_END = {
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

/* Pin USBD_CUSTOM_HID_REPORT_DESC_SIZE to the actual array size --
 * any drift produces trailing-zero padding that Windows rejects with
 * Code 10. If this fires, update USBD_CUSTOM_HID_REPORT_DESC_SIZE in
 * board/f411_blackpill/Inc/usbd_conf.h to the value the compiler
 * complains about. */
_Static_assert(sizeof(FreeJoy_ReportDesc) == USBD_CUSTOM_HID_REPORT_DESC_SIZE,
               "FreeJoy_ReportDesc size != USBD_CUSTOM_HID_REPORT_DESC_SIZE");

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
 * payload. Dispatch into the board-agnostic App_HidOutDispatch which
 * handles config receive, firmware-update trigger, and LED-state
 * updates by report ID.
 *
 * Critical: re-arm EP1 OUT for the next report. Cube's CustomHID class
 * library naks all subsequent OUTs after the first if the user code
 * doesn't call USBD_CUSTOM_HID_ReceivePacket from OutEvent. Without
 * this, only the first PARAM ping ever reaches the device and every
 * CONFIG_IN/CONFIG_OUT request silently fails.  */
extern USBD_HandleTypeDef hUsbDeviceFS;  /* board/f411_blackpill/Src/board_usb.c */

static int8_t FreeJoy_HID_OutEvent(uint8_t *report_buffer)
{
	App_HidOutDispatch(report_buffer);
	USBD_CUSTOM_HID_ReceivePacket(&hUsbDeviceFS);
	return USBD_OK;
}

USBD_CUSTOM_HID_ItfTypeDef FreeJoy_HID_fops = {
	FreeJoy_ReportDesc,
	FreeJoy_HID_Init,
	FreeJoy_HID_DeInit,
	FreeJoy_HID_OutEvent,
};
