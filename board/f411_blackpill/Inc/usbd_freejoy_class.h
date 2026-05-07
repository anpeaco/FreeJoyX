/**
  ******************************************************************************
  * @file    usbd_freejoy_class.h
  * @brief   F411 HID+HID+CDC composite class for FreeJoy (Phase 4E).
  *
  * Custom USBD_ClassTypeDef implementation that owns four interfaces:
  *
  *   Interface 0 (Joystick HID)        EP1 IN  — REPORT_ID 1
  *   Interface 1 (Configurator HID)    EP2 IN  — REPORT_IDs 2, 3, 5
  *                                     EP2 OUT — REPORT_IDs 4, 5, 6
  *   Interface 2 (CDC Communication)   no endpoints (notification omitted
  *                                     to fit F411 OTG-FS's 4-EP budget;
  *                                     see Phase 4E plan, Option A)
  *   Interface 3 (CDC Data)            EP3 IN  — bulk to host (SimHub TX)
  *                                     EP3 OUT — bulk from host (SimHub RX)
  *
  * Cube USBD's USBD_HandleTypeDef holds a single `pClass` pointer, so we
  * cannot register multiple class instances. This class is a focused
  * composite dispatcher that routes Setup / DataIn / DataOut by interface
  * number and endpoint number, internally maintaining separate state for
  * each interface.
  *
  * Bootloader stays on stock Cube CustomHID (single interface) — its
  * makefile excludes `usbd_freejoy_class.c` from the source list and
  * sets `-DBOOTLOADER` so shared headers (usbd_conf.h, usbd_freejoy_desc.c)
  * pick the bootloader-flavoured paths.
  ******************************************************************************
  */

#ifndef __USBD_FREEJOY_CLASS_H
#define __USBD_FREEJOY_CLASS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "usbd_ioreq.h"
#include "usbd_conf.h"

/* HID class spec request codes (USB HID 1.11 §7.2). Local copies so we
 * don't have to include usbd_customhid.h (we don't use that class). */
#define HID_REQ_GET_REPORT             0x01U
#define HID_REQ_GET_IDLE               0x02U
#define HID_REQ_GET_PROTOCOL           0x03U
#define HID_REQ_SET_REPORT             0x09U
#define HID_REQ_SET_IDLE               0x0AU
#define HID_REQ_SET_PROTOCOL           0x0BU

#define HID_DESCRIPTOR_TYPE            0x21U  /* HID class descriptor */
#define HID_REPORT_DESCRIPTOR_TYPE     0x22U  /* HID report descriptor */

/* Per-interface IN-pipe state. CUSTOM_HID_IDLE means the EP IN FIFO is
 * available; CUSTOM_HID_BUSY means an IN transfer is in flight and the
 * next SendReport will return BUSY. */
typedef enum {
	FREEJOY_HID_IDLE = 0,
	FREEJOY_HID_BUSY = 1,
} FreeJoy_HidState_t;

/* Composite class handle. One instance allocated by USBD core per
 * device through USBD_static_malloc. Holds:
 *   - per-interface IN-pipe states (joystick + configurator)
 *   - host-side HID state (Protocol / IdleState / AltSetting) per i/f
 *   - configurator OUT report buffer for EP2 OUT receives
 *   - SET_REPORT control-transfer staging
 */
typedef struct {
	FreeJoy_HidState_t  joy_state;        /* EP1 IN status */
	FreeJoy_HidState_t  cfg_state;        /* EP2 IN status */

	uint8_t             joy_protocol;
	uint8_t             joy_idle_state;
	uint8_t             joy_alt_setting;

	uint8_t             cfg_protocol;
	uint8_t             cfg_idle_state;
	uint8_t             cfg_alt_setting;

	/* SET_REPORT staging. EP0 RxReady demuxes to the configurator's
	 * OutEvent; the joystick interface has no SET_REPORT path. */
	uint8_t             set_report_pending;
	uint8_t             set_report_target_iface;
	uint8_t             ep2_out_buf[FREEJOY_OUTREPORT_BUF_SIZE];

	/* CDC state (Phase 4E). Mirrors F103's CDC_Send_DATA / EP4_OUT_Callback
	 * flow from application/Src/usb_endp.c. */
	FreeJoy_HidState_t  cdc_in_state;          /* EP3 IN busy/idle */
	uint8_t             cdc_rx_armed;          /* EP3 OUT receive in flight */
	uint8_t             set_line_coding_pending;
	uint8_t             cdc_line_coding[CDC_LINE_CODING_LEN];

	/* Static TX/RX buffers -- per memory note
	 * (project_f411_async_fifo_stack_buffer.md), buffers passed to
	 * USBD_LL_Transmit MUST be persistent. The HAL writes the FIFO
	 * asynchronously from the TXFE IRQ; stack buffers go out of scope
	 * before the FIFO load runs. */
	uint8_t             cdc_tx_buf[FREEJOY_CDC_DATA_SIZE];
	uint8_t             cdc_rx_buf[FREEJOY_CDC_DATA_SIZE];
} USBD_FreeJoy_HandleTypeDef;

/* Class instance — register with USBD_RegisterClass(). */
extern USBD_ClassTypeDef USBD_FreeJoy_CompositeClass;

/* Send a 64-byte joystick HID IN report on EP1 IN. Returns USBD_OK if
 * queued, USBD_BUSY if previous IN transfer hasn't finished, USBD_FAIL
 * if device not yet configured or class handle missing. */
uint8_t USBD_FreeJoy_SendJoyReport(USBD_HandleTypeDef *pdev,
                                   uint8_t *report, uint16_t len);

/* Send a 64-byte configurator HID IN report on EP2 IN. Same return
 * convention as the joystick variant. */
uint8_t USBD_FreeJoy_SendCfgReport(USBD_HandleTypeDef *pdev,
                                   uint8_t *report, uint16_t len);

/* Re-arm EP2 OUT to receive the next configurator OUT report. Mirrors
 * `USBD_CUSTOM_HID_ReceivePacket` from Cube's CustomHID. The composite
 * class re-arms internally after every successful DataOut, so callers
 * normally don't need this -- it stays exported only for parity with the
 * pre-Phase-4F shape that called it from `FreeJoy_HID_OutEvent`. */
uint8_t USBD_FreeJoy_ReceivePacket(USBD_HandleTypeDef *pdev);

/* Send a CDC bulk packet on EP3 IN. Returns -1 on success (queued), 0
 * on BUSY -- F103's CDC_Send_DATA inverted-return convention preserved
 * because callers (application/Src/simhub.c::WriteLine) ignore the
 * return value but the convention is documented in F103's usb_endp.c.
 * Length must be < FREEJOY_CDC_DATA_SIZE (64) -- packets are not
 * fragmented at this layer. */
int8_t USBD_FreeJoy_SendCdcData(USBD_HandleTypeDef *pdev,
                                uint8_t *data, uint8_t len);

/* Re-arm EP3 OUT receive if the SimHub ring buffer has room. The
 * composite class re-arms inline from DataOut whenever space allows;
 * this exported helper is for the polled path (simhub.c calls
 * SH_ProcessEndpData from the main loop while it waits for bytes,
 * which lets us recover after the ring drains via SH_Read). */
void USBD_FreeJoy_RearmCdcReceive(USBD_HandleTypeDef *pdev);

/* OutEvent for the configurator interface, defined in usbd_freejoy_if.c.
 * Called from the composite class's DataOut path with the 64-byte EP2
 * OUT buffer. report_buffer[0] is the report ID. */
void FreeJoy_CfgOutEvent(uint8_t *report_buffer);

/* Per-interface report descriptors, defined in usbd_freejoy_if.c. */
extern uint8_t FreeJoy_JoyReportDesc[FREEJOY_JOY_REPORT_DESC_SIZE];
extern uint8_t FreeJoy_CfgReportDesc[FREEJOY_CFG_REPORT_DESC_SIZE];

#ifdef __cplusplus
}
#endif

#endif /* __USBD_FREEJOY_CLASS_H */
