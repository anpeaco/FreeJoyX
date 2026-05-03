/**
  ******************************************************************************
  * @file           : board_phase_stubs.c
  * @brief          : F411 link stubs for symbols owned by later phases.
  *
  * Symbols here exist purely to satisfy the linker so the F411 image
  * builds end-to-end. Each function is a no-op (or returns a sane
  * "nothing happened" value). The real implementations land:
  *
  *   - ws2812b_*       Phase 8 (F411 LED port; current F411 has no TIM1+DMA
  *                     bit-bang of the WS2812B protocol)
  *   - SH_ProcessEndpData / CDC_Send_DATA
  *                     Phase 4 (USB CDC -- simhub telemetry over USB)
  *
  * When the matching phase lands, delete the corresponding stub here
  * and (where appropriate) compile the application's real source file
  * into the F411 build.
  ******************************************************************************
  */

#include <stdint.h>
#include "ws2812b.h"
#include "bitmap.h"

/* WS2812B / RGB-LED chain output. F103 drives this with TIM1_CH1 + DMA;
 * F411 needs a port behind a board_ws2812b.h seam (Phase 8). */
void ws2812b_Init(uint8_t led_type)
{
	(void)led_type;
}

int ws2812b_IsReady(void)
{
	/* "Always ready" so application code never spins waiting on us.
	 * led_effects.c::ArgbLed_Process exits on !IsReady(); returning 1
	 * lets it proceed and call SendRGB which is also a no-op. */
	return 1;
}

void ws2812b_SendRGB(argb_led_t *rgb, unsigned count)
{
	(void)rgb; (void)count;
}

void ws2812b_SendHSV(HSV_t *hsv, unsigned count)
{
	(void)hsv; (void)count;
}

/* simhub.c calls these to push CDC packets over the F103 USB stack.
 * F411 has no CDC class today (Phase 4 ships HID-only); compositing
 * HID+CDC is deferred. SH_ProcessEndpData stays a no-op; CDC_Send_DATA
 * pretends the send completed. */
void SH_ProcessEndpData(void)
{
}

uint32_t CDC_Send_DATA(uint8_t *data, uint8_t len)
{
	(void)data; (void)len;
	return 0;
}

/* Phase 4D extracted EP1_OUT_Callback + out_buffer; the OutEvent path
 * in usbd_freejoy_if.c now calls App_HidOutDispatch in
 * application/Src/usb_app.c directly. The two link stubs that used to
 * live here are gone -- usb_app.c is the strong definition on both
 * targets, no per-board wiring needed. */
