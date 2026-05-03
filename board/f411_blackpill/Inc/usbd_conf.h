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

/* CustomHID Class Config -- mirrors application/Src/usb_desc.c report
 * descriptor sizing. The 7 report-IDs (JOY/PARAM/CONFIG_IN/CONFIG_OUT/
 * FIRMWARE/LED) plus the 64-byte payload + 1-byte ID header in
 * usb_desc.c::CustomHID_ReportDescriptor compute to 233 bytes. Round up
 * for safety; usbd_freejoy_desc.c reports the exact byte count back to
 * the host via Get_ReportDescriptor. */
#define CUSTOM_HID_HS_BINTERVAL                     0x05U
#define CUSTOM_HID_FS_BINTERVAL                     0x05U
#define USBD_CUSTOMHID_OUTREPORT_BUF_SIZE           0x40U  /* 64 -- one HID OUT chunk */
#define USBD_CUSTOM_HID_REPORT_DESC_SIZE            233U

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
