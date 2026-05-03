/**
  ******************************************************************************
  * @file           : board_usb.c
  * @brief          : F411 BlackPill board_usb.h implementation.
  *
  * Drives the Cube USBD middleware: USBD_Init, USBD_RegisterClass with
  * USBD_CUSTOM_HID_CLASS, USBD_CUSTOM_HID_RegisterInterface with the
  * FreeJoy_HID_fops descriptor pointer + OutEvent dispatch (defined
  * in usbd_freejoy_if.c), USBD_Start. Send-report routes through
  * USBD_CUSTOM_HID_SendReport which queues an IN transfer on EP1.
  ******************************************************************************
  */

#include "board_usb.h"
#include "stm32f4xx_ll_utils.h"

#include "usbd_core.h"
#include "usbd_customhid.h"

extern USBD_DescriptorsTypeDef       FreeJoy_Desc;       /* usbd_freejoy_desc.c */
extern USBD_CUSTOM_HID_ItfTypeDef    FreeJoy_HID_fops;   /* usbd_freejoy_if.c */

USBD_HandleTypeDef hUsbDeviceFS;

void Board_USB_Init(void)
{
	USBD_Init(&hUsbDeviceFS, &FreeJoy_Desc, 0);
	USBD_RegisterClass(&hUsbDeviceFS, USBD_CUSTOM_HID_CLASS);
	USBD_CUSTOM_HID_RegisterInterface(&hUsbDeviceFS, &FreeJoy_HID_fops);
	USBD_Start(&hUsbDeviceFS);
}

void Board_USB_DeInit(void)
{
	USBD_Stop(&hUsbDeviceFS);
	USBD_DeInit(&hUsbDeviceFS);
}

int8_t Board_USB_SendReport(uint8_t report_id, uint8_t *data, uint8_t length)
{
	(void)report_id;	/* F411 uses a single CustomHID class instance with
	                     all 7 report IDs in one collection -- the report_id
	                     is already in data[0] per the HID convention. */

	if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED) {
		return -1;
	}

	/* USBD_CUSTOM_HID_SendReport returns USBD_OK (0) when the IN
	 * transfer was queued, USBD_BUSY (1) when the previous IN report
	 * is still in flight. Convert to the F103 0/-1 convention. */
	uint8_t rc = USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, data, length);
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
