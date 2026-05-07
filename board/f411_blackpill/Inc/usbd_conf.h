/**
  ******************************************************************************
  * @file    usbd_conf.h
  * @brief   FreeJoyX-F411 USBD low-level driver configuration.
  *
  * Trimmed-down version of the CubeF4 template (the upstream is
  * Middlewares/ST/STM32_USB_Device_Library/Core/Inc/usbd_conf_template.h).
  * Strips the per-class config knobs we don't use (DFU, AUDIO, MSC, UVC,
  * CDC, BillBoard, ECM, RNDIS).
  *
  * The application links a custom dual-HID composite class
  * (`board/f411_blackpill/Src/usbd_freejoy_class.c`) implementing
  * Joystick HID on EP1 IN and Configurator HID on EP2 IN/OUT. This
  * matches F103's two-HID layout so Windows binds usbccgp.sys for the
  * composite parent — required for per-interface friendly-name refresh
  * (Phase 4F).
  *
  * The bootloader keeps the stock CubeF4 `Class/CustomHID/` (single
  * interface) for its DFU flow. -DBOOTLOADER from `armgcc/makefile.boot`
  * gates app-only paths in shared BSP files.
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

/* App build is HID + HID + CDC composite (Phase 4E): 4 interfaces.
 *   Interface 0 — Joystick HID
 *   Interface 1 — Configurator HID
 *   Interface 2 — CDC Communication Class (no endpoints; see Phase 4E
 *                 plan, Option A — F411 OTG-FS doesn't have a 5th IN
 *                 slot for CDC notification, so we omit it. Linux
 *                 cdc-acm + Windows usbser.sys + macOS IOUSBHost all
 *                 accept bNumEndpoints=0 here.)
 *   Interface 3 — CDC Data Class (EP3 IN + EP3 OUT bulk)
 * Bootloader keeps the stock single-CustomHID layout (1 interface). */
#ifdef BOOTLOADER
#define USBD_MAX_NUM_INTERFACES                     1U
#else
#define USBD_MAX_NUM_INTERFACES                     4U
#endif
#define USBD_MAX_NUM_CONFIGURATION                  1U
#define USBD_MAX_STR_DESC_SIZ                       0x100U
#define USBD_SELF_POWERED                           0U     /* bus-powered */
#define USBD_DEBUG_LEVEL                            0U     /* no printf */

/* ============================================================
 * Bootloader: stock Cube CustomHID class (single interface)
 * ============================================================
 *
 * `USBD_CUSTOM_HID_REPORT_DESC_SIZE` must equal the EXACT byte count of
 * Boot_ReportDesc in bootloader/f411/Src/boot_usb_if.c -- the class
 * library reports it as wDescriptorLength to the host AND as the
 * GET_REPORT_DESCRIPTOR response length. Any mismatch produces trailing
 * zeros that Windows treats as malformed HID items, failing enumeration
 * with Code 10. The bootloader's report descriptor is the same shape as
 * the app's pre-Phase-4F descriptor (181 bytes); revisit if the
 * bootloader's flasher protocol ever changes shape.
 *
 * The CUSTOM_HID_EPIN/EPOUT macros override the class library's defaults
 * of 2 bytes -- Cube ships defaults sized for tiny consumer-control
 * devices, not 64-byte HID reports. Without the override Windows
 * fragmented our 64-byte IN reports into 32 x 2-byte packets and
 * silently dropped most OUT reports. */
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

/* ============================================================
 * Application: dual-HID composite (Phase 4F)
 * ============================================================
 *
 * Joystick HID (REPORT_ID 1) on EP1 IN. F103 polls at 1 ms; matched here.
 * Configurator HID (REPORT_IDs 2..6) on EP2 IN + EP2 OUT. F103 polls EP2
 * IN at 2 ms and EP2 OUT at 16 ms; matched.
 *
 * Endpoint pressure: F411 OTG-FS has 4 IN + 4 OUT EP pairs (incl. EP0).
 * App uses EP0 + EP1 IN + EP2 IN/OUT = 3 IN + 1 OUT (excl. EP0). EP3 is
 * free for Phase 4E (CDC for SimHub) which would add 1 IN + 1 OUT (and
 * may need a third EP for CDC notification, share-able with EP0 control
 * via class-specific requests if pressure becomes real).
 *
 * Report descriptor sizes (FREEJOY_JOY_REPORT_DESC_SIZE / _CFG_) are
 * pinned via _Static_assert in usbd_freejoy_if.c against the actual
 * array sizes -- matches the discipline established for the single-
 * interface descriptor before Phase 4F.
 */
#define FREEJOY_JOY_EPIN_ADDR                       0x81U
#define FREEJOY_JOY_EPIN_SIZE                       0x40U
#define FREEJOY_JOY_FS_BINTERVAL                    0x01U  /* 1 ms */

#define FREEJOY_CFG_EPIN_ADDR                       0x82U
#define FREEJOY_CFG_EPIN_SIZE                       0x40U
#define FREEJOY_CFG_EPIN_FS_BINTERVAL               0x02U  /* 2 ms */

#define FREEJOY_CFG_EPOUT_ADDR                      0x02U
#define FREEJOY_CFG_EPOUT_SIZE                      0x40U
#define FREEJOY_CFG_EPOUT_FS_BINTERVAL              0x10U  /* 16 ms */

/* Shared OUT report buffer size; same 64-byte HID chunk as the bootloader. */
#define FREEJOY_OUTREPORT_BUF_SIZE                  0x40U

/* Pinned-size macros for the two app report descriptors. Bytes match
 * F103's JoystickHID_SIZ_REPORT_DESC and CustomHID_SIZ_REPORT_DESC
 * exactly, since Phase 4F ports F103's descriptors verbatim. */
#define FREEJOY_JOY_REPORT_DESC_SIZE                86U
#define FREEJOY_CFG_REPORT_DESC_SIZE                106U

/* CDC for SimHub (Phase 4E). Bulk data on EP3 IN/OUT. CDC notification
 * EP omitted -- see USBD_MAX_NUM_INTERFACES comment above for why.
 *
 * EP3 IN TXFIFO is sized for 2x 64-byte bulk packets pipelined; RX
 * shared FIFO covers EP2 OUT + EP3 OUT (sizing details in usbd_conf.c).
 *
 * Class requests handled in usbd_freejoy_class.c::USBD_FreeJoy_Setup
 * for interface 2 (CDC Communication): GET_LINE_CODING,
 * SET_LINE_CODING, SET_CONTROL_LINE_STATE, SEND_BREAK. */
#define FREEJOY_CDC_DATA_EPIN_ADDR                  0x83U  /* EP3 IN */
#define FREEJOY_CDC_DATA_EPOUT_ADDR                 0x03U  /* EP3 OUT */
#define FREEJOY_CDC_DATA_SIZE                       0x40U  /* 64-byte bulk */

/* CDC class request codes (USB CDC PSTN spec §6.3.10 / §6.3.12). */
#define CDC_SET_LINE_CODING                         0x20U
#define CDC_GET_LINE_CODING                         0x21U
#define CDC_SET_CONTROL_LINE_STATE                  0x22U
#define CDC_SEND_BREAK                              0x23U
#define CDC_LINE_CODING_LEN                         7U     /* dwDTERate (4) + bCharFormat + bParityType + bDataBits */

/* Static memory aliases -- USBD_static_malloc allocates from a pool
 * sized for one class handle instance. Bootloader allocates a
 * USBD_CUSTOM_HID_HandleTypeDef; app allocates a USBD_FreeJoy_HandleTypeDef.
 * The pool is sized in usbd_conf.c for the larger of the two so a
 * single binary target works either way. */
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
