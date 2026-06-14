/**
  ******************************************************************************
  * @file           : board_phase_stubs.c
  * @brief          : F411 link stubs for symbols owned by later phases.
  *
  * Symbols here exist purely to satisfy the linker so the F411 image
  * builds end-to-end. Each function is a no-op (or returns a sane
  * "nothing happened" value). The real implementations land:
  *
  *   - SH_ProcessEndpData / CDC_Send_DATA
  *                     Phase 4E (USB CDC -- simhub telemetry over USB)
  *
  * When the matching phase lands, delete the corresponding stub here
  * and (where appropriate) compile the application's real source file
  * into the F411 build.
  *
  * Phase 8c retired the ws2812b_* stubs -- the real LL TIM1_CH3 + DMA2
  * Stream 6 implementation lives in board/f411_blackpill/Src/board_ws2812b.c.
  ******************************************************************************
  */

#include <stdint.h>

/* simhub.c calls these to push CDC packets over the F103 USB stack.
 * F411 has no CDC class today (Phase 4 ships HID-only); compositing
 * HID+CDC is deferred (anpeaco/FreeJoyX#5). SH_ProcessEndpData stays a
 * no-op; CDC_Send_DATA reports the full length as "sent" so callers that
 * treat a short return as a retryable error (and would spin) don't -- the
 * data is silently discarded until Phase 4E lands. The configurator greys
 * out SimHub-over-USB on F411 so the user isn't offered this dead path. */
void SH_ProcessEndpData(void)
{
}

uint32_t CDC_Send_DATA(uint8_t *data, uint8_t len)
{
	(void)data;
	return len;
}

/* Phase 4D extracted EP1_OUT_Callback + out_buffer; the OutEvent path
 * in usbd_freejoy_if.c now calls App_HidOutDispatch in
 * application/Src/usb_app.c directly. The two link stubs that used to
 * live here are gone -- usb_app.c is the strong definition on both
 * targets, no per-board wiring needed. */
