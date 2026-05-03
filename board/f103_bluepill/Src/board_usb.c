/**
  ******************************************************************************
  * @file           : board_usb.c
  * @brief          : F103 BluePill board_usb.h wrappers.
  *
  * Thin shims on top of the existing F1 USB-FS-Device + custom HID
  * stack (application/Src/usb_hw.c, usb_endp.c). Keeps the F103 USB
  * code paths unchanged; just exposes them through the board_usb.h
  * names application code now calls.
  ******************************************************************************
  */

#include "board_usb.h"
#include "usb_hw.h"

extern int8_t USB_CUSTOM_HID_SendReport(uint8_t EP_num, uint8_t *data, uint8_t length);
extern void   USB_HW_Init(void);
extern void   USB_HW_DeInit(void);
extern void   PowerOff(void);
extern uint8_t SerialNum(uint8_t *str, uint8_t length);

void Board_USB_Init(void)
{
	USB_HW_Init();
}

void Board_USB_DeInit(void)
{
	PowerOff();
	USB_HW_DeInit();
}

int8_t Board_USB_SendReport(uint8_t report_id, uint8_t *data, uint8_t length)
{
	/* F103 maps report_id to the F1 endpoint slot:
	 *   JOY (1)         -> EP1 IN  (joystick HID interface)
	 *   PARAM (2)       -> EP2 IN  (custom HID interface, params reports)
	 *   CONFIG_IN (3)   -> EP2 IN  (custom HID interface, config reads)
	 *   FIRMWARE/LED    -> not sent IN -- they're OUT-only */
	uint8_t ep = (report_id == 1) ? 1 : 2;
	return USB_CUSTOM_HID_SendReport(ep, data, length);
}

void Board_GetSerialNum(uint8_t *str, uint8_t length)
{
	(void)SerialNum(str, length);
}
