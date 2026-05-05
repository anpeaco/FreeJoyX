/**
  ******************************************************************************
  * @file           : boot_usb_if.c
  * @brief          : F411 bootloader CustomHID interface (flash dispatch).
  *
  * Mirrors application/Src/usb_endp.c flash dispatch pattern from the
  * F103 bootloader (bootloader/Src/usb_endp.c::EP1_OUT_Callback). The
  * configurator's flasher protocol uses REPORT_ID 4 (the local
  * REPORT_ID_FIRMWARE in F103's bootloader -- NB this is NOT the
  * application's REPORT_ID_FIRMWARE = 5; the configurator-side comment
  * at FreeJoyConfiguratorQtX/src/hiddevice.cpp:18 confirms the bootloader
  * uses ID 4 for legacy reasons).
  *
  * Wire protocol (FreeJoyConfiguratorQtX/src/hiddevice.cpp:570-680):
  *   First packet  [4, 0,0,0, len_lo, len_hi, crc_lo, crc_hi, 0...]
  *                 -> erase app sectors S5/S6/S7
  *                 -> reply [4, 0,1] requesting first body packet
  *   Body packet   [4, cnt_hi, cnt_lo, 0, byte0..byte59]
  *                 -> write 60 bytes at APP_VTOR + (cnt-1)*60
  *                 -> reply [4, (cnt+1)>>8, (cnt+1)&0xFF]
  *   Last packet   same as body but cnt*60 >= len
  *                 -> CRC16 over received image, set flash_finished=1
  *                 -> reply [4, 0xF0, 0x00] OK / 0xF002 CRC err / etc.
  *
  * Status codes: 0xF000=OK, 0xF001=size err, 0xF002=CRC err, 0xF003=erase err.
  ******************************************************************************
  */

#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"

#include "usbd_core.h"
#include "usbd_customhid.h"

#define REPORT_ID_FLASH      0x04U
#define APP_VTOR_ADDR        0x08020000U
#define APP_MAX_BYTES        0x60000U     /* 384 KB = sectors S5..S7 */
#define CHUNK_BYTES          60U

extern USBD_HandleTypeDef hUsbDeviceFS;

/* CRC16 helper from utils/crc16.c (already in the build). */
extern uint16_t Crc16(uint8_t *buf, uint16_t num);

/* State across packets. flash_started gates body-packet dispatch on
 * having seen a successful first-packet erase. flash_finished is the
 * main-loop exit signal. */
volatile uint8_t flash_started  = 0;
volatile uint8_t flash_finished = 0;
static uint16_t  firmware_len = 0;
static uint16_t  crc_in       = 0;

/* HID report descriptor. Vendor-defined collection with REPORT_ID 4
 * carrying a 63-byte input + 63-byte output. Padded with zeros to
 * USBD_CUSTOM_HID_REPORT_DESC_SIZE (233 from the application's
 * usbd_conf.h) since the CustomHID class reports that fixed length to
 * the host -- valid HID parsers stop at END_COLLECTION 0xC0. */
__ALIGN_BEGIN static uint8_t Boot_ReportDesc[USBD_CUSTOM_HID_REPORT_DESC_SIZE] __ALIGN_END = {
	0x06, 0x00, 0xFF,             /* USAGE_PAGE (Vendor Defined 1) */
	0x09, 0x01,                   /* USAGE (Vendor Usage 1) */
	0xA1, 0x01,                   /* COLLECTION (Application) */

	0x85, REPORT_ID_FLASH,        /*   REPORT_ID (4) */
	0x09, 0x02,                   /*   USAGE (Vendor Usage 2) */
	0x15, 0x00,                   /*   LOGICAL_MINIMUM (0) */
	0x26, 0xFF, 0x00,             /*   LOGICAL_MAXIMUM (255) */
	0x75, 0x08,                   /*   REPORT_SIZE (8) */
	0x95, 0x3F,                   /*   REPORT_COUNT (63) */
	0x81, 0x00,                   /*   INPUT (Data,Ary,Abs) */

	0x09, 0x03,                   /*   USAGE (Vendor Usage 3) */
	0x75, 0x08,                   /*   REPORT_SIZE (8) */
	0x95, 0x3F,                   /*   REPORT_COUNT (63) */
	0x91, 0x00,                   /*   OUTPUT (Data,Ary,Abs) */

	0xC0,                         /* END_COLLECTION */
	/* Trailing zeros to the 233-byte declared length follow. */
};

static int8_t Boot_HID_Init(void)
{
	flash_started  = 0;
	flash_finished = 0;
	firmware_len   = 0;
	crc_in         = 0;
	return USBD_OK;
}

static int8_t Boot_HID_DeInit(void)
{
	return USBD_OK;
}

/* Erase application sectors S5/S6/S7. Returns 0 on success, -1 on any
 * sector erase failure. */
static int Boot_EraseApp(void)
{
	HAL_FLASH_Unlock();
	FLASH_EraseInitTypeDef erase = {
		.TypeErase    = FLASH_TYPEERASE_SECTORS,
		.Banks        = FLASH_BANK_1,
		.Sector       = FLASH_SECTOR_5,
		.NbSectors    = 3,
		.VoltageRange = FLASH_VOLTAGE_RANGE_3,
	};
	uint32_t sector_err = 0;
	HAL_StatusTypeDef st = HAL_FLASHEx_Erase(&erase, &sector_err);
	HAL_FLASH_Lock();
	return (st == HAL_OK) ? 0 : -1;
}

/* Program 60 bytes (CHUNK_BYTES) at addr halfword-by-halfword. Matches
 * the F103 bootloader's pattern -- the chunk size isn't word-aligned so
 * halfword keeps the loop trivial. F411 supports halfword program at
 * VOLTAGE_RANGE_3 (RM0383 sec 3.6). */
static void Boot_WriteChunk(uint32_t addr, const uint8_t *data60)
{
	HAL_FLASH_Unlock();
	for (uint8_t i = 0; i < CHUNK_BYTES; i += 2) {
		uint16_t hw = (uint16_t)data60[i] | ((uint16_t)data60[i + 1] << 8);
		HAL_FLASH_Program(FLASH_TYPEPROGRAM_HALFWORD, addr + i, hw);
	}
	HAL_FLASH_Lock();
}

static int8_t Boot_HID_OutEvent(uint8_t *report_buffer)
{
	if (report_buffer[0] != REPORT_ID_FLASH) {
		return USBD_OK;
	}

	uint16_t firmware_in_cnt = 0;
	uint16_t cnt = ((uint16_t)report_buffer[1] << 8) | (uint16_t)report_buffer[2];

	if (cnt == 0) {
		/* First packet: image length + expected CRC + erase. */
		firmware_len = ((uint16_t)report_buffer[5] << 8) | (uint16_t)report_buffer[4];
		crc_in       = ((uint16_t)report_buffer[7] << 8) | (uint16_t)report_buffer[6];

		if (firmware_len <= APP_MAX_BYTES) {
			flash_started = 1;
			if (Boot_EraseApp() == 0) {
				firmware_in_cnt = cnt + 1;
			} else {
				firmware_in_cnt = 0xF003;
			}
		} else {
			firmware_in_cnt = 0xF001;
		}
	} else if (flash_started && firmware_len > 0 && (cnt * CHUNK_BYTES) < firmware_len) {
		/* Body packet. */
		uint32_t addr = APP_VTOR_ADDR + (uint32_t)(cnt - 1) * CHUNK_BYTES;
		Boot_WriteChunk(addr, &report_buffer[4]);
		firmware_in_cnt = cnt + 1;
	} else if (flash_started && firmware_len > 0) {
		/* Last packet: same write, then verify CRC. */
		uint32_t addr = APP_VTOR_ADDR + (uint32_t)(cnt - 1) * CHUNK_BYTES;
		Boot_WriteChunk(addr, &report_buffer[4]);

		uint16_t crc_comp = Crc16((uint8_t *)APP_VTOR_ADDR, firmware_len);
		if (crc_in == crc_comp && crc_comp != 0) {
			flash_started   = 0;
			flash_finished  = 1;
			firmware_in_cnt = 0xF000;
		} else {
			firmware_in_cnt = 0xF002;
		}
	}

	if (firmware_in_cnt > 0) {
		/* reply MUST be static. F411's OTG-FS in non-DMA mode reads
		 * the buffer asynchronously when TXFE IRQ fires; stack memory
		 * is dead by then. */
		static uint8_t reply[3];
		reply[0] = REPORT_ID_FLASH;
		reply[1] = (uint8_t)(firmware_in_cnt >> 8);
		reply[2] = (uint8_t)(firmware_in_cnt & 0xFF);
		USBD_CUSTOM_HID_SendReport(&hUsbDeviceFS, reply, 3);
	}

	/* Re-arm EP1 OUT so the next chunk can be received. Cube's
	 * CustomHID class lib NAKs subsequent OUTs after the first
	 * unless the user code calls this from OutEvent. Without it
	 * only the first DFU chunk gets through. */
	USBD_CUSTOM_HID_ReceivePacket(&hUsbDeviceFS);

	return USBD_OK;
}

USBD_CUSTOM_HID_ItfTypeDef Boot_HID_fops = {
	Boot_ReportDesc,
	Boot_HID_Init,
	Boot_HID_DeInit,
	Boot_HID_OutEvent,
};
