
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
	
		FreeJoy software for game device controllers
    Copyright (C) 2020  Yury Vostrenkov (yuvostrenkov@gmail.com)

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <https://www.gnu.org/licenses/>.
		
  ******************************************************************************
  */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

#include "board_dfu.h"
#include "periphery.h"
#include "config.h"
#include "analog.h"
#include "buttons.h"
#include "leds.h"
#include "encoders.h"
#include "led_effects.h"
#include "simhub.h"

#include "board_usb.h"
#ifdef BOARD_F103_BLUEPILL
/* F1-only USB headers still needed for the host-driven flasher trigger
 * path that lives in usb_endp.c (REPORT_ID_FIRMWARE handling reaches
 * into bootloader globals). F411 routes that through Board_EnterDfu
 * directly; no F1 USB lib needed. */
#include "usb_hw.h"
#include "usb_lib.h"
#include "usb_pwr.h"
#endif


/* Private variables ---------------------------------------------------------*/
dev_config_t dev_config;
volatile uint8_t bootloader = 0;

/* Private function prototypes -----------------------------------------------*/

/**
  * @brief  The application entry point.
  *
  * @retval None
  */
int main(void)
{
	// Relocate vector table to the application's load address. Wrapped
	// in the BSP so the F411 port can swap in its own offset (S5-relative
	// rather than F103's 8-KB-bootloader-relative).
	Board_RelocateVectorTable();

	SysTick_Init();

	// getting configuration from flash memory
	DevConfigGet(&dev_config);

	// set default config at first startup
	if ((dev_config.firmware_version & 0xFFF0) != (FIRMWARE_VERSION &0xFFF0))
	{
		/* If this fails, the version-mismatch check at the top of
		 * the next boot will retry init_config. Self-healing on
		 * power cycle; no action to take here. */
		(void)DevConfigSet((dev_config_t *) &init_config);
		DevConfigGet(&dev_config);
	}
	AppConfigInit(&dev_config);

	Board_USB_Init();
	// wait for USB initialization
	Delay_ms(1000);

	IO_Init(&dev_config);

	EncodersInit(&dev_config);	// add rgb timer check. what?
	ShiftRegistersInit(&dev_config);
	RadioButtons_Init(&dev_config);
	SequentialButtons_Init(&dev_config);
	Gestures_Init(&dev_config);

	// init sensors
	AxesInit(&dev_config);
	// start sequential periphery reading
	Timers_Init(&dev_config);
	
	uint8_t serial_num[24] = {0};
	Board_GetSerialNum((uint8_t*)serial_num, 24);

	// ring buffer for cdc
	uint8_t buf[MAX_RING_BIF_SIZE];
	ring_buf_t *rb = RB_GetPtr();
	RB_Init(rb, buf, MAX_RING_BIF_SIZE);
	
  while (1)
  {
		ButtonsDebounceProcess(&dev_config);
		ButtonsReadLogical(&dev_config);
		
		LEDs_PhysicalProcess(&dev_config);
		
		analog_data_t tmp[8];
		AnalogGet(NULL, tmp, NULL);
		PWM_SetFromAxis(&dev_config, tmp);
		
		ArgbLed_Process(&dev_config, serial_num, 24, GetMillis());
		
		// Enter flasher command received
		if (bootloader > 0)
		{
			// Disable HID report generation
			Board_TickStop();
			Delay_ms(50);	// time to let HID end last transmission
			// Graceful USB disconnect before DFU jump.
			Board_USB_DeInit();
			Delay_ms(500);
			Board_EnterDfu();
		}
  }
}

/**
  * @brief  Jumping to memory address corresponding bootloader program
  * @param  None
  * @retval None
  */
/* EnterBootloader() moved to board/f103_bluepill/Src/board_dfu.c as
 * Board_EnterDfu() in the F411 BSP-seam refactor (Phase 1). The single
 * caller above now invokes Board_EnterDfu() directly. */


/**
  * @}
  */

/**
  * @}
  */
