/**
  ******************************************************************************
  * @file           : usbd_conf.c
  * @brief          : F411 USBD low-level driver: HAL_PCD MSP + USBD_LL_* bridge.
  *
  * Two layers in this file:
  *
  *   1. HAL_PCD_Msp{Init,DeInit} -- the BSP hooks HAL calls during
  *      HAL_PCD_Init. Configures PA11/PA12 as OTG_FS DM/DP (AF10), enables
  *      the OTG_FS clock, and sets the NVIC priority for OTG_FS_IRQn.
  *      VBUS sensing on PA9 is intentionally disabled so the encoder
  *      driver retains PA9 (TIM1_CH2 = Encoder 1 B). ID sensing on PA10
  *      is also unused (device-mode only; PA10 stays a free GPIO).
  *
  *   2. USBD_LL_* shims that bridge the USBD core's API to HAL_PCD.
  *      Lifted near-verbatim from CubeF4's
  *      Projects/STM32F411E-Discovery/Demonstrations/Src/usbd_conf.c
  *      reference. The HAL_PCD_*Callback handlers feed the USBD core
  *      (USBD_LL_SetupStage, _DataInStage, etc.) so the device library
  *      sees stack events even though the LL_USB driver runs the wire.
  *
  * Static memory: USBD_static_malloc returns slots from a fixed-size
  * pool sized for one USBD_CUSTOM_HID_HandleTypeDef instance. No heap
  * is required; the firmware doesn't link malloc.
  *
  * The OTG_FS_IRQHandler implementation calls HAL_PCD_IRQHandler with
  * our hpcd; the IRQ vector itself is registered via the entry in
  * board/f411_blackpill/Src/stm32f4xx_it.c (added in step 4).
  ******************************************************************************
  */

#include "stm32f4xx_hal.h"
#include "usbd_core.h"
#include "usbd_conf.h"
#include "usbd_customhid.h"   /* USBD_CUSTOM_HID_HandleTypeDef sizing (bootloader) */

#ifndef BOOTLOADER
#include "usbd_freejoy_class.h"  /* USBD_FreeJoy_HandleTypeDef sizing (app) */
#endif

PCD_HandleTypeDef hpcd;

/*============================================================================
 *                          PCD MSP (HAL <- BSP)
 *==========================================================================*/
void HAL_PCD_MspInit(PCD_HandleTypeDef *hpcd)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	if (hpcd->Instance == USB_OTG_FS) {
		__HAL_RCC_GPIOA_CLK_ENABLE();

		/* PA11 = OTG_FS_DM, PA12 = OTG_FS_DP, AF10. */
		GPIO_InitStruct.Pin       = GPIO_PIN_11 | GPIO_PIN_12;
		GPIO_InitStruct.Mode      = GPIO_MODE_AF_PP;
		GPIO_InitStruct.Pull      = GPIO_NOPULL;
		GPIO_InitStruct.Speed     = GPIO_SPEED_HIGH;
		GPIO_InitStruct.Alternate = GPIO_AF10_OTG_FS;
		HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

		/* Deliberately NOT configuring PA9 (VBUS sense) -- vbus_sensing
		 * is disabled in USBD_LL_Init so the OTG-FS peripheral doesn't
		 * read it. PA9 is FreeJoy's Encoder 1 B (TIM1_CH2 AF1) and
		 * gets configured by Board_FastEncoderInit. Touching it here
		 * would silently break the encoder. */

		/* Deliberately NOT configuring PA10 (OTG_FS_ID) -- we run as
		 * USB device only; ID detection is irrelevant. PA10 stays a
		 * free user GPIO. */

		__HAL_RCC_USB_OTG_FS_CLK_ENABLE();

		HAL_NVIC_SetPriority(OTG_FS_IRQn, 3, 0);
		HAL_NVIC_EnableIRQ(OTG_FS_IRQn);
	}
}

void HAL_PCD_MspDeInit(PCD_HandleTypeDef *hpcd)
{
	if (hpcd->Instance == USB_OTG_FS) {
		__HAL_RCC_USB_OTG_FS_CLK_DISABLE();
		HAL_NVIC_DisableIRQ(OTG_FS_IRQn);
	}
}

/*============================================================================
 *                       LL Driver Callbacks (PCD -> USBD)
 *==========================================================================*/
void HAL_PCD_SetupStageCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_SetupStage(hpcd->pData, (uint8_t *)hpcd->Setup);
}

void HAL_PCD_DataOutStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
	USBD_LL_DataOutStage(hpcd->pData, epnum, hpcd->OUT_ep[epnum].xfer_buff);
}

void HAL_PCD_DataInStageCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
	USBD_LL_DataInStage(hpcd->pData, epnum, hpcd->IN_ep[epnum].xfer_buff);
}

void HAL_PCD_SOFCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_SOF(hpcd->pData);
}

void HAL_PCD_ResetCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_SetSpeed(hpcd->pData, USBD_SPEED_FULL);
	USBD_LL_Reset(hpcd->pData);
}

void HAL_PCD_SuspendCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_Suspend(hpcd->pData);
}

void HAL_PCD_ResumeCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_Resume(hpcd->pData);
}

void HAL_PCD_ISOOUTIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
	USBD_LL_IsoOUTIncomplete(hpcd->pData, epnum);
}

void HAL_PCD_ISOINIncompleteCallback(PCD_HandleTypeDef *hpcd, uint8_t epnum)
{
	USBD_LL_IsoINIncomplete(hpcd->pData, epnum);
}

void HAL_PCD_ConnectCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_DevConnected(hpcd->pData);
}

void HAL_PCD_DisconnectCallback(PCD_HandleTypeDef *hpcd)
{
	USBD_LL_DevDisconnected(hpcd->pData);
}

/*============================================================================
 *                       LL Driver Interface (USBD -> PCD)
 *==========================================================================*/
USBD_StatusTypeDef USBD_LL_Init(USBD_HandleTypeDef *pdev)
{
	hpcd.Instance                  = USB_OTG_FS;
	hpcd.Init.dev_endpoints        = 4;
	hpcd.Init.use_dedicated_ep1    = 0;
	hpcd.Init.dma_enable           = 0;
	hpcd.Init.low_power_enable     = 0;
	hpcd.Init.phy_itface           = PCD_PHY_EMBEDDED;
	hpcd.Init.Sof_enable           = 0;
	hpcd.Init.speed                = PCD_SPEED_FULL;
	hpcd.Init.vbus_sensing_enable  = 0;   /* see HAL_PCD_MspInit comment */

	hpcd.pData     = pdev;
	pdev->pData    = &hpcd;

	HAL_PCD_Init(&hpcd);

	/* Endpoint FIFO sizing for OTG-FS (1.25 KB / 320 32-bit words total).
	 *
	 * Bootloader (single CustomHID, EP1 IN/OUT only):
	 *   RX shared FIFO  128 words = 512 bytes (all OUT incl. SETUP)
	 *   EP0 IN FIFO      64 words = 256 bytes
	 *   EP1 IN FIFO     128 words = 512 bytes (margin for back-to-back IN)
	 *   total           320 words = 1280 bytes (full budget)
	 *
	 * Application (Phase 4E HID+HID+CDC, EP1 IN + EP2 IN/OUT + EP3 IN/OUT):
	 *   RX shared FIFO   96 words = 384 bytes (EP2 OUT + EP3 OUT + SETUP;
	 *                                          formula min for 2 OUT EPs at
	 *                                          64-byte packets is ~51
	 *                                          words, so ~1.9x margin)
	 *   EP0 IN FIFO      64 words = 256 bytes
	 *   EP1 IN FIFO      64 words = 256 bytes (joy)
	 *   EP2 IN FIFO      64 words = 256 bytes (cfg)
	 *   EP3 IN FIFO      32 words = 128 bytes (CDC bulk; 2x 64-byte
	 *                                          packets pipelined)
	 *   total           320 words = 1280 bytes (full budget — no slack)
	 */
	HAL_PCDEx_SetTxFiFo(&hpcd, 0, 0x40);
#ifdef BOOTLOADER
	HAL_PCDEx_SetRxFiFo(&hpcd, 0x80);
	HAL_PCDEx_SetTxFiFo(&hpcd, 1, 0x80);
#else
	HAL_PCDEx_SetRxFiFo(&hpcd, 0x60);
	HAL_PCDEx_SetTxFiFo(&hpcd, 1, 0x40);
	HAL_PCDEx_SetTxFiFo(&hpcd, 2, 0x40);
	HAL_PCDEx_SetTxFiFo(&hpcd, 3, 0x20);
#endif

	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_DeInit(USBD_HandleTypeDef *pdev)
{
	HAL_PCD_DeInit(pdev->pData);
	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Start(USBD_HandleTypeDef *pdev)
{
	HAL_PCD_Start(pdev->pData);
	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Stop(USBD_HandleTypeDef *pdev)
{
	HAL_PCD_Stop(pdev->pData);
	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_OpenEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                  uint8_t ep_type, uint16_t ep_mps)
{
	HAL_PCD_EP_Open(pdev->pData, ep_addr, ep_mps, ep_type);
	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_CloseEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
	HAL_PCD_EP_Close(pdev->pData, ep_addr);
	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_FlushEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
	HAL_PCD_EP_Flush(pdev->pData, ep_addr);
	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_StallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
	HAL_PCD_EP_SetStall(pdev->pData, ep_addr);
	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_ClearStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
	HAL_PCD_EP_ClrStall(pdev->pData, ep_addr);
	return USBD_OK;
}

uint8_t USBD_LL_IsStallEP(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
	PCD_HandleTypeDef *p = pdev->pData;
	if ((ep_addr & 0x80) == 0x80) {
		return p->IN_ep[ep_addr & 0x7F].is_stall;
	} else {
		return p->OUT_ep[ep_addr & 0x7F].is_stall;
	}
}

USBD_StatusTypeDef USBD_LL_SetUSBAddress(USBD_HandleTypeDef *pdev, uint8_t dev_addr)
{
	HAL_PCD_SetAddress(pdev->pData, dev_addr);
	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_Transmit(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                    uint8_t *pbuf, uint32_t size)
{
	HAL_PCD_EP_Transmit(pdev->pData, ep_addr, pbuf, size);
	return USBD_OK;
}

USBD_StatusTypeDef USBD_LL_PrepareReceive(USBD_HandleTypeDef *pdev, uint8_t ep_addr,
                                          uint8_t *pbuf, uint32_t size)
{
	HAL_PCD_EP_Receive(pdev->pData, ep_addr, pbuf, size);
	return USBD_OK;
}

uint32_t USBD_LL_GetRxDataSize(USBD_HandleTypeDef *pdev, uint8_t ep_addr)
{
	return HAL_PCD_EP_GetRxCount(pdev->pData, ep_addr);
}

void USBD_LL_Delay(uint32_t Delay)
{
	HAL_Delay(Delay);
}

/*============================================================================
 *                       Static memory pool for USBD core
 *==========================================================================*/
/* USBD core asks for one block per active class instance via USBD_malloc.
 * Pool sized to fit the larger of the two possible class handles:
 *   - Bootloader: USBD_CUSTOM_HID_HandleTypeDef (stock Cube class)
 *   - Application: USBD_FreeJoy_HandleTypeDef (Phase 4F dual-HID composite)
 * Each binary picks one, so the pool only needs to fit that one in
 * practice; sizing for the larger keeps usbd_conf.c board-build-agnostic. */
#ifdef BOOTLOADER
#define USBD_STATIC_POOL_SIZE   sizeof(USBD_CUSTOM_HID_HandleTypeDef)
#else
#define USBD_STATIC_POOL_SIZE   sizeof(USBD_FreeJoy_HandleTypeDef)
#endif
static uint8_t USBD_StaticPool[USBD_STATIC_POOL_SIZE] __attribute__((aligned(4)));
static uint8_t USBD_StaticInUse;

void *USBD_static_malloc(uint32_t size)
{
	(void)size;
	if (USBD_StaticInUse) return NULL;
	USBD_StaticInUse = 1;
	return (void *)USBD_StaticPool;
}

void USBD_static_free(void *p)
{
	(void)p;
	USBD_StaticInUse = 0;
}
