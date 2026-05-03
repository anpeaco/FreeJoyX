/**
  ******************************************************************************
  * @file           : board_uart.c
  * @brief          : F411 BlackPill UART driver -- compile/link stub (Phase 5c).
  *
  * simhub.c links against UART_Start / UART_WriteNonBlocking and the
  * gen_crc16 CRC helper declared in application/Inc/uart.h. F103's
  * implementation lives in board/f103_bluepill/Src/board_uart.c.
  *
  * F411 will eventually use LL_USART1 on PA9 (AF7) backed by DMA2 (per
  * RM0383). PA9 is shared with TIM1_CH2 (Encoder 1 B) on AF1 -- the
  * configurator-side mutex from Step 1 already enforces this exclusion
  * for both boards. Phase 5c ships compile/link stubs; runtime arrives
  * once a BlackPill is on a scope.
  *
  * gen_crc16 is chip-agnostic and provided in full here so simhub
  * builds without dragging in board/f103_bluepill code on F411.
  ******************************************************************************
  */

#include "uart.h"

void UART_Start(void)
{
	/* No-op stub. */
}

void UART_WriteNonBlocking(uint8_t * data, uint16_t length)
{
	(void)data; (void)length;
}

uint16_t gen_crc16(const uint8_t *data, uint16_t size)
{
	uint16_t out = 0, crc = 0;
	int32_t bits_read = 0, bit_flag = 0, i = 0;
	int32_t j = 0x0001;

	if (data == NULL) return 0;

	while (size > 0)
	{
		bit_flag = out >> 15;
		out <<= 1;
		out |= (*data >> bits_read) & 1;

		bits_read++;
		if (bits_read > 7)
		{
			bits_read = 0;
			data++;
			size --;
		}

		if (bit_flag) out ^= CRC16;
	}

	for (i = 0; i < 16; ++i)
	{
		bit_flag = out >> 15;
		out <<= 1;
		if (bit_flag) out ^= CRC16;
	}

	i = 0x8000;
	for (; i !=0; i >>= 1, j <<= 1)
	{
		if (i & out) crc |= j;
	}

	return crc;
}
