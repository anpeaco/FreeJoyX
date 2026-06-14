/**
  ******************************************************************************
  * @file           : board_ws2812b.c
  * @brief          : F411 WS2812B / PL9823 driver -- LL TIM1_CH3 + DMA2 S6 C6.
  *
  * Identical algorithm to F103 (board/f103_bluepill/Src/board_ws2812b.c):
  *   - Static double-buffered PWM_t array
  *   - Circular DMA from buffer to TIM1->CCR3
  *   - HT + TC interrupts feed the next half each cycle
  *   - Source (RGB or HSV) -> filter callback fills PWM_t entries
  *
  * Only the chip-specific glue changes:
  *   - Timer/DMA register access via LL instead of StdPeriph
  *   - DMA mapping is DMA2 Stream 6 Channel 6 (TIM1_CH3 on F411 per
  *     RM0383 Table 28), not F103's DMA1 Channel 6
  *   - Period recomputed for F411's 96 MHz APB2 timer clock:
  *       F103:  72 MHz / 90  = 800 kHz, T0H=30 (417 ns), T1H=60 (833 ns)
  *       F411:  96 MHz / 120 = 800 kHz, T0H=40 (417 ns), T1H=80 (833 ns)
  *     PL9823 timing scales the same way: F103 18/72 -> F411 24/96.
  *
  * TIM1 is advanced-control on F411 (same as F103) so BDTR.MOE must be
  * enabled or CH3 stays idle. PA10 is driven via AF1 alternate function.
  *
  * Pre-hardware build only -- the timing-critical bit pattern hasn't been
  * verified against a real LED chain. Numbers come from F103's Step 1
  * empirical config scaled to 96 MHz; should be in spec on hardware day,
  * but T0H/T1H may need a tick of fine-tuning if the LEDs misread bits.
  ******************************************************************************
  */

#include <stdint.h>
#include <string.h>

#include "stm32f4xx.h"
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_gpio.h"
#include "stm32f4xx_ll_tim.h"
#include "stm32f4xx_ll_dma.h"

#include "bitmap.h"
#include "ws2812b.h"

/*----------------------------------------------------------------------
 * Bit-pulse counts (96 MHz timer clock, 120-tick period @ 800 kHz)
 *--------------------------------------------------------------------*/
#define WS2812B_PERIOD          120
#define WS2812B_PULSE_HIGH      80     /* T1H ~= 833 ns */
#define WS2812B_PULSE_LOW       40     /* T0H ~= 417 ns */
#define PL9823_PULSE_HIGH       96     /* PL9823 T1H ~= 1000 ns */
#define PL9823_PULSE_LOW        24     /* PL9823 T0H ~= 250 ns */

#define WS2812B_BUFFER_SIZE     60
#define WS2812B_START_SIZE      2

#define WS2812B_USE_GAMMA_CORRECTION
#define WS2812B_USE_PRECALCULATED_GAMMA_TABLE

#define MIN(a, b)   ({ __typeof__(a) a1 = a; __typeof__(b) b1 = b; a1 < b1 ? a1 : b1; })

#if defined(__ICCARM__)
__packed struct PWM
#else
struct __attribute__((packed)) PWM
#endif
{
	uint16_t g[8], r[8], b[8];
};

typedef struct PWM PWM_t;
typedef void (SrcFilter_t)(void **, PWM_t **, unsigned *, unsigned);

#ifdef WS2812B_USE_GAMMA_CORRECTION
#ifdef WS2812B_USE_PRECALCULATED_GAMMA_TABLE
static const uint8_t LEDGammaTable[] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1,
	2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9, 10,
	10, 11, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 17, 17, 18, 18, 19, 19, 20, 20, 21, 21,
	22, 23, 23, 24, 24, 25, 26, 26, 27, 28, 28, 29, 30, 30, 31, 32, 32, 33, 34, 35, 35, 36, 37, 38,
	38, 39, 40, 41, 42, 42, 43, 44, 45, 46, 47, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 56, 57, 58,
	59, 60, 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 84,
	85, 86, 87, 88, 89, 91, 92, 93, 94, 95, 97, 98, 99, 100, 102, 103, 104, 105, 107, 108, 109, 111,
	112, 113, 115, 116, 117, 119, 120, 121, 123, 124, 126, 127, 128, 130, 131, 133, 134, 136, 137,
	139, 140, 142, 143, 145, 146, 148, 149, 151, 152, 154, 155, 157, 158, 160, 162, 163, 165, 166,
	168, 170, 171, 173, 175, 176, 178, 180, 181, 183, 185, 186, 188, 190, 192, 193, 195, 197, 199,
	200, 202, 204, 206, 207, 209, 211, 213, 215, 217, 218, 220, 222, 224, 226, 228, 230, 232, 233,
	235, 237, 239, 241, 243, 245, 247, 249, 251, 253, 255 };
#endif
#endif

static inline uint8_t LEDGamma(uint8_t v)
{
#ifdef WS2812B_USE_GAMMA_CORRECTION
#ifdef WS2812B_USE_PRECALCULATED_GAMMA_TABLE
	return LEDGammaTable[v];
#else
	return (v * v + v) >> 8;
#endif
#else
	return v;
#endif
}

static volatile int DMABusy;

static PWM_t DMABuffer[WS2812B_BUFFER_SIZE];

static SrcFilter_t *DMAFilter;
static void *DMASrc;
static unsigned DMACount;

static uint8_t argb_type = ARGB_WS2812B;

static void SrcFilterNull(void **src, PWM_t **pwm, unsigned *count, unsigned size)
{
	(void)src; (void)count;
	memset(*pwm, 0, size * sizeof(PWM_t));
	*pwm += size;
}

static void RGB2PWM(argb_led_t *rgb, PWM_t *pwm)
{
	uint8_t r, g, b;
	if (rgb->is_disabled)
	{
		r = LEDGamma(0);
		g = LEDGamma(0);
		b = LEDGamma(0);
	}
	else
	{
		r = LEDGamma(rgb->color.r);
		g = LEDGamma(rgb->color.g);
		b = LEDGamma(rgb->color.b);
	}

	uint8_t mask = 128;

	uint8_t h, l;
	if (argb_type == ARGB_WS2812B)
	{
		h = WS2812B_PULSE_HIGH;
		l = WS2812B_PULSE_LOW;
	}
	else
	{
		h = PL9823_PULSE_HIGH;
		l = PL9823_PULSE_LOW;
	}

	int i;
	for (i = 0; i < 8; i++)
	{
		pwm->r[i] = r & mask ? h : l;
		pwm->g[i] = g & mask ? h : l;
		pwm->b[i] = b & mask ? h : l;
		mask >>= 1;
	}
}

static void SrcFilterRGB(void **src, PWM_t **pwm, unsigned *count, unsigned size)
{
	argb_led_t *rgb = *src;
	PWM_t *p = *pwm;

	*count -= size;

	while (size--)
	{
		RGB2PWM(rgb++, p++);
	}

	*src = rgb;
	*pwm = p;
}

static void SrcFilterHSV(void **src, PWM_t **pwm, unsigned *count, unsigned size)
{
	HSV_t *hsv = *src;
	PWM_t *p = *pwm;

	*count -= size;

	while (size--)
	{
		argb_led_t rgb;

		HSV2RGB(hsv++, &rgb.color);
		RGB2PWM(&rgb, p++);
	}

	*src = hsv;
	*pwm = p;
}

static void DMASend(SrcFilter_t *filter, void *src, unsigned count)
{
	if (!DMABusy)
	{
		DMABusy = 1;

		DMAFilter = filter;
		DMASrc = src;
		DMACount = count;

		PWM_t *pwm = DMABuffer;
		PWM_t *end = &DMABuffer[WS2812B_BUFFER_SIZE];

		// Start sequence (zeros to drive DOUT low for the WS2812B reset latch)
		SrcFilterNull(NULL, &pwm, NULL, WS2812B_START_SIZE);

		// RGB PWM data
		DMAFilter(&DMASrc, &pwm, &DMACount, MIN(DMACount, end - pwm));

		// Rest of buffer
		if (pwm < end)
			SrcFilterNull(NULL, &pwm, NULL, end - pwm);

		// Start transfer
		LL_DMA_SetDataLength(DMA2, LL_DMA_STREAM_6, (uint32_t)sizeof(DMABuffer) / sizeof(uint16_t));

		LL_TIM_EnableCounter(TIM1);
		LL_DMA_EnableStream(DMA2, LL_DMA_STREAM_6);
	}
}

static void DMASendNext(PWM_t *pwm, PWM_t *end)
{
	if (!DMAFilter)
	{
		// Stop transfer
		LL_TIM_DisableCounter(TIM1);
		LL_DMA_DisableStream(DMA2, LL_DMA_STREAM_6);

		DMABusy = 0;
	}
	else if (!DMACount)
	{
		// Rest of buffer
		SrcFilterNull(NULL, &pwm, NULL, end - pwm);

		DMAFilter = NULL;
	}
	else
	{
		// RGB PWM data
		DMAFilter(&DMASrc, &pwm, &DMACount, MIN(DMACount, end - pwm));

		// Rest of buffer
		if (pwm < end)
			SrcFilterNull(NULL, &pwm, NULL, end - pwm);
	}
}

void DMA2_Stream6_IRQHandler(void)
{
	if (LL_DMA_IsActiveFlag_HT6(DMA2))
	{
		LL_DMA_ClearFlag_HT6(DMA2);
		DMASendNext(DMABuffer, &DMABuffer[WS2812B_BUFFER_SIZE / 2]);
	}

	if (LL_DMA_IsActiveFlag_TC6(DMA2))
	{
		LL_DMA_ClearFlag_TC6(DMA2);
		DMASendNext(&DMABuffer[WS2812B_BUFFER_SIZE / 2], &DMABuffer[WS2812B_BUFFER_SIZE]);
	}
}

void ws2812b_Init(uint8_t led_type)
{
	argb_type = led_type;

	/* Clocks: GPIOA for PA10, TIM1 on APB2, DMA2 on AHB1. */
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOA);
	LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_DMA2);
	LL_APB2_GRP1_EnableClock(LL_APB2_GRP1_PERIPH_TIM1);

	/* PA10 -> TIM1_CH3 (AF1), open-drain (matches F103
	 * WS2812B_GPIO_Mode = GPIO_Mode_AF_OD). External pull-up on the LED
	 * data line is required either way; OD lets a 5V chain be pulled
	 * above 3V3 without back-driving the MCU pin. */
	LL_GPIO_InitTypeDef gpio = {0};
	gpio.Pin        = LL_GPIO_PIN_10;
	gpio.Mode       = LL_GPIO_MODE_ALTERNATE;
	gpio.Speed      = LL_GPIO_SPEED_FREQ_VERY_HIGH;
	gpio.OutputType = LL_GPIO_OUTPUT_OPENDRAIN;
	gpio.Pull       = LL_GPIO_PULL_NO;
	gpio.Alternate  = LL_GPIO_AF_1;
	LL_GPIO_Init(GPIOA, &gpio);

	/* TIM1 single-owner rule: TIM1 is shared by Fast Encoder 1 (PA8/PA9,
	 * board_encoder.c), LED PWM channel TIM1_CH1 (PA8, board_pwm.c) and this
	 * WS2812B driver (PA10/CH3). Each reconfigures the whole TIM1 time base,
	 * so at most ONE may be active in a given config -- last initializer wins.
	 * The configurator must keep these mutually exclusive (same constraint as
	 * F103). Verify the Enc1 + WS2812B coexistence filter on the configurator
	 * side before shipping a config that enables both.
	 *
	 * TIM1 base: 96 MHz / (PERIOD ticks) = 800 kHz output rate. No
	 * prescaler -- the F411 timer clock matches the WS2812B 1.25 us
	 * period directly when ARR = 119. */
	LL_TIM_InitTypeDef tim = {0};
	tim.Prescaler         = 0;
	tim.CounterMode       = LL_TIM_COUNTERMODE_UP;
	tim.Autoreload        = WS2812B_PERIOD - 1U;
	tim.ClockDivision     = LL_TIM_CLOCKDIVISION_DIV1;
	tim.RepetitionCounter = 0;
	LL_TIM_Init(TIM1, &tim);
	LL_TIM_EnableARRPreload(TIM1);

	/* CH3 PWM mode 1, polarity high, preload enabled, initial duty 0
	 * (line idle-low between transfers). */
	LL_TIM_OC_InitTypeDef oc = {0};
	oc.OCMode       = LL_TIM_OCMODE_PWM1;
	oc.OCState      = LL_TIM_OCSTATE_ENABLE;
	oc.OCNState     = LL_TIM_OCSTATE_DISABLE;
	oc.OCPolarity   = LL_TIM_OCPOLARITY_HIGH;
	oc.OCNPolarity  = LL_TIM_OCPOLARITY_HIGH;
	oc.OCIdleState  = LL_TIM_OCIDLESTATE_LOW;
	oc.OCNIdleState = LL_TIM_OCIDLESTATE_LOW;
	oc.CompareValue = 0;
	LL_TIM_OC_Init(TIM1, LL_TIM_CHANNEL_CH3, &oc);
	LL_TIM_OC_EnablePreload(TIM1, LL_TIM_CHANNEL_CH3);

	/* TIM1 advanced -> BDTR.MOE must be set or outputs stay tristated. */
	LL_TIM_EnableAllOutputs(TIM1);

	/* TIM1_CH3 -> DMA2 Stream 6. On STM32F4 the TIM1_CH3 request appears on
	 * BOTH Channel 0 (the grouped TIM1_CH1/CH2/CH3 entry) and Channel 6 (the
	 * dedicated TIM1 column) of Stream 6 per RM0383 Table 28, so Channel 6 is
	 * valid. NOT yet scope-verified on hardware (anpeaco/FreeJoyX issue: WS2812B
	 * DMA verify) -- if addressable LEDs don't light on the BlackPill, switch
	 * this to LL_DMA_CHANNEL_0 first and re-scope PA10. Half-word transfers,
	 * peripheral fixed at TIM1->CCR3, memory increments through DMABuffer,
	 * circular so the half/full IRQs keep refilling until DMASend disables it. */
	LL_DMA_InitTypeDef dma = {0};
	dma.Channel              = LL_DMA_CHANNEL_6;
	dma.Direction            = LL_DMA_DIRECTION_MEMORY_TO_PERIPH;
	dma.Mode                 = LL_DMA_MODE_CIRCULAR;
	dma.PeriphOrM2MSrcAddress = (uint32_t)&TIM1->CCR3;
	dma.MemoryOrM2MDstAddress = (uint32_t)DMABuffer;
	dma.PeriphOrM2MSrcIncMode = LL_DMA_PERIPH_NOINCREMENT;
	dma.MemoryOrM2MDstIncMode = LL_DMA_MEMORY_INCREMENT;
	dma.PeriphOrM2MSrcDataSize = LL_DMA_PDATAALIGN_HALFWORD;
	dma.MemoryOrM2MDstDataSize = LL_DMA_MDATAALIGN_HALFWORD;
	dma.NbData               = (uint32_t)sizeof(DMABuffer) / sizeof(uint16_t);
	dma.Priority             = LL_DMA_PRIORITY_HIGH;
	dma.FIFOMode             = LL_DMA_FIFOMODE_DISABLE;
	dma.MemBurst             = LL_DMA_MBURST_SINGLE;
	dma.PeriphBurst          = LL_DMA_PBURST_SINGLE;
	LL_DMA_Init(DMA2, LL_DMA_STREAM_6, &dma);

	/* TIM1 raises a DMA request on CC3 each update event. */
	LL_TIM_EnableDMAReq_CC3(TIM1);

	/* HT/TC IRQs trigger the chained refill in DMA2_Stream6_IRQHandler. */
	LL_DMA_EnableIT_HT(DMA2, LL_DMA_STREAM_6);
	LL_DMA_EnableIT_TC(DMA2, LL_DMA_STREAM_6);
	NVIC_SetPriority(DMA2_Stream6_IRQn, 2);
	NVIC_EnableIRQ(DMA2_Stream6_IRQn);
}

inline int ws2812b_IsReady(void)
{
	return !DMABusy;
}

void ws2812b_SendRGB(argb_led_t *rgb, unsigned count)
{
	DMASend(&SrcFilterRGB, rgb, count);
}

void ws2812b_SendHSV(HSV_t *hsv, unsigned count)
{
	DMASend(&SrcFilterHSV, hsv, count);
}
