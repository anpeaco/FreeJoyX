/**
  ******************************************************************************
  * @file           : usbd_freejoy_desc.c
  * @brief          : F411 USB descriptors (device + strings).
  *
  * Provides USBD_DescriptorsTypeDef vtable instance the USBD core calls
  * during enumeration to fetch the device descriptor and the various
  * string descriptors. The CustomHID class auto-generates the config +
  * interface + endpoint + HID descriptors from values in usbd_conf.h
  * plus the report descriptor passed via the USBD_CUSTOM_HID_ItfTypeDef
  * (see usbd_freejoy_if.c).
  *
  * Wire-format VID 0x0483 (ST) / PID 0x5750 unchanged from F103 so the
  * configurator's existing connect path works on either board. Manufacturer
  * "FreeJoyX", product "FreeJoyX HID". Serial built from the F411 96-bit
  * UID at 0x1FFF7A10 via LL_GetUID_Word0/1/2.
  ******************************************************************************
  */

#include "usbd_def.h"
#include "usbd_core.h"
#include "stm32f4xx_ll_utils.h"

#include <string.h>

#ifndef BOOTLOADER
#include "common_types.h"

/* Defined in application/Src/main.c. The bootloader build (-DBOOTLOADER
 * in makefile.boot) skips this so it doesn't need application headers
 * or the dev_config symbol -- the bootloader keeps the static product
 * string. */
extern dev_config_t dev_config;
#endif

#define USBD_VID                        0x0483
#define USBD_PID                        0x5750
#define USBD_LANGID_STRING              0x0409  /* U.S. English */
#define USBD_SIZ_STRING_SERIAL          0x1A    /* 2-byte header + 12 chars * 2 (UTF-16) */
#define USBD_MANUFACTURER_STRING        "FreeJoyX"
/* In the bootloader build the iProduct identifies the firmware as
 * "FreeJoyX Flasher" so the configurator's flasher-detection filter
 * (HidDevice::FLASHER_PROD_STR) recognises the F411 bootloader the
 * same way it recognises the F103 one. The application build keeps
 * "FreeJoyX HID" as the fallback when dev_config.device_name is
 * empty (factory-reset / fresh-boot). */
#ifdef BOOTLOADER
#define USBD_PRODUCT_FS_STRING          "FreeJoyX Flasher"
#else
#define USBD_PRODUCT_FS_STRING          "FreeJoyX HID"
#endif
#define USBD_CONFIGURATION_FS_STRING    "FreeJoyX Config"
#define USBD_INTERFACE_FS_STRING        "FreeJoyX Interface"

/* Device descriptor -- standard USB 2.00 single-config layout. */
__ALIGN_BEGIN static uint8_t USBD_DeviceDesc[USB_LEN_DEV_DESC] __ALIGN_END = {
	0x12,                       /* bLength */
	USB_DESC_TYPE_DEVICE,       /* bDescriptorType */
	0x00, 0x02,                 /* bcdUSB = 2.00 */
	0x00,                       /* bDeviceClass (defined per-interface) */
	0x00,                       /* bDeviceSubClass */
	0x00,                       /* bDeviceProtocol */
	USB_MAX_EP0_SIZE,           /* bMaxPacketSize EP0 */
	LOBYTE(USBD_VID), HIBYTE(USBD_VID),
	LOBYTE(USBD_PID), HIBYTE(USBD_PID),
	0x00, 0x02,                 /* bcdDevice = 2.00 */
	USBD_IDX_MFC_STR,           /* iManufacturer */
	USBD_IDX_PRODUCT_STR,       /* iProduct */
	USBD_IDX_SERIAL_STR,        /* iSerialNumber */
	USBD_MAX_NUM_CONFIGURATION, /* bNumConfigurations */
};

/* Language ID string. */
__ALIGN_BEGIN static uint8_t USBD_LangIDDesc[USB_LEN_LANGID_STR_DESC] __ALIGN_END = {
	USB_LEN_LANGID_STR_DESC,
	USB_DESC_TYPE_STRING,
	LOBYTE(USBD_LANGID_STRING),
	HIBYTE(USBD_LANGID_STRING),
};

/* Scratch buffer for ASCII-to-UTF16 string-descriptor conversion. */
__ALIGN_BEGIN static uint8_t USBD_StrDesc[USBD_MAX_STR_DESC_SIZ] __ALIGN_END;

/* Build a USB string descriptor from an ASCII C string. CubeF4 supplies
 * USBD_GetString in usbd_ctlreq.c which does this; declare it locally. */
extern void USBD_GetString(uint8_t *desc, uint8_t *unicode, uint16_t *len);

/* Serial number derived from the 96-bit chip UID at 0x1FFF7A10. Each
 * 32-bit word becomes 8 hex digits; total 24 ASCII chars. */
__ALIGN_BEGIN static uint8_t USBD_StringSerial[USBD_SIZ_STRING_SERIAL] __ALIGN_END = {
	USBD_SIZ_STRING_SERIAL,
	USB_DESC_TYPE_STRING,
};

static void IntToUnicode(uint32_t value, uint8_t *pbuf, uint8_t len)
{
	for (uint8_t i = 0; i < len; i++) {
		uint8_t nibble = (value >> 28) & 0x0F;
		pbuf[2 * i] = (nibble < 10) ? ('0' + nibble) : ('A' + nibble - 10);
		pbuf[2 * i + 1] = 0;
		value <<= 4;
	}
}

static void Get_SerialNumStr(void)
{
	uint32_t w0 = LL_GetUID_Word0();
	uint32_t w1 = LL_GetUID_Word1();
	uint32_t w2 = LL_GetUID_Word2();

	/* Mix word2 into the lower words so the serial uses the full 96 bits. */
	w0 += w2;

	IntToUnicode(w0, &USBD_StringSerial[2],     8);
	IntToUnicode(w1, &USBD_StringSerial[2 + 16], 4);
}

static uint8_t * USBD_FreeJoy_DeviceDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	(void)speed;
	*length = sizeof(USBD_DeviceDesc);
	return USBD_DeviceDesc;
}

static uint8_t * USBD_FreeJoy_LangIDStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	(void)speed;
	*length = sizeof(USBD_LangIDDesc);
	return USBD_LangIDDesc;
}

static uint8_t * USBD_FreeJoy_ManufacturerStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	(void)speed;
	USBD_GetString((uint8_t *)USBD_MANUFACTURER_STRING, USBD_StrDesc, length);
	return USBD_StrDesc;
}

static uint8_t * USBD_FreeJoy_ProductStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	(void)speed;

#ifndef BOOTLOADER
	/* Mirror F103's Get_ProductStr (application/Src/usb_hw.c:238): pull
	 * the configurator-set USB product name out of dev_config.device_name.
	 * The field is 26 bytes, may or may not be NUL-terminated; copy through
	 * a NUL-terminated scratch buffer so USBD_GetString's strlen is safe.
	 * Fall back to the compiled-in default when the slot is empty (first
	 * boot, factory-reset, or wire-format-driven blanking). */
	static char product_name[27];
	memcpy(product_name, dev_config.device_name, sizeof(dev_config.device_name));
	product_name[sizeof(product_name) - 1] = '\0';

	const uint8_t *src = (product_name[0] != '\0')
		? (const uint8_t *)product_name
		: (const uint8_t *)USBD_PRODUCT_FS_STRING;

	USBD_GetString((uint8_t *)src, USBD_StrDesc, length);
#else
	USBD_GetString((uint8_t *)USBD_PRODUCT_FS_STRING, USBD_StrDesc, length);
#endif
	return USBD_StrDesc;
}

static uint8_t * USBD_FreeJoy_SerialStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	(void)speed;
	*length = USBD_SIZ_STRING_SERIAL;
	Get_SerialNumStr();
	return USBD_StringSerial;
}

static uint8_t * USBD_FreeJoy_ConfigStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	(void)speed;
	USBD_GetString((uint8_t *)USBD_CONFIGURATION_FS_STRING, USBD_StrDesc, length);
	return USBD_StrDesc;
}

static uint8_t * USBD_FreeJoy_InterfaceStrDescriptor(USBD_SpeedTypeDef speed, uint16_t *length)
{
	(void)speed;
	USBD_GetString((uint8_t *)USBD_INTERFACE_FS_STRING, USBD_StrDesc, length);
	return USBD_StrDesc;
}

USBD_DescriptorsTypeDef FreeJoy_Desc = {
	USBD_FreeJoy_DeviceDescriptor,
	USBD_FreeJoy_LangIDStrDescriptor,
	USBD_FreeJoy_ManufacturerStrDescriptor,
	USBD_FreeJoy_ProductStrDescriptor,
	USBD_FreeJoy_SerialStrDescriptor,
	USBD_FreeJoy_ConfigStrDescriptor,
	USBD_FreeJoy_InterfaceStrDescriptor,
};
