/**
  ******************************************************************************
  * @file    usbd_freejoy_class.c
  * @brief   F411 HID+HID+CDC composite class — Phases 4F + 4E.
  *
  * Routes Setup / DataIn / DataOut by interface number and endpoint
  * across four interfaces:
  *
  *   Interface 0 — Joystick HID (EP1 IN)
  *   Interface 1 — Configurator HID (EP2 IN, EP2 OUT)
  *   Interface 2 — CDC Communication (no endpoints; notification omitted
  *                                    per Phase 4E plan, Option A)
  *   Interface 3 — CDC Data (EP3 IN bulk, EP3 OUT bulk)
  *
  * Modeled on Cube's `Class/CustomHID/Src/usbd_customhid.c` (HID parts)
  * and F103's hand-rolled USB-FS-Device CDC handlers in
  * `application/Src/usb_endp.c` and `application/Src/usb_prop.c` (CDC
  * parts) but unified into one composite class. Matches F103's 4-interface
  * layout (application/Src/usb_desc.c::Composite_ConfigDescriptor) byte-
  * for-byte except the CDC notification endpoint is dropped (F411 OTG-FS
  * only has 4 IN EP slots, all four taken by EP0 ctrl + Joy + Cfg +
  * CDC bulk; see Phase 4E plan).
  ******************************************************************************
  */

#include "usbd_freejoy_class.h"
#include "usbd_ctlreq.h"
#include "usbd_core.h"

#include "simhub.h"          /* SH_ProcessIncomingData, SH_BufferFreeSize */

#include <string.h>

/*============================================================================
 *  Combined configuration descriptor (125 bytes — Phase 4E)
 *
 *  Layout matches F103's four-interface layout in
 *  application/Src/usb_desc.c::Composite_ConfigDescriptor[] EXCEPT:
 *    - Interface 2 (CDC Communication) declares bNumEndpoints = 0 and
 *      omits the interrupt-IN notification endpoint. F103 declares it
 *      on EP3 IN (8 bytes); F411 OTG-FS doesn't have a 5th IN slot
 *      to spare. SimHub never reads notifications anyway -- they're
 *      modem-state changes (DCD, ring, framing errors) that don't apply
 *      to a virtual COM on a USB joystick. Linux cdc-acm + Windows
 *      usbser.sys + macOS IOUSBHost all bind the device fine without
 *      it. (Phase 4E plan, Option A.)
 *
 *  Byte breakdown:
 *     9 bytes   Configuration header (bNumInterfaces=4, bus-powered, 100 mA)
 *     9 bytes   Interface 0 — Joystick HID, 1 endpoint
 *     9 bytes   HID class descriptor (joy report descriptor pointer)
 *     7 bytes   Endpoint 1 IN, interrupt, 64 bytes, 1 ms
 *     9 bytes   Interface 1 — Configurator HID, 2 endpoints
 *     9 bytes   HID class descriptor (configurator report descriptor pointer)
 *     7 bytes   Endpoint 2 IN, interrupt, 64 bytes, 2 ms
 *     7 bytes   Endpoint 2 OUT, interrupt, 64 bytes, 16 ms
 *     8 bytes   IAD: associates interfaces 2-3 as one CDC function
 *     9 bytes   Interface 2 — CDC Communication, 0 endpoints (Option A)
 *     5 bytes   CDC Header functional descriptor
 *     5 bytes   CDC Call Management functional descriptor
 *     4 bytes   CDC ACM functional descriptor
 *     5 bytes   CDC Union functional descriptor
 *     9 bytes   Interface 3 — CDC Data, 2 bulk endpoints
 *     7 bytes   Endpoint 3 IN, bulk, 64 bytes
 *     7 bytes   Endpoint 3 OUT, bulk, 64 bytes
 *  ---------
 *   125 bytes total.
 *============================================================================*/
#define USBD_FREEJOY_CFG_DESC_SIZ  125U

__ALIGN_BEGIN static uint8_t USBD_FreeJoy_CfgDesc[USBD_FREEJOY_CFG_DESC_SIZ] __ALIGN_END = {
	/* Configuration descriptor */
	0x09,                                  /* bLength */
	USB_DESC_TYPE_CONFIGURATION,           /* bDescriptorType */
	LOBYTE(USBD_FREEJOY_CFG_DESC_SIZ),     /* wTotalLength LSB */
	HIBYTE(USBD_FREEJOY_CFG_DESC_SIZ),     /* wTotalLength MSB */
	0x04,                                  /* bNumInterfaces */
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

	/*--- IAD: Interface Association Descriptor ---
	 * Tells the host that interfaces 2-3 are one CDC function so
	 * Windows binds usbser.sys to them as a single COM port. Without
	 * the IAD, Windows would treat the two CDC interfaces as
	 * separate functions and the device wouldn't enumerate as a COM
	 * port at all. */
	0x08,                                  /* bLength */
	0x0B,                                  /* bDescriptorType: IAD */
	0x02,                                  /* bFirstInterface */
	0x02,                                  /* bInterfaceCount */
	0x02,                                  /* bFunctionClass: CDC */
	0x02,                                  /* bFunctionSubClass: ACM */
	0x01,                                  /* bFunctionProtocol: V.250 AT */
	0x00,                                  /* iFunction */

	/*--- Interface 2: CDC Communication Class, 0 endpoints (Option A) ---*/
	0x09,                                  /* bLength */
	USB_DESC_TYPE_INTERFACE,               /* bDescriptorType */
	0x02,                                  /* bInterfaceNumber */
	0x00,                                  /* bAlternateSetting */
	0x00,                                  /* bNumEndpoints (no notification EP) */
	0x02,                                  /* bInterfaceClass: CDC */
	0x02,                                  /* bInterfaceSubClass: ACM */
	0x01,                                  /* bInterfaceProtocol: V.250 AT */
	0x00,                                  /* iInterface */

	/* CDC Header functional descriptor */
	0x05,                                  /* bFunctionLength */
	0x24,                                  /* bDescriptorType: CS_INTERFACE */
	0x00,                                  /* bDescriptorSubtype: Header */
	0x10, 0x01,                            /* bcdCDC: 1.10 */

	/* CDC Call Management functional descriptor */
	0x05,                                  /* bFunctionLength */
	0x24,                                  /* bDescriptorType: CS_INTERFACE */
	0x01,                                  /* bDescriptorSubtype: Call Mgmt */
	0x00,                                  /* bmCapabilities: D0+D1=0
	                                        *   (we don't handle call mgmt) */
	0x03,                                  /* bDataInterface: 3 */

	/* CDC ACM functional descriptor */
	0x04,                                  /* bFunctionLength */
	0x24,                                  /* bDescriptorType: CS_INTERFACE */
	0x02,                                  /* bDescriptorSubtype: ACM */
	0x02,                                  /* bmCapabilities: supports
	                                        *   Set/Get/Set_Line_Coding +
	                                        *   Set_Control_Line_State +
	                                        *   Serial_State (matches F103) */

	/* CDC Union functional descriptor */
	0x05,                                  /* bFunctionLength */
	0x24,                                  /* bDescriptorType: CS_INTERFACE */
	0x06,                                  /* bDescriptorSubtype: Union */
	0x02,                                  /* bMasterInterface: 2 */
	0x03,                                  /* bSlaveInterface0: 3 */

	/*--- Interface 3: CDC Data Class, 2 bulk endpoints ---*/
	0x09,                                  /* bLength */
	USB_DESC_TYPE_INTERFACE,               /* bDescriptorType */
	0x03,                                  /* bInterfaceNumber */
	0x00,                                  /* bAlternateSetting */
	0x02,                                  /* bNumEndpoints */
	0x0A,                                  /* bInterfaceClass: CDC Data */
	0x00,                                  /* bInterfaceSubClass */
	0x00,                                  /* bInterfaceProtocol */
	0x00,                                  /* iInterface */

	/* Endpoint 3 IN — CDC bulk, device → host (SimHub TX) */
	0x07,                                  /* bLength */
	USB_DESC_TYPE_ENDPOINT,                /* bDescriptorType */
	FREEJOY_CDC_DATA_EPIN_ADDR,            /* bEndpointAddress: 0x83 */
	0x02,                                  /* bmAttributes: bulk */
	LOBYTE(FREEJOY_CDC_DATA_SIZE),         /* wMaxPacketSize LSB */
	HIBYTE(FREEJOY_CDC_DATA_SIZE),         /* wMaxPacketSize MSB */
	0x00,                                  /* bInterval: ignored for bulk */

	/* Endpoint 3 OUT — CDC bulk, host → device (SimHub RX) */
	0x07,                                  /* bLength */
	USB_DESC_TYPE_ENDPOINT,                /* bDescriptorType */
	FREEJOY_CDC_DATA_EPOUT_ADDR,           /* bEndpointAddress: 0x03 */
	0x02,                                  /* bmAttributes: bulk */
	LOBYTE(FREEJOY_CDC_DATA_SIZE),         /* wMaxPacketSize LSB */
	HIBYTE(FREEJOY_CDC_DATA_SIZE),         /* wMaxPacketSize MSB */
	0x00,                                  /* bInterval: ignored for bulk */
};

/* Compile-time pin: any descriptor drift fails the build instead of
 * shipping a malformed config. Mirrors the discipline established for
 * FREEJOY_JOY_REPORT_DESC_SIZE and FREEJOY_CFG_REPORT_DESC_SIZE in
 * usbd_freejoy_if.c. */
_Static_assert(sizeof(USBD_FreeJoy_CfgDesc) == USBD_FREEJOY_CFG_DESC_SIZ,
               "Composite descriptor size drifted from USBD_FREEJOY_CFG_DESC_SIZ");

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

	(void)USBD_LL_OpenEP(pdev, FREEJOY_JOY_EPIN_ADDR,       USBD_EP_TYPE_INTR, FREEJOY_JOY_EPIN_SIZE);
	(void)USBD_LL_OpenEP(pdev, FREEJOY_CFG_EPIN_ADDR,       USBD_EP_TYPE_INTR, FREEJOY_CFG_EPIN_SIZE);
	(void)USBD_LL_OpenEP(pdev, FREEJOY_CFG_EPOUT_ADDR,      USBD_EP_TYPE_INTR, FREEJOY_CFG_EPOUT_SIZE);
	(void)USBD_LL_OpenEP(pdev, FREEJOY_CDC_DATA_EPIN_ADDR,  USBD_EP_TYPE_BULK, FREEJOY_CDC_DATA_SIZE);
	(void)USBD_LL_OpenEP(pdev, FREEJOY_CDC_DATA_EPOUT_ADDR, USBD_EP_TYPE_BULK, FREEJOY_CDC_DATA_SIZE);

	pdev->ep_in[FREEJOY_JOY_EPIN_ADDR & 0xFU].is_used       = 1U;
	pdev->ep_in[FREEJOY_CFG_EPIN_ADDR & 0xFU].is_used       = 1U;
	pdev->ep_out[FREEJOY_CFG_EPOUT_ADDR & 0xFU].is_used     = 1U;
	pdev->ep_in[FREEJOY_CDC_DATA_EPIN_ADDR & 0xFU].is_used  = 1U;
	pdev->ep_out[FREEJOY_CDC_DATA_EPOUT_ADDR & 0xFU].is_used = 1U;

	h->joy_state    = FREEJOY_HID_IDLE;
	h->cfg_state    = FREEJOY_HID_IDLE;
	h->cdc_in_state = FREEJOY_HID_IDLE;
	h->cdc_rx_armed = 0U;

	/* CDC line-coding default: 115200 8N1. Mirrors F103's default in
	 * application/Src/usb_prop.c::CDC_GetLineCoding. The actual baud
	 * rate is meaningless for our virtual COM, but Windows mode-checks
	 * what we report so we keep it consistent. */
	h->cdc_line_coding[0] = 0x00;       /* dwDTERate LSB */
	h->cdc_line_coding[1] = 0xC2;       /* 0x0001C200 = 115200 */
	h->cdc_line_coding[2] = 0x01;
	h->cdc_line_coding[3] = 0x00;       /* dwDTERate MSB */
	h->cdc_line_coding[4] = 0x00;       /* bCharFormat: 1 stop bit */
	h->cdc_line_coding[5] = 0x00;       /* bParityType: none */
	h->cdc_line_coding[6] = 0x08;       /* bDataBits: 8 */

	/* Pre-arm EP2 OUT so the host can queue the first OUT report. The
	 * configurator's first interaction is normally Read Config (REPORT_ID 4
	 * OUT) which fails silently if the EP isn't armed yet. */
	(void)USBD_LL_PrepareReceive(pdev, FREEJOY_CFG_EPOUT_ADDR,
	                             h->ep2_out_buf, FREEJOY_OUTREPORT_BUF_SIZE);

	/* Pre-arm EP3 OUT so SimHub LED-config bytes can land as soon as
	 * the host opens the COM port. Mirrors the F103 EP4_OUT bring-up
	 * in application/Src/usb_prop.c::CustomHID_Reset. */
	(void)USBD_LL_PrepareReceive(pdev, FREEJOY_CDC_DATA_EPOUT_ADDR,
	                             h->cdc_rx_buf, FREEJOY_CDC_DATA_SIZE);
	h->cdc_rx_armed = 1U;

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

	(void)USBD_LL_CloseEP(pdev, FREEJOY_CDC_DATA_EPIN_ADDR);
	pdev->ep_in[FREEJOY_CDC_DATA_EPIN_ADDR & 0xFU].is_used = 0U;

	(void)USBD_LL_CloseEP(pdev, FREEJOY_CDC_DATA_EPOUT_ADDR);
	pdev->ep_out[FREEJOY_CDC_DATA_EPOUT_ADDR & 0xFU].is_used = 0U;

	if (pdev->pClassDataCmsit[pdev->classId] != NULL) {
		USBD_free(pdev->pClassDataCmsit[pdev->classId]);
		pdev->pClassDataCmsit[pdev->classId] = NULL;
		pdev->pClassData = NULL;
	}

	return (uint8_t)USBD_OK;
}

/*============================================================================
 *  CDC SETUP — class + standard requests for interfaces 2 (CDC Comm)
 *  and 3 (CDC Data).
 *
 *  Mirrors F103's CustomHID_Data_Setup CDC paths in
 *  application/Src/usb_prop.c::CustomHID_Data_Setup +
 *  CustomHID_NoData_Setup. The line-coding GET/SET, control-line-state
 *  and send-break are class requests; GET_INTERFACE / SET_INTERFACE /
 *  GET_STATUS are standard.
 *
 *  The CDC Data interface (3) has no class requests of its own and only
 *  needs the standard interface requests; we handle it here for
 *  symmetry rather than dispatching it back into the HID path.
 *==========================================================================*/
static uint8_t USBD_FreeJoy_SetupCdc(USBD_HandleTypeDef *pdev,
                                     USBD_SetupReqTypedef *req,
                                     USBD_FreeJoy_HandleTypeDef *h,
                                     uint8_t iface)
{
	uint16_t           status_info = 0U;
	uint8_t            zero        = 0U;
	USBD_StatusTypeDef ret         = USBD_OK;

	switch (req->bmRequest & USB_REQ_TYPE_MASK) {
	case USB_REQ_TYPE_CLASS:
		if (iface != 2U) {
			USBD_CtlError(pdev, req);
			return (uint8_t)USBD_FAIL;
		}
		switch (req->bRequest) {
		case CDC_GET_LINE_CODING:
			(void)USBD_CtlSendData(pdev, h->cdc_line_coding,
			                       (uint16_t)CDC_LINE_CODING_LEN);
			break;

		case CDC_SET_LINE_CODING:
			/* 7-byte payload follows on EP0; route in EP0_RxReady. */
			if (req->wLength != (uint16_t)CDC_LINE_CODING_LEN) {
				USBD_CtlError(pdev, req);
				return (uint8_t)USBD_FAIL;
			}
			h->set_line_coding_pending = 1U;
			(void)USBD_CtlPrepareRx(pdev, h->cdc_line_coding,
			                        (uint16_t)CDC_LINE_CODING_LEN);
			break;

		case CDC_SET_CONTROL_LINE_STATE:
		case CDC_SEND_BREAK:
			/* No-op: we don't back a real UART. F103 does the same
			 * (application/Src/usb_prop.c::SET_CONTROL_LINE_STATE
			 * returns USB_SUCCESS without doing anything). */
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

		case USB_REQ_GET_INTERFACE:
			/* CDC interfaces only support alt-setting 0. */
			if (pdev->dev_state == USBD_STATE_CONFIGURED) {
				(void)USBD_CtlSendData(pdev, &zero, 1U);
			} else {
				USBD_CtlError(pdev, req);
				ret = USBD_FAIL;
			}
			break;

		case USB_REQ_SET_INTERFACE:
			if (pdev->dev_state != USBD_STATE_CONFIGURED || req->wValue != 0U) {
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

	/* CDC interfaces (2 = CDC Comm, 3 = CDC Data) have a separate
	 * class-request set and no HID descriptors; route them off. */
	if (iface >= 2U) {
		return USBD_FreeJoy_SetupCdc(pdev, req, h, iface);
	}

	uint8_t        *protocol      = (iface == 0U) ? &h->joy_protocol      : &h->cfg_protocol;
	uint8_t        *idle_state    = (iface == 0U) ? &h->joy_idle_state    : &h->cfg_idle_state;
	uint8_t        *alt_setting   = (iface == 0U) ? &h->joy_alt_setting   : &h->cfg_alt_setting;
	uint8_t        *report_desc   = (iface == 0U) ? FreeJoy_JoyReportDesc : FreeJoy_CfgReportDesc;
	uint16_t        report_size   = (iface == 0U) ? FREEJOY_JOY_REPORT_DESC_SIZE
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
 *  EP0_RxReady — SET_REPORT or SET_LINE_CODING data has arrived on EP0
 *
 *  Two staged data-stage receives can land here:
 *    - HID SET_REPORT payload (REPORT_ID 5 firmware trigger, etc.) was
 *      copied into h->ep2_out_buf via USBD_CtlPrepareRx in Setup. Route
 *      to the configurator OutEvent.
 *    - CDC SET_LINE_CODING payload (7 bytes) was copied directly into
 *      h->cdc_line_coding by USBD_CtlPrepareRx in SetupCdc. No-op here
 *      because the buffer is already up to date; we just clear the
 *      pending flag so a subsequent stray RxReady doesn't get routed
 *      to the configurator OutEvent.
 *==========================================================================*/
static uint8_t USBD_FreeJoy_EP0_RxReady(USBD_HandleTypeDef *pdev)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return (uint8_t)USBD_FAIL;

	if (h->set_line_coding_pending) {
		h->set_line_coding_pending = 0U;
		/* Line coding now reflects the host's request. Nothing else to
		 * do -- we don't own a real UART. */
	} else if (h->set_report_pending) {
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
	} else if (ep == (FREEJOY_CDC_DATA_EPIN_ADDR & 0x0FU)) {
		/* Mirrors F103's EP5_IN_Callback in
		 * application/Src/usb_endp.c -- clears the
		 * EP5_PrevXferComplete flag so the next CDC_Send_DATA
		 * call can queue another bulk packet. */
		h->cdc_in_state = FREEJOY_HID_IDLE;
	}
	return (uint8_t)USBD_OK;
}

/*============================================================================
 *  DataOut — OUT report received on EP2 OUT (HID) or EP3 OUT (CDC bulk);
 *  dispatch + re-arm.
 *==========================================================================*/
static uint8_t USBD_FreeJoy_DataOut(USBD_HandleTypeDef *pdev, uint8_t epnum)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return (uint8_t)USBD_FAIL;

	const uint8_t ep = (uint8_t)(epnum & 0x0FU);
	if (ep == (FREEJOY_CFG_EPOUT_ADDR & 0x0FU)) {
		FreeJoy_CfgOutEvent(h->ep2_out_buf);
		/* Re-arm immediately. Cube's CustomHID class made callers do
		 * this manually; we own the dispatcher so we always re-arm. */
		(void)USBD_LL_PrepareReceive(pdev, FREEJOY_CFG_EPOUT_ADDR,
		                             h->ep2_out_buf, FREEJOY_OUTREPORT_BUF_SIZE);
	} else if (ep == (FREEJOY_CDC_DATA_EPOUT_ADDR & 0x0FU)) {
		/* Mirrors F103's EP4_OUT_Callback in
		 * application/Src/usb_endp.c. Push the received bytes into
		 * the SimHub ring buffer and re-arm the receive only if the
		 * ring still has space; otherwise SH_ProcessEndpData (called
		 * from simhub.c::SH_Read on the polled path) re-arms after
		 * the application drains the ring. */
		uint32_t rx_len = USBD_LL_GetRxDataSize(pdev, ep);
		uint16_t free_size = MAX_RING_BIF_SIZE;
		h->cdc_rx_armed = 0U;
		if (rx_len > 0U) {
			free_size = SH_ProcessIncomingData(h->cdc_rx_buf, (uint8_t)rx_len);
		}
		if (free_size > SH_PACKET_SIZE) {
			h->cdc_rx_armed = 1U;
			(void)USBD_LL_PrepareReceive(pdev, FREEJOY_CDC_DATA_EPOUT_ADDR,
			                             h->cdc_rx_buf, FREEJOY_CDC_DATA_SIZE);
		}
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

/*============================================================================
 *  CDC TX -- mirrors F103's CDC_Send_DATA (application/Src/usb_endp.c:220).
 *
 *  Inverted return value preserved (see usbd_freejoy_class.h):
 *    -1  -> queued for transmit
 *     0  -> BUSY (previous packet still in flight, device not configured,
 *           or len >= 64). simhub.c callers ignore the return value but
 *           the convention is documented.
 *
 *  The data MUST be copied into h->cdc_tx_buf rather than handed straight
 *  to USBD_LL_Transmit -- per memory note
 *  project_f411_async_fifo_stack_buffer.md, the OTG-FS PCD reads the
 *  source buffer asynchronously from the TXFE IRQ, so a stack buffer can
 *  go out of scope before the FIFO load completes and the host receives
 *  garbage. The handle struct is statically allocated, so cdc_tx_buf
 *  lives long enough.
 *==========================================================================*/
int8_t USBD_FreeJoy_SendCdcData(USBD_HandleTypeDef *pdev,
                                uint8_t *data, uint8_t len)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return 0;
	if (pdev->dev_state != USBD_STATE_CONFIGURED) return 0;
	if (h->cdc_in_state != FREEJOY_HID_IDLE) return 0;
	if (len >= FREEJOY_CDC_DATA_SIZE) return 0;

	memcpy(h->cdc_tx_buf, data, len);
	h->cdc_in_state = FREEJOY_HID_BUSY;
	(void)USBD_LL_Transmit(pdev, FREEJOY_CDC_DATA_EPIN_ADDR,
	                       h->cdc_tx_buf, len);
	return -1;
}

/*============================================================================
 *  CDC RX re-arm helper -- mirrors F103's SH_ProcessEndpData
 *  (application/Src/usb_endp.c:179). Called from simhub.c::SH_Read on
 *  the polled wait loop; lets the EP3 OUT receive resume after the ring
 *  buffer drains via RB_Pop.
 *==========================================================================*/
void USBD_FreeJoy_RearmCdcReceive(USBD_HandleTypeDef *pdev)
{
	USBD_FreeJoy_HandleTypeDef *h =
		(USBD_FreeJoy_HandleTypeDef *)pdev->pClassDataCmsit[pdev->classId];
	if (h == NULL) return;
	if (pdev->dev_state != USBD_STATE_CONFIGURED) return;
	if (h->cdc_rx_armed) return;                 /* already pending */

	if (SH_BufferFreeSize() > SH_PACKET_SIZE) {
		h->cdc_rx_armed = 1U;
		(void)USBD_LL_PrepareReceive(pdev, FREEJOY_CDC_DATA_EPOUT_ADDR,
		                             h->cdc_rx_buf, FREEJOY_CDC_DATA_SIZE);
	}
}
