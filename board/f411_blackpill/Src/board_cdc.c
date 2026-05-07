/**
  ******************************************************************************
  * @file           : board_cdc.c
  * @brief          : F411 CDC ACM bridge for SimHub.
  *
  * Wraps the FreeJoy composite class's CDC helpers in the symbol names
  * that application/Src/simhub.c expects (CDC_Send_DATA,
  * SH_ProcessEndpData). On F103 these were implemented inline in
  * application/Src/usb_endp.c against the USB-FS-Device library; F411
  * routes them through the Cube USBD-based custom composite class in
  * board/f411_blackpill/Src/usbd_freejoy_class.c.
  *
  * Phase 4E (HID + CDC composite) -- replaces the no-op stubs that lived
  * in the old board_phase_stubs.c file. Until Phase 4E landed, F411
  * accepted SimHub-targeted firmware calls but never sent or received
  * any bytes on the wire; SimHub on the host saw a non-responsive virtual
  * COM port (or no COM port at all, since the CDC interface wasn't
  * declared).
  ******************************************************************************
  */

#include <stdint.h>

#include "usbd_def.h"
#include "usbd_freejoy_class.h"

extern USBD_HandleTypeDef hUsbDeviceFS;        /* board/f411_blackpill/Src/board_usb.c */

/* Push outgoing bytes onto EP3 IN. Mirrors the F103 signature in
 * application/Inc/usb_endp.h::CDC_Send_DATA -- inverted-return convention
 * preserved (-1 = queued, 0 = BUSY). simhub.c::WriteLine ignores the
 * return value, but the convention is documented for parity. */
int8_t CDC_Send_DATA(uint8_t *ptrBuffer, uint8_t send_length)
{
	return USBD_FreeJoy_SendCdcData(&hUsbDeviceFS, ptrBuffer, send_length);
}

/* Re-arm EP3 OUT receive after the SimHub ring buffer drains. Called
 * periodically from application/Src/simhub.c::SH_Read while it polls for
 * incoming bytes. The composite class also re-arms inline from DataOut
 * when the ring still has space; this polled path is the recovery hook
 * after the application drains a full ring. */
void SH_ProcessEndpData(void)
{
	USBD_FreeJoy_RearmCdcReceive(&hUsbDeviceFS);
}
