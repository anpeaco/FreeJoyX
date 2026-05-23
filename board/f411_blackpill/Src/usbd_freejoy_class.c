/**
  ******************************************************************************
  * @file    usbd_freejoy_class.c
  * @brief   F411 dual-HID composite class — Phase 4F implementation.
  *
  * Routes Setup / DataIn / DataOut by interface number and endpoint
  * across two HID interfaces:
  *
  *   Interface 0 — Joystick HID (EP1 IN)
  *   Interface 1 — Configurator HID (EP2 IN, EP2 OUT)
  *
  * Modeled on Cube's `Class/CustomHID/Src/usbd_customhid.c` but with
  * per-interface state and a combined config descriptor. Matches F103's
  * dual-HID layout (application/Src/usb_desc.c) so usbccgp.sys binds and
  * Windows refreshes per-interface friendly names on every enumeration.
  ******************************************************************************
  */

#include "usbd_freejoy_class.h"
#include "usbd_ctlreq.h"
#include "usbd_core.h"

#include <string.h>

/* Active size of the joystick HID report descriptor. Set by
 * USBD_FreeJoy_SetJoyReportDescSize() at USB init from BuildJoyReportDesc's
 * return value; mirrored into the composite config descriptor's
 * wDescriptorLength bytes and the standalone joystick HID class descriptor.
 * Initialised to the compile-time max so the bootloader build (which leaves
 * the descriptor in its static full-shape form) still works without calling
 * the setter. */
static uint16_t joy_report_desc_size = FREEJOY_JOY_REPORT_DESC_SIZE;

/*============================================================================
 *  Combined configuration descriptor (66 bytes)
 *
 *  Layout matches F103's first two interfaces in
 *  application/Src/usb_desc.c::Composite_ConfigDescriptor[]:
 *    9 bytes   Configuration header (bNumInterfaces=2, bus-powered, 100mA)
 *    9 bytes   Interface 0 — Joystick HID, 1 endpoint
 *    9 bytes   HID class descriptor (joy report descriptor pointer)
 *    7 bytes   Endpoint 1 IN, interrupt, 64 bytes, 1 ms
 *    9 bytes   Interface 1 — Configurator HID, 2 endpoints
 *    9 bytes   HID class descriptor (configurator report descriptor pointer)
 *    7 bytes   Endpoint 2 IN, interrupt, 64 bytes, 2 ms
 *    7 bytes   Endpoint 2 OUT, interrupt, 64 bytes, 16 ms
 *============================================================================*/
#define USBD_FREEJOY_CFG_DESC_SIZ  66U

__ALIGN_BEGIN static uint8_t USBD_FreeJoy_CfgDesc[USBD_FREEJOY_CFG_DESC_SIZ] __ALIGN_END = {
	/* Configuration descriptor */
	0x09,                                  /* bLength */
	USB_DESC_TYPE_CONFIGURATION,           /* bDescriptorType */
	LOBYTE(USBD_FREEJOY_CFG_DESC_SIZ),     /* wTotalLength LSB */
	HIBYTE(USBD_FREEJOY_CFG_DESC_SIZ),     /* wTotalLength MSB */
	0x02,                                  /* bNumInterfaces */
	0x01,                                  /* bConfigurationValue */
	0x00,                                  /* iConfiguration */
	0x80,                                  /* bmAttributes: bus-powered */
	0x32,                                  /* MaxPower 100 mA */

	/*--- Interface 0: Joystick HID, 1 IN endpoint ---*/
	0x09,                                  /* bLength */
	USB_DESC_TYPE_INTERFACE,               /* bDescriptorType */
	0x00,                                  /* bInterfaceNumber */
	0x00,                                  /* bAlternateSetting */
	0x01,                                  /* bNumEndpoints */
	0x03,                                  /* bInterfaceClass: HID */
	0x00,                                  /* bInterfaceSubClass: no boot */
	0x00,                                  /* bInterfaceProtocol: none */
	0x00,                                  /* iInterface (match F103: 0) */

	/* HID class descriptor for interface 0 */
	0x09,                                  /* bLength */
	HID_DESCRIPTOR_TYPE,                   /* bDescriptorType: HID */
	0x10, 0x01,                            /* bcdHID 1.10 */
	0x00,                                  /* bCountryCode */
	0x01,                                  /* bNumDescriptors */
	HID_REPORT_DESCRIPTOR_TYPE,            /* bDescriptorType: Report */
	LOBYTE(FREEJOY_JOY_REPORT_DESC_SIZE),  /* wDescriptorLength LSB */
	HIBYTE(FREEJOY_JOY_REPORT_DESC_SIZE),  /* wDescriptorLength MSB
	                                        * (note: F103 has 0 here — that
	                                        *  was a legacy bug; setting it
	                                        *  correctly here is strictly
	                                        *  more compliant) */

	/* Endpoint 1 IN — joystick reports, interrupt, 64 bytes, 1 ms */
	0x07,                                  /* bLength */
	USB_DESC_TYPE_ENDPOINT,                /* bDescriptorType */
	FREEJOY_JOY_EPIN_ADDR,                 /* bEndpointAddress: 0x81 */
	0x03,                                  /* bmAttributes: interrupt */
	LOBYTE(FREEJOY_JOY_EPIN_SIZE),         /* wMaxPacketSize LSB */
	HIBYTE(FREEJOY_JOY_EPIN_SIZE),         /* wMaxPacketSize MSB */
	FREEJOY_JOY_FS_BINTERVAL,              /* bInterval: 1 ms */

	/*--- Interface 1: Configurator HID, IN + OUT endpoints ---*/
	0x09,                                  /* bLength */
	USB_DESC_TYPE_INTERFACE,               /* bDescriptorType */
	0x01,                                  /* bInterfaceNumber */
	0x00,                                  /* bAlternateSetting */
	0x02,                                  /* bNumEndpoints */
	0x03,                                  /* bInterfaceClass: HID */
	0x00,                                  /* bInterfaceSubClass */
	0x00,                                  /* bInterfaceProtocol */
	0x00,                                  /* iInterface */

	/* HID class descriptor for interface 1 */
	0x09,                                  /* bLength */
	HID_DESCRIPTOR_TYPE,                   /* bDescriptorType: HID */
	0x10, 0x01,                            /* bcdHID 1.10 */
	0x00,                                  /* bCountryCode */
	0x01,                                  /* bNumDescriptors */
	HID_REPORT_DESCRIPTOR_TYPE,            /* bDescriptorType: Report */
	LOBYTE(FREEJOY_CFG_REPORT_DESC_SIZE),  /* wDescriptorLength LSB */
	HIBYTE(FREEJOY_CFG_REPORT_DESC_SIZE),  /* wDescriptorLength MSB */

	/* Endpoint 2 IN — configurator IN reports, interrupt, 64 bytes, 2 ms */
	0x07,                                  /* bLength */
	USB_DESC_TYPE_ENDPOINT,                /* bDescriptorType */
	FREEJOY_CFG_EPIN_ADDR,                 /* bEndpointAddress: 0x82 */
	0x03,                                  /* bmAttributes: interrupt */
	LOBYTE(FREEJOY_CFG_EPIN_SIZE),         /* wMaxPacketSize LSB */
	HIBYTE(FREEJOY_CFG_EPIN_SIZE),         /* wMaxPacketSize MSB */
	FREEJOY_CFG_EPIN_FS_BINTERVAL,         /* bInterval: 2 ms */

	/* Endpoint 2 OUT — configurator OUT reports, interrupt, 64 bytes, 16 ms */
	0x07,                                  /* bLength */
	USB_DESC_TYPE_ENDPOINT,                /* bDescriptorType */
	FREEJOY_CFG_EPOUT_ADDR,                /* bEndpointAddress: 0x02 */
	0x03,                                  /* bmAttributes: interrupt */
	LOBYTE(FREEJOY_CFG_EPOUT_SIZE),        /* wMaxPacketSize LSB */
	HIBYTE(FREEJOY_CFG_EPOUT_SIZE),        /* wMaxPacketSize MSB */
	FREEJOY_CFG_EPOUT_FS_BINTERVAL,        /* bInterval: 16 ms */
};

/* Standard HID class descriptor reused for GET_DESCRIPTOR (DescriptorType
 * 0x21) responses. Two variants — joystick and configurator — keyed on
 * which interface the request targets. Layout matches the embedded
 * descriptors above byte-for-byte except this is the standalone form. */
__ALIGN_BEGIN static uint8_t USBD_FreeJoy_JoyHidDesc[9] __ALIGN_END = {
	0x09, HID_DESCRIPTOR_TYPE, 0x10, 0x01, 0x00, 0x01,
	HID_REPORT_DESCRIPTOR_TYPE,
	LOBYTE(FREEJOY_JOY_REPORT_DESC_SIZE),
	HIBYTE(FREEJOY_JOY_REPORT_DESC_SIZE),
};

__ALIGN_BEGIN static uint8_t USBD_FreeJoy_CfgHidDesc[9] __ALIGN_END = {
	0x09, HID_DESCRIPTOR_TYPE, 0x10, 0x01, 0x00, 0x01,
	HID_REPORT_DESCRIPTOR_TYPE,
	LOBYTE(FREEJOY_CFG_REPORT_DESC_SIZE),
	HIBYTE(FREEJOY_CFG_REPORT_DESC_SIZE),
};

/* Device qualifier — full-speed device, never used at this speed but
 * USBD core asks for it. */
__ALIGN_BEGIN static uint8_t USBD_FreeJoy_DeviceQualifierDesc[USB_LEN_DEV_QUALIFIER_DESC] __ALIGN_END = {
	USB_LEN_DEV_QUALIFIER_DESC,
	USB_DESC_TYPE_DEVICE_QUALIFIER,
	0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00,
};

/*============================================================================
 *  Class vtable (forward declarations of the eight callback methods)
 *==========================================================================*/
static uint8_t  USBD_FreeJoy_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  USBD_FreeJoy_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx);
static uint8_t  USBD_FreeJoy_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req);
static uint8_t  USBD_FreeJoy_EP0_RxReady(USBD_HandleTypeDef *pdev);
static uint8_t  USBD_FreeJoy_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t  USBD_FreeJoy_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum);
static uint8_t *USBD_FreeJoy_GetFSCfgDesc(uint16_t *length);
static uint8_t *USBD_FreeJoy_GetHSCfgDesc(uint16_t *length);
static uint8_t *USBD_FreeJoy_GetOtherSpeedCfgDesc(uint16_t *length);
static uint8_t *USBD_FreeJoy_GetDeviceQualifierDesc(uint16_t *length);

USBD_ClassTypeDef USBD_FreeJoy_CompositeClass = {
	USBD_FreeJoy_Init,
	USBD_FreeJoy_DeInit,
	USBD_FreeJoy_Setup,
	NULL,                          /* EP0_TxSent */
	USBD_FreeJoy_EP0_RxReady,      /* EP0_RxReady (SET_REPORT data stage) */
	USBD_FreeJoy_DataIn,
	USBD_FreeJoy_DataOut,
	NULL,                          /* SOF */
	NULL,
	NULL,
	USBD_FreeJoy_GetHSCfgDesc,
	USBD_FreeJoy_GetFSCfgDesc,
	USBD_FreeJoy_GetOtherSpeedCfgDesc,
	USBD_FreeJoy_GetDeviceQualifierDesc,
};

/*============================================================================
 *  Init / DeInit — open and close all three endpoints
 *==========================================================================*/
static uint8_t USBD_FreeJoy_Init(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
	(void)cfgidx;

	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)USBD_malloc(sizeof(USBD_FreeJoy_HandleTypeDef));
	if (h == NULL) {
		pdev->pClassDataCmsit[pdev->classId] = NULL;
		return (uint8_t)USBD_EMEM;
	}
	memset(h, 0, sizeof(*h));

	pdev->pClassDataCmsit[pdev->classId] = (void *)h;
	pdev->pClassData = pdev->pClassDataCmsit[pdev->classId];

	/* bIntervals are full-speed; F411 never enumerates at high speed. */
	pdev->ep_in[FREEJOY_JOY_EPIN_ADDR & 0xFU].bInterval   = FREEJOY_JOY_FS_BINTERVAL;
	pdev->ep_in[FREEJOY_CFG_EPIN_ADDR & 0xFU].bInterval   = FREEJOY_CFG_EPIN_FS_BINTERVAL;
	pdev->ep_out[FREEJOY_CFG_EPOUT_ADDR & 0xFU].bInterval = FREEJOY_CFG_EPOUT_FS_BINTERVAL;

	(void)USBD_LL_OpenEP(pdev, FREEJOY_JOY_EPIN_ADDR,   USBD_EP_TYPE_INTR, FREEJOY_JOY_EPIN_SIZE);
	(void)USBD_LL_OpenEP(pdev, FREEJOY_CFG_EPIN_ADDR,   USBD_EP_TYPE_INTR, FREEJOY_CFG_EPIN_SIZE);
	(void)USBD_LL_OpenEP(pdev, FREEJOY_CFG_EPOUT_ADDR,  USBD_EP_TYPE_INTR, FREEJOY_CFG_EPOUT_SIZE);

	pdev->ep_in[FREEJOY_JOY_EPIN_ADDR & 0xFU].is_used   = 1U;
	pdev->ep_in[FREEJOY_CFG_EPIN_ADDR & 0xFU].is_used   = 1U;
	pdev->ep_out[FREEJOY_CFG_EPOUT_ADDR & 0xFU].is_used = 1U;

	h->joy_state = FREEJOY_HID_IDLE;
	h->cfg_state = FREEJOY_HID_IDLE;

	/* Pre-arm EP2 OUT so the host can queue the first OUT report. The
	 * configurator's first interaction is normally Read Config (REPORT_ID 4
	 * OUT) which fails silently if the EP isn't armed yet. */
	(void)USBD_LL_PrepareReceive(pdev, FREEJOY_CFG_EPOUT_ADDR,
	                             h->ep2_out_buf, FREEJOY_OUTREPORT_BUF_SIZE);

	return (uint8_t)USBD_OK;
}

static uint8_t USBD_FreeJoy_DeInit(USBD_HandleTypeDef *pdev, uint8_t cfgidx)
{
	(void)cfgidx;

	(void)USBD_LL_CloseEP(pdev, FREEJOY_JOY_EPIN_ADDR);
	pdev->ep_in[FREEJOY_JOY_EPIN_ADDR & 0xFU].is_used   = 0U;
	pdev->ep_in[FREEJOY_JOY_EPIN_ADDR & 0xFU].bInterval = 0U;

	(void)USBD_LL_CloseEP(pdev, FREEJOY_CFG_EPIN_ADDR);
	pdev->ep_in[FREEJOY_CFG_EPIN_ADDR & 0xFU].is_used   = 0U;
	pdev->ep_in[FREEJOY_CFG_EPIN_ADDR & 0xFU].bInterval = 0U;

	(void)USBD_LL_CloseEP(pdev, FREEJOY_CFG_EPOUT_ADDR);
	pdev->ep_out[FREEJOY_CFG_EPOUT_ADDR & 0xFU].is_used   = 0U;
	pdev->ep_out[FREEJOY_CFG_EPOUT_ADDR & 0xFU].bInterval = 0U;

	if (pdev->pClassDataCmsit[pdev->classId] != NULL) {
		USBD_free(pdev->pClassDataCmsit[pdev->classId]);
		pdev->pClassDataCmsit[pdev->classId] = NULL;
		pdev->pClassData = NULL;
	}

	return (uint8_t)USBD_OK;
}

/*============================================================================
 *  Setup — class + standard requests, dispatched per interface
 *
 *  bmRequestType.recipient = Interface; req->wIndex (low byte) = interface
 *  number. We branch by interface to look up the correct per-interface
 *  state and report descriptor.
 *==========================================================================*/
static uint8_t USBD_FreeJoy_Setup(USBD_HandleTypeDef *pdev, USBD_SetupReqTypedef *req)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return (uint8_t)USBD_FAIL;

	const uint8_t iface = (uint8_t)(req->wIndex & 0xFFU);

	uint8_t        *protocol      = (iface == 0U) ? &h->joy_protocol      : &h->cfg_protocol;
	uint8_t        *idle_state    = (iface == 0U) ? &h->joy_idle_state    : &h->cfg_idle_state;
	uint8_t        *alt_setting   = (iface == 0U) ? &h->joy_alt_setting   : &h->cfg_alt_setting;
	uint8_t        *report_desc   = (iface == 0U) ? FreeJoy_JoyReportDesc : FreeJoy_CfgReportDesc;
	uint16_t        report_size   = (iface == 0U) ? joy_report_desc_size
	                                              : FREEJOY_CFG_REPORT_DESC_SIZE;
	uint8_t        *hid_desc      = (iface == 0U) ? USBD_FreeJoy_JoyHidDesc
	                                              : USBD_FreeJoy_CfgHidDesc;

	uint16_t          status_info = 0U;
	uint16_t          len         = 0U;
	uint8_t          *pbuf        = NULL;
	USBD_StatusTypeDef ret        = USBD_OK;

	switch (req->bmRequest & USB_REQ_TYPE_MASK) {
	case USB_REQ_TYPE_CLASS:
		switch (req->bRequest) {
		case HID_REQ_SET_PROTOCOL:
			*protocol = (uint8_t)req->wValue;
			break;

		case HID_REQ_GET_PROTOCOL:
			(void)USBD_CtlSendData(pdev, protocol, 1U);
			break;

		case HID_REQ_SET_IDLE:
			*idle_state = (uint8_t)(req->wValue >> 8);
			break;

		case HID_REQ_GET_IDLE:
			(void)USBD_CtlSendData(pdev, idle_state, 1U);
			break;

		case HID_REQ_SET_REPORT:
			/* Stage SET_REPORT for EP0 RxReady. The data arrives next
			 * via EP0_RxReady; we route it to the configurator
			 * OutEvent because our protocol only sends OUT reports
			 * to interface 1. (A SET_REPORT on the joystick interface
			 * would be unusual; we still accept it but discard.) */
			if (req->wLength > FREEJOY_OUTREPORT_BUF_SIZE) {
				USBD_CtlError(pdev, req);
				return (uint8_t)USBD_FAIL;
			}
			h->set_report_pending      = 1U;
			h->set_report_target_iface = iface;
			(void)USBD_CtlPrepareRx(pdev, h->ep2_out_buf, req->wLength);
			break;

		default:
			USBD_CtlError(pdev, req);
			ret = USBD_FAIL;
			break;
		}
		break;

	case USB_REQ_TYPE_STANDARD:
		switch (req->bRequest) {
		case USB_REQ_GET_STATUS:
			if (pdev->dev_state == USBD_STATE_CONFIGURED) {
				(void)USBD_CtlSendData(pdev, (uint8_t *)&status_info, 2U);
			} else {
				USBD_CtlError(pdev, req);
				ret = USBD_FAIL;
			}
			break;

		case USB_REQ_GET_DESCRIPTOR: {
			const uint8_t desc_type = (uint8_t)(req->wValue >> 8);
			if (desc_type == HID_REPORT_DESCRIPTOR_TYPE) {
				len  = (req->wLength < report_size) ? req->wLength : report_size;
				pbuf = report_desc;
			} else if (desc_type == HID_DESCRIPTOR_TYPE) {
				len  = (req->wLength < 9U) ? req->wLength : 9U;
				pbuf = hid_desc;
			}
			if (pbuf != NULL) {
				(void)USBD_CtlSendData(pdev, pbuf, len);
			} else {
				USBD_CtlError(pdev, req);
				ret = USBD_FAIL;
			}
			break;
		}

		case USB_REQ_GET_INTERFACE:
			if (pdev->dev_state == USBD_STATE_CONFIGURED) {
				(void)USBD_CtlSendData(pdev, alt_setting, 1U);
			} else {
				USBD_CtlError(pdev, req);
				ret = USBD_FAIL;
			}
			break;

		case USB_REQ_SET_INTERFACE:
			if (pdev->dev_state == USBD_STATE_CONFIGURED) {
				*alt_setting = (uint8_t)req->wValue;
			} else {
				USBD_CtlError(pdev, req);
				ret = USBD_FAIL;
			}
			break;

		case USB_REQ_CLEAR_FEATURE:
			break;

		default:
			USBD_CtlError(pdev, req);
			ret = USBD_FAIL;
			break;
		}
		break;

	default:
		USBD_CtlError(pdev, req);
		ret = USBD_FAIL;
		break;
	}

	return (uint8_t)ret;
}

/*============================================================================
 *  EP0_RxReady — SET_REPORT data has arrived on EP0
 *
 *  The 1-byte / few-byte payloads our protocol sends via SET_REPORT (e.g.
 *  the firmware-update trigger string on REPORT_ID 5) end up here. Route
 *  to the configurator OutEvent if that's the target interface.
 *==========================================================================*/
static uint8_t USBD_FreeJoy_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return (uint8_t)USBD_FAIL;

	if (h->set_report_pending) {
		h->set_report_pending = 0U;
		if (h->set_report_target_iface == 1U) {
			FreeJoy_CfgOutEvent(h->ep2_out_buf);
		}
		/* iface 0 (joystick) SET_REPORT: no app-side handler, drop. */
	}
	return (uint8_t)USBD_OK;
}

/*============================================================================
 *  DataIn — IN endpoint transfer complete; clear per-interface busy flag
 *==========================================================================*/
static uint8_t USBD_FreeJoy_DataIn(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return (uint8_t)USBD_FAIL;

	const uint8_t ep = (uint8_t)(epnum & 0x0FU);
	if (ep == (FREEJOY_JOY_EPIN_ADDR & 0x0FU)) {
		h->joy_state = FREEJOY_HID_IDLE;
	} else if (ep == (FREEJOY_CFG_EPIN_ADDR & 0x0FU)) {
		h->cfg_state = FREEJOY_HID_IDLE;
	}
	return (uint8_t)USBD_OK;
}

/*============================================================================
 *  DataOut — OUT report received on EP2 OUT; dispatch + re-arm
 *==========================================================================*/
static uint8_t USBD_FreeJoy_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return (uint8_t)USBD_FAIL;

	if ((epnum & 0x0FU) == (FREEJOY_CFG_EPOUT_ADDR & 0x0FU)) {
		FreeJoy_CfgOutEvent(h->ep2_out_buf);
		/* Re-arm immediately. Cube's CustomHID class made callers do
		 * this manually; we own the dispatcher so we always re-arm. */
		(void)USBD_LL_PrepareReceive(pdev, FREEJOY_CFG_EPOUT_ADDR,
		                             h->ep2_out_buf, FREEJOY_OUTREPORT_BUF_SIZE);
	}
	return (uint8_t)USBD_OK;
}

/*============================================================================
 *  Config descriptor accessors (USBD core asks for them by speed).
 *  All three speeds return the same FS descriptor — F411 is FS-only.
 *==========================================================================*/
static uint8_t *USBD_FreeJoy_GetFSCfgDesc(uint16_t *length)
{
	*length = (uint16_t)sizeof(USBD_FreeJoy_CfgDesc);
	return USBD_FreeJoy_CfgDesc;
}

static uint8_t *USBD_FreeJoy_GetHSCfgDesc(uint16_t *length)
{
	*length = (uint16_t)sizeof(USBD_FreeJoy_CfgDesc);
	return USBD_FreeJoy_CfgDesc;
}

static uint8_t *USBD_FreeJoy_GetOtherSpeedCfgDesc(uint16_t *length)
{
	*length = (uint16_t)sizeof(USBD_FreeJoy_CfgDesc);
	return USBD_FreeJoy_CfgDesc;
}

static uint8_t *USBD_FreeJoy_GetDeviceQualifierDesc(uint16_t *length)
{
	*length = (uint16_t)sizeof(USBD_FreeJoy_DeviceQualifierDesc);
	return USBD_FreeJoy_DeviceQualifierDesc;
}

/*============================================================================
 *  Public send-report helpers (called from board_usb.c)
 *==========================================================================*/
uint8_t USBD_FreeJoy_SendJoyReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return (uint8_t)USBD_FAIL;
	if (pdev->dev_state != USBD_STATE_CONFIGURED) return (uint8_t)USBD_FAIL;

	if (h->joy_state != FREEJOY_HID_IDLE) return (uint8_t)USBD_BUSY;
	h->joy_state = FREEJOY_HID_BUSY;
	(void)USBD_LL_Transmit(pdev, FREEJOY_JOY_EPIN_ADDR, report, len);
	return (uint8_t)USBD_OK;
}

uint8_t USBD_FreeJoy_SendCfgReport(USBD_HandleTypeDef *pdev, uint8_t *report, uint16_t len)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return (uint8_t)USBD_FAIL;
	if (pdev->dev_state != USBD_STATE_CONFIGURED) return (uint8_t)USBD_FAIL;

	if (h->cfg_state != FREEJOY_HID_IDLE) return (uint8_t)USBD_BUSY;
	h->cfg_state = FREEJOY_HID_BUSY;
	(void)USBD_LL_Transmit(pdev, FREEJOY_CFG_EPIN_ADDR, report, len);
	return (uint8_t)USBD_OK;
}

uint8_t USBD_FreeJoy_ReceivePacket(USBD_HandleTypeDef *pdev)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return (uint8_t)USBD_FAIL;

	(void)USBD_LL_PrepareReceive(pdev, FREEJOY_CFG_EPOUT_ADDR,
	                             h->ep2_out_buf, FREEJOY_OUTREPORT_BUF_SIZE);
	return (uint8_t)USBD_OK;
}

/* Composite-descriptor offsets for the joystick HID class descriptor's
 * wDescriptorLength field. See USBD_FreeJoy_CfgDesc layout: 9-byte config
 * header + 9-byte interface-0 descriptor puts the HID class descriptor at
 * offset 18, and wDescriptorLength lives at offsets 25 (LSB) / 26 (MSB)
 * within that block. */
#define FREEJOY_CFGDESC_JOY_HID_WDESC_LSB   25U
#define FREEJOY_CFGDESC_JOY_HID_WDESC_MSB   26U

/* Standalone HID class descriptor layout: wDescriptorLength sits at
 * offsets 7 (LSB) / 8 (MSB) of the 9-byte block. */
#define FREEJOY_JOY_HID_DESC_WDESC_LSB      7U
#define FREEJOY_JOY_HID_DESC_WDESC_MSB      8U

void USBD_FreeJoy_SetJoyReportDescSize(uint16_t size)
{
	if (size == 0U || size > FREEJOY_JOY_REPORT_DESC_SIZE) return;

	joy_report_desc_size = size;

	USBD_FreeJoy_CfgDesc[FREEJOY_CFGDESC_JOY_HID_WDESC_LSB] = LOBYTE(size);
	USBD_FreeJoy_CfgDesc[FREEJOY_CFGDESC_JOY_HID_WDESC_MSB] = HIBYTE(size);

	USBD_FreeJoy_JoyHidDesc[FREEJOY_JOY_HID_DESC_WDESC_LSB] = LOBYTE(size);
	USBD_FreeJoy_JoyHidDesc[FREEJOY_JOY_HID_DESC_WDESC_MSB] = HIBYTE(size);
}
