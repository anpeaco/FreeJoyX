/**
  ******************************************************************************
  * @file           : usbd_freejoy_if.c
  * @brief          : F411 dual-HID composite report descriptors + OutEvent.
  *
  * Phase 4F splits the single-interface CustomHID report descriptor that
  * shipped with F411's Phase 4 into two report descriptors, matching
  * F103's two-HID layout exactly (application/Src/usb_desc.c
  * `JoystickHID_ReportDescriptor` and `CustomHID_ReportDescriptor`):
  *
  *   - Joystick HID  (REPORT_ID 1)         on interface 0, EP1 IN
  *   - Configurator HID (REPORT_IDs 2..6)  on interface 1, EP2 IN/OUT
  *
  * The composite class wrapper in `usbd_freejoy_class.c` owns the
  * combined config descriptor + endpoint setup + Setup/DataIn/DataOut
  * dispatch by interface number. This file contains only the per-
  * interface report descriptors and the configurator's OutEvent shim
  * into the board-agnostic application dispatch.
  *
  * Why dual-HID: Windows binds usbccgp.sys to composite devices
  * (bNumInterfaces >= 2 with bDeviceClass = 0x00). usbccgp creates
  * per-interface child device nodes whose friendly names refresh on
  * every enumeration -- which is what we want when the user changes
  * dev_config.device_name. The pre-Phase-4F single-interface layout
  * skipped usbccgp, hitting the persistent iProduct cache and
  * requiring the user to manually uninstall + replug to see the new
  * name. F103 has been multi-interface from day one (Joystick HID +
  * Configurator HID, since well before SimHub added CDC), which is
  * why this class of bug never surfaced there.
  ******************************************************************************
  */

#include "usbd_freejoy_class.h"
#include "common_defines.h"
#include <stdint.h>

/* Phase 4D: OutEvent calls App_HidOutDispatch directly (in
 * application/Src/usb_app.c) on the configurator interface. The joystick
 * interface is IN-only (no OUT endpoint, no SET_REPORT path), so it has
 * no OutEvent -- the report-descriptor below is a pure INPUT collection. */
extern void App_HidOutDispatch(const uint8_t *hid_buf);

/*============================================================================
 *  Joystick HID report descriptor (interface 0, REPORT_ID 1)
 *
 *  Verbatim port of F103 application/Src/usb_desc.c
 *  JoystickHID_ReportDescriptor[]. Generic-Desktop / Joystick collection
 *  with 128 buttons + 8 axes + 4 hats. MAX_BUTTONS_NUM=128 and
 *  MAX_AXIS_NUM=8 from common_defines.h drive the report sizing.
 *==========================================================================*/
__ALIGN_BEGIN uint8_t FreeJoy_JoyReportDesc[FREEJOY_JOY_REPORT_DESC_SIZE] __ALIGN_END = {
	0x05, 0x01,                     /* USAGE_PAGE (Generic Desktop) */
	0x09, 0x04,                     /* USAGE (Joystick) */
	0xA1, 0x01,                     /* COLLECTION (Application) */

	0x85, REPORT_ID_JOY,            /*   REPORT_ID (1) */

	/* buttons */
	0x05, 0x09,                     /*   USAGE_PAGE (Button) */
	0x19, 0x01,                     /*   USAGE_MINIMUM (Button 1) */
	0x29, MAX_BUTTONS_NUM,          /*   USAGE_MAXIMUM (Button 128) */
	0x15, 0x00,                     /*   LOGICAL_MINIMUM (0) */
	0x25, 0x01,                     /*   LOGICAL_MAXIMUM (1) */
	0x75, 0x01,                     /*   REPORT_SIZE (1) */
	0x95, MAX_BUTTONS_NUM,          /*   REPORT_COUNT (128) */
	0x81, 0x00,                     /*   INPUT (Data,Ary,Abs) */

	/* axes */
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

	/* POV / hats */
	0x09, 0x39,                     /*   USAGE (Hat switch) */
	0x15, 0x00,                     /*   LOGICAL_MINIMUM (0) */
	0x25, 0x07,                     /*   LOGICAL_MAXIMUM (7) */
	0x35, 0x00,                     /*   PHYSICAL_MINIMUM (0) */
	0x46, 0x3B, 0x01,               /*   PHYSICAL_MAXIMUM (315) */
	0x65, 0x12,                     /*   UNIT (SI Rot:Angular Pos) */
	0x75, 0x08,                     /*   REPORT_SIZE (8) */
	0x95, 0x01,                     /*   REPORT_COUNT (1) */
	0x81, 0x02,                     /*   INPUT (Data,Var,Abs) */
	0x09, 0x39, 0x81, 0x02,         /*   USAGE (Hat switch) + INPUT */
	0x09, 0x39, 0x81, 0x02,
	0x09, 0x39, 0x81, 0x02,

	0xC0,                           /* END_COLLECTION */
};

_Static_assert(sizeof(FreeJoy_JoyReportDesc) == FREEJOY_JOY_REPORT_DESC_SIZE,
               "FreeJoy_JoyReportDesc size != FREEJOY_JOY_REPORT_DESC_SIZE — "
               "update FREEJOY_JOY_REPORT_DESC_SIZE in usbd_conf.h");

/*============================================================================
 *  Configurator HID report descriptor (interface 1, REPORT_IDs 2..6)
 *
 *  Verbatim port of F103 application/Src/usb_desc.c
 *  CustomHID_ReportDescriptor[]. Vendor-Defined collection with 5
 *  REPORT_IDs: PARAM (2), CONFIG_IN (3), CONFIG_OUT (4), FIRMWARE (5),
 *  LED (6). Each REPORT_ID has its own INPUT and (optionally) OUTPUT
 *  fields sized for the configurator's wire-format protocol.
 *==========================================================================*/
__ALIGN_BEGIN uint8_t FreeJoy_CfgReportDesc[FREEJOY_CFG_REPORT_DESC_SIZE] __ALIGN_END = {
	0x06, 0x00, 0xFF,               /* USAGE_PAGE (Vendor Defined 1) */
	0x09, 0x01,                     /* USAGE (Vendor Usage 1) */
	0xA1, 0x01,                     /* COLLECTION (Application) */

	/* REPORT_ID 2 (PARAM): IN 63 bytes, OUT 1 byte */
	0x85, REPORT_ID_PARAM,          /*   REPORT_ID (2) */
	0x06, 0x00, 0xFF,               /*   USAGE_PAGE (Vendor Defined 1) */
	0x09, 0x02,                     /*   USAGE (Vendor 2) */
	0x15, 0x00, 0x26, 0xFF, 0x00,   /*   LOGICAL_MIN/MAX 0..255 */
	0x75, 0x08, 0x95, 0x3F,         /*   REPORT_SIZE 8 / COUNT 63 */
	0x81, 0x00,                     /*   INPUT */
	0x09, 0x03, 0x75, 0x08, 0x95, 0x01, 0x91, 0x00,

	/* REPORT_ID 3 (CONFIG_IN): IN 63 bytes, OUT 1 byte */
	0x85, REPORT_ID_CONFIG_IN,      /*   REPORT_ID (3) */
	0x06, 0x00, 0xFF,               /*   USAGE_PAGE (Vendor Defined 1) */
	0x09, 0x04, 0x15, 0x00, 0x26, 0xFF, 0x00,
	0x75, 0x08, 0x95, 0x3F, 0x81, 0x00,
	0x09, 0x05, 0x75, 0x08, 0x95, 0x01, 0x91, 0x00,

	/* REPORT_ID 4 (CONFIG_OUT): IN 1 byte, OUT 63 bytes */
	0x85, REPORT_ID_CONFIG_OUT,     /*   REPORT_ID (4) */
	0x09, 0x06, 0x75, 0x08, 0x95, 0x01, 0x81, 0x00,
	0x09, 0x07, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x00,

	/* REPORT_ID 5 (FIRMWARE): IN 2 bytes, OUT 63 bytes */
	0x85, REPORT_ID_FIRMWARE,       /*   REPORT_ID (5) */
	0x09, 0x08, 0x75, 0x08, 0x95, 0x02, 0x81, 0x00,
	0x09, 0x09, 0x75, 0x08, 0x95, 0x3F, 0x91, 0x00,

	/* REPORT_ID 6 (LED): OUT 4 bytes (no IN) */
	0x85, REPORT_ID_LED,            /*   REPORT_ID (6) */
	0x09, 0x0A, 0x75, 0x08, 0x95, 0x04, 0x91, 0x00,

	0xC0,                           /* END_COLLECTION */
};

_Static_assert(sizeof(FreeJoy_CfgReportDesc) == FREEJOY_CFG_REPORT_DESC_SIZE,
               "FreeJoy_CfgReportDesc size != FREEJOY_CFG_REPORT_DESC_SIZE — "
               "update FREEJOY_CFG_REPORT_DESC_SIZE in usbd_conf.h");

/*============================================================================
 *  Configurator-interface OutEvent: called by the composite class when
 *  a HID OUT report arrives on EP2 OUT. Routes to the board-agnostic
 *  App_HidOutDispatch which handles config receive, firmware-update
 *  trigger, and LED-state updates by report ID byte.
 *==========================================================================*/
void FreeJoy_CfgOutEvent(uint8_t *report_buffer)
{
	App_HidOutDispatch(report_buffer);
}
