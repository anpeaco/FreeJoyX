/**
  ******************************************************************************
  * @file           : board_usb.h
  * @brief          : Board-agnostic USB device API.
  *
  * F103 wraps the F1 USB-FS-Device library (StdPeriph-era) plus the
  * application's hand-rolled custom HID class (application/Src/usb_*.c).
  * F411 wraps Cube's USBD middleware on top of HAL_PCD plus the
  * USBD_CUSTOM_HID class (board/f411_blackpill/Src/usbd_*.c plus the
  * vendored Drivers/STM32_USB_Device_Library/).
  *
  * This header is the seam application code calls -- the underlying
  * USB stack is invisible to main.c, simhub.c, etc.
  ******************************************************************************
  */

#ifndef BOARD_USB_H_
#define BOARD_USB_H_

#include <stdint.h>

/* Bring up the USB peripheral. F103: enables RCC + GPIO + NVIC for the
 * USB-FS-Device, configures the F1 USB stack callbacks, asserts the
 * PA12 pull-up. F411: configures OTG-FS GPIOs (PA11/PA12 AF10) +
 * NVIC, runs USBD_Init / USBD_RegisterClass / USBD_RegisterInterface
 * / USBD_Start. Idempotent within reason; main.c calls once at
 * startup. */
void Board_USB_Init(void);

/* Tear down the USB peripheral. F103: PowerOff + USB_HW_DeInit.
 * F411: USBD_Stop + USBD_DeInit. Used in the bootloader-jump path
 * to give the host a graceful disconnect before the reset. */
void Board_USB_DeInit(void);

/* Send a HID IN report on the configurator channel (EP1 IN on F411,
 * EP2 IN on F103's composite layout). report_id = 1..6 per the
 * REPORT_ID_* enum in common_defines.h. data[0] should already be
 * report_id; length includes that byte. Returns 0 on accepted, -1
 * if the previous IN transfer hasn't completed yet (caller retries
 * next tick). */
int8_t Board_USB_SendReport(uint8_t report_id, uint8_t *data, uint8_t length);

/* Build a 24-character ASCII serial number from the chip UID into the
 * caller's buffer. F103: reads 0x1FFFF7E8 (96-bit unique device ID).
 * F411: reads 0x1FFF7A10 via LL_GetUID_Word0/1/2. The serial is used
 * by main.c to initialise the simhub identity packet. */
void Board_GetSerialNum(uint8_t *str, uint8_t length);

#endif /* BOARD_USB_H_ */
