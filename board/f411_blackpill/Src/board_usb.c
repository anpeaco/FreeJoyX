/**
  ******************************************************************************
  * @file           : board_usb.c
  * @brief          : F411 BlackPill board_usb.h implementation.
  *
  * Drives the Cube USBD middleware. Phase 4F switched the app from the
  * stock single-CustomHID class to a custom dual-HID composite class
  * (`USBD_FreeJoy_CompositeClass` from `usbd_freejoy_class.c`), so
  * Windows binds usbccgp.sys to the device and per-interface friendly
  * names refresh on every enumeration. Send-report routes by REPORT_ID:
  * REPORT_ID_JOY (1) goes to EP1 IN, REPORT_IDs 2..6 go to EP2 IN.
  *
  * The bootloader keeps the stock Cube CustomHID single-interface
  * layout (separate makefile, `-DBOOTLOADER` flag, separate descriptor
  * source).
  ******************************************************************************
  */

#include "board_usb.h"
#include "stm32f4xx_ll_utils.h"

#include "usbd_core.h"
#include "usbd_freejoy_class.h"
#include "common_defines.h"

extern USBD_DescriptorsTypeDef FreeJoy_Desc;     /* usbd_freejoy_desc.c */

USBD_HandleTypeDef hUsbDeviceFS;

void Board_USB_Init(void)
{
	USBD_Init(&hUsbDeviceFS, &FreeJoy_Desc, 0);
	USBD_RegisterClass(&hUsbDeviceFS, &USBD_FreeJoy_CompositeClass);
	USBD_Start(&hUsbDeviceFS);
}

void Board_USB_DeInit(void)
{
	USBD_Stop(&hUsbDeviceFS);
	USBD_DeInit(&hUsbDeviceFS);
}

int8_t Board_USB_SendReport(uint8_t report_id, uint8_t *data, uint8_t length)
{
	if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
		return -1;
	}

	/* Route by report ID. Joystick (REPORT_ID 1) goes out on the
	 * joystick HID interface (EP1 IN); everything else (PARAM,
	 * CONFIG_IN, FIRMWARE, LED) goes on the configurator HID
	 * interface (EP2 IN). The two endpoints are independent, so
	 * joystick reports no longer contend with configurator traffic
	 * the way they did with single-interface (see usb_app.c
	 * Phase-4F follow-up that drops the alternation hack). */
	uint8_t rc;
	if (report_id == REPORT_ID_JOY) {
		rc = USBD_FreeJoy_SendJoyReport(&hUsbDeviceFS, data, length);
	} else {
		rc = USBD_FreeJoy_SendCfgReport(&hUsbDeviceFS, data, length);
	}
	return (rc == USBD_OK) ? 0 : -1;
}

static char hex_char(uint8_t nibble)
{
	return (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
}

void Board_GetSerialNum(uint8_t *str, uint8_t length)
{
	uint32_t w0 = LL_GetUID_Word0();
	uint32_t w1 = LL_GetUID_Word1();
	uint32_t w2 = LL_GetUID_Word2();

	/* Format: 24 hex chars = three 32-bit words written MSB-first.
	 * Matches the visible string F103's SerialNum produces -- enough
	 * uniqueness for the configurator's per-device identity match. */
	uint32_t words[3] = { w0, w1, w2 };
	for (uint8_t i = 0; i < 24 && i < length; i++) {
		uint8_t word_idx = i / 8;
		uint8_t shift = 28 - (i % 8) * 4;
		str[i] = (uint8_t)hex_char((words[word_idx] >> shift) & 0xF);
	}
	if (length > 24) str[24] = 0;
}
