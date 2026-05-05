/**
  ******************************************************************************
  * @file    usbd_conf.h
  * @brief   FreeJoyX-F411 USBD low-level driver configuration.
  *
  * Trimmed-down version of the CubeF4 template (the upstream is
  * Middlewares/ST/STM32_USB_Device_Library/Core/Inc/usbd_conf_template.h).
  * Strips the per-class config knobs we don't use (DFU, AUDIO, MSC, UVC,
  * CDC, BillBoard, ECM, RNDIS) -- we ship one CustomHID class and that's it.
  *
  * Memory: static allocation via USBD_static_malloc / USBD_static_free
  * declared at the bottom; the implementations live in usbd_conf.c.
  * No dynamic allocator is used, so the firmware can run without malloc.
  *
  * Logging: all USBD_UsrLog / USBD_ErrLog / USBD_DbgLog macros expand to
  * empty -- no printf in the firmware (no UART console, syscalls.c
  * stubs anything libc tries).
  ******************************************************************************
  */

#ifndef __USBD_CONF_H
#define __USBD_CONF_H

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f4xx.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define USBD_MAX_NUM_INTERFACES                     1U
#define USBD_MAX_NUM_CONFIGURATION                  1U
#define USBD_MAX_STR_DESC_SIZ                       0x100U
#define USBD_SELF_POWERED                           0U     /* bus-powered */
#define USBD_DEBUG_LEVEL                            0U     /* no printf */

/* CustomHID Class Config. USBD_CUSTOM_HID_REPORT_DESC_SIZE must equal
 * the EXACT byte count of FreeJoy_ReportDesc in usbd_freejoy_if.c --
 * the class library reports it as wDescriptorLength to the host AND
 * as the GET_REPORT_DESCRIPTOR response length. Any mismatch produces
 * trailing zeros that Windows treats as malformed HID items, failing
 * enumeration with Code 10. A _Static_assert in usbd_freejoy_if.c
 * pins the size to sizeof(FreeJoy_ReportDesc); update both together
 * if the descriptor changes.
 *
 * CUSTOM_HID_EPIN_SIZE and CUSTOM_HID_EPOUT_SIZE override the class
 * library's defaults of 2 bytes -- those defaults are wMaxPacketSize
 * baked into the config descriptor, and Cube's defaults are sized for
 * tiny consumer-control devices, not 64-byte HID reports. With the
 * default of 2, Windows fragmented our 64-byte IN reports into 32 ×
 * 2-byte packets (slow but worked) and silently dropped most OUT
 * reports (App_HidOutDispatch never fired for CONFIG_IN). 64 matches
 * what we actually send/receive. */
#define CUSTOM_HID_HS_BINTERVAL                     0x05U
#define CUSTOM_HID_FS_BINTERVAL                     0x05U
#define CUSTOM_HID_EPIN_SIZE                        0x40U  /* 64 -- one HID IN chunk */
#define CUSTOM_HID_EPOUT_SIZE                       0x40U  /* 64 -- one HID OUT chunk */
#define USBD_CUSTOMHID_OUTREPORT_BUF_SIZE           0x40U  /* 64 -- one HID OUT chunk */
#define USBD_CUSTOM_HID_REPORT_DESC_SIZE            181U

/* Pass the entire 64-byte HID OUT buffer to OutEvent so the dispatch
 * code can read the report ID from byte[0] and route to the matching
 * EP1_OUT_Callback handler -- matches the F103 usb_endp.c flow. The
 * default (event_idx, state) signature is intended for a different
 * style of HID device where each OUT report is a single button event. */
#define USBD_CUSTOMHID_REPORT_BUFFER_EVENT_ENABLED

/* Static memory aliases -- USBD_static_malloc allocates from a pool
 * sized for one USBD_CUSTOM_HID_HandleTypeDef instance. */
#define USBD_malloc                                 (void *)USBD_static_malloc
#define USBD_free                                   USBD_static_free
#define USBD_memset                                 memset
#define USBD_memcpy                                 memcpy
#define USBD_Delay                                  HAL_Delay

#define USBD_UsrLog(...)                            do {} while (0)
#define USBD_ErrLog(...)                            do {} while (0)
#define USBD_DbgLog(...)                            do {} while (0)

void *USBD_static_malloc(uint32_t size);
void  USBD_static_free(void *p);

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CONF_H */
