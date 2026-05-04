/**
  ******************************************************************************
  * @file           : periphery.c
  * @brief          : Periphery driver implementation
	
		
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

#include "periphery.h"
#include "ws2812b.h"
#include "uart.h"
#include "board_pwm.h"

#ifdef BOARD_F411_BLACKPILL
/* Generator_Init's F411 branch uses LL_TIM + LL_GPIO + LL bus to set up
 * the 4 MHz GEN clock for TLE5011. Other F411 BSP files include these
 * headers directly; periphery.c is one of the few application/Src files
 * that needs to know it's running on F411. */
#include "stm32f4xx_ll_bus.h"
#include "stm32f4xx_ll_tim.h"
#endif

/* define compiler specific symbols */
#if defined ( __CC_ARM   )
#define __ASM            __asm                                      /*!< asm keyword for ARM Compiler          */
#define __INLINE         __inline                                   /*!< inline keyword for ARM Compiler       */

#elif defined ( __ICCARM__ )
#define __ASM           __asm                                       /*!< asm keyword for IAR Compiler          */
#define __INLINE        inline                                      /*!< inline keyword for IAR Compiler. Only avaiable in High optimization mode! */

#elif defined   (  __GNUC__  )
#define __ASM            __asm                                      /*!< asm keyword for GNU Compiler          */
#define __INLINE         inline                                     /*!< inline keyword for GNU Compiler       */

#elif defined   (  __TASKING__  )
#define __ASM            __asm                                      /*!< asm keyword for TASKING Compiler      */
#define __INLINE         inline                                     /*!< inline keyword for TASKING Compiler   */

#endif


volatile int64_t Ticks;
volatile uint32_t TimingDelay;

/* pin_config[] moved to board/f103_bluepill/Src/board_pins.c as part of the
 * F411 BSP-seam refactor (Phase 1). The extern declaration in periphery.h
 * still resolves it. */


/**
  * @brief SysTick Configuration
  * @retval None
  */
void SysTick_Init(void) {
    /* CMSIS-portable: SystemCoreClock is updated by SystemInit on F103
     * (system_stm32f10x.c) and by Board_ClockInit_F411's
     * LL_SetSystemCoreClock(96000000) on F411. Avoids the F1-only
     * RCC_GetClocksFreq StdPeriph helper and works on both boards. */
    SysTick_Config(SystemCoreClock / 1000);
}

/**
  * @brief Timers Configuration
  * @retval None
  */
void Timers_Init(dev_config_t * p_dev_config)
{
	// Reset tick counter
	Ticks = 0;

	// Main tick (encoders, axis sampling, HID report cadence)
	Board_TickInit(2000);

	// LED PWM channels (TIM3 + optional TIM1) -- per-board impl behind the seam.
	Board_LedPwm_Init(p_dev_config);
}

/**
  * @brief Update PWM values
	* @param p_dev_config: Pointer to device config
	* @param axis_data: Pointer to axis values
  * @retval None
  */
void PWM_SetFromAxis(dev_config_t * p_dev_config, analog_data_t * axis_data)
{
	Board_LedPwm_SetFromAxis(p_dev_config, axis_data);
}


/**
  * @brief Get up-time milliseconds
  * @retval milliseconds
  */
int64_t GetMillis(void) 
{
    return Ticks/(TICKS_IN_MILLISECOND);
}


/**
  * @brief Delay implementation
  * @retval None
  */
void Delay_ms(uint32_t nTime) 
{
    TimingDelay = nTime;
    while (TimingDelay != 0);
}

/**
  * @brief Delay implementation
  * @retval None
  */
void Delay_us(uint32_t nTime) 
{
    int32_t us = nTime * 5;

    while (us > 0) {
        us--;
    }
}

/**
  * @brief Generator Initialization Function
  * @param None
  * @retval None
  */
void Generator_Init(void) {
#ifdef BOARD_F103_BLUEPILL
    /* TIM4_CH1 PWM at PB6 -> 4 MHz square wave for the TLE5011 GEN
     * input. */
    TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;
    GPIO_InitTypeDef GPIO_InitStructureure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM4, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    /* Time base configuration. APB1 timer clock = 72 MHz / 18 = 4 MHz. */
    TIM_TimeBaseStructure.TIM_Period = 18 - 1;
    TIM_TimeBaseStructure.TIM_Prescaler = 0;
    TIM_TimeBaseStructure.TIM_ClockDivision = 0;
    TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM4, &TIM_TimeBaseStructure);

    /* PWM1 Mode configuration: Channel1 */
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 9;
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC1Init(TIM4, &TIM_OCInitStructure);
    TIM_OC1PreloadConfig(TIM4, TIM_OCPreload_Enable);

    TIM_ARRPreloadConfig(TIM4, ENABLE);

    /*GPIOB Configuration: TIM4 channel1*/
    GPIO_InitStructureure.GPIO_Pin = GPIO_Pin_6;
    GPIO_InitStructureure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructureure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructureure);

    /* TIM4 enable counter */
    TIM_Cmd(TIM4, ENABLE);
#elif defined(BOARD_F411_BLACKPILL)
    /* TIM4_CH1 PB6 (slot 17) AF2 -> 4 MHz square wave for the TLE5011
     * GEN clock input. APB1 timer clock = 96 MHz; period 24 with no
     * prescaler -> 4 MHz, pulse 12 -> 50% duty. Same role as F103;
     * Encoder 2 mutex (also on TIM4_CH1) is enforced configurator-side. */
    LL_APB1_GRP1_EnableClock(LL_APB1_GRP1_PERIPH_TIM4);
    LL_AHB1_GRP1_EnableClock(LL_AHB1_GRP1_PERIPH_GPIOB);

    LL_TIM_InitTypeDef tim = {0};
    tim.Prescaler         = 0;
    tim.CounterMode       = LL_TIM_COUNTERMODE_UP;
    tim.Autoreload        = 24 - 1;
    tim.ClockDivision     = LL_TIM_CLOCKDIVISION_DIV1;
    tim.RepetitionCounter = 0;
    LL_TIM_Init(TIM4, &tim);
    LL_TIM_EnableARRPreload(TIM4);

    LL_TIM_OC_InitTypeDef oc = {0};
    oc.OCMode       = LL_TIM_OCMODE_PWM1;
    oc.OCState      = LL_TIM_OCSTATE_ENABLE;
    oc.OCPolarity   = LL_TIM_OCPOLARITY_HIGH;
    oc.OCIdleState  = LL_TIM_OCIDLESTATE_LOW;
    oc.CompareValue = 12;
    LL_TIM_OC_Init(TIM4, LL_TIM_CHANNEL_CH1, &oc);
    LL_TIM_OC_EnablePreload(TIM4, LL_TIM_CHANNEL_CH1);

    /* PB6 (slot 17) AF2 + push-pull. Use the BSP seams so this stays
     * consistent with the rest of IO_Init's mode/AF wiring. */
    Board_PinSetMode(17, BOARD_GPIO_AF_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);
    Board_PinSetAfRole(17, BOARD_AF_ROLE_TLE5011_GEN);

    LL_TIM_EnableCounter(TIM4);
#endif /* BOARD_xxx */
}

/* IO init function */
void IO_Init (dev_config_t * p_dev_config)
{
#ifdef BOARD_F103_BLUEPILL
	GPIO_InitTypeDef GPIO_InitStructure;

	/* F103 needs three things F411 doesn't:
	 * 1. AFIO global pin remap (TIM3 partial, JTAG release for PB3-5).
	 *    F411 routes via per-pin AF code via Board_PinSetMode below.
	 * 2. RCC GPIO port clock enable -- F411 enables on demand inside
	 *    Board_PinSetMode itself.
	 * 3. CRL/CRH reset to 0x4444... (input-floating). F411 GPIO regs
	 *    have a different layout (MODER/OTYPER/OSPEEDR/PUPDR); reset
	 *    state is already input-floating so no equivalent needed. */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_NoJTRST, ENABLE);
	GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
	GPIO_PinRemapConfig(GPIO_PartialRemap_TIM3, ENABLE);

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB,ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC,ENABLE);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_10MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
	GPIO_Init(GPIOC, &GPIO_InitStructure);

	while ((p_dev_config->firmware_version & 0xFFF0) != (FIRMWARE_VERSION & 0xFFF0))
	{
		// blink LED if firmware version doesnt match
		GPIOB->ODR ^= GPIO_Pin_12;
		GPIOC->ODR ^=	GPIO_Pin_13;
		Delay_ms(300);
	}

	// Reset GPIO
	GPIOA->CRL=0x44444444;
	GPIOA->CRH=0x44444444;
	GPIOA->ODR=0x0;
	GPIOB->CRL=0x44444444;
	GPIOB->CRH=0x44444444;
	GPIOB->ODR=0x0;
	GPIOC->CRL=0x44444444;
	GPIOC->CRH=0x44444444;
	GPIOC->ODR=0x0;
#endif



	// setting up GPIO according confgiguration
	for (int i=0; i<USED_PINS_NUM; i++)
	{
		// buttons
		if (p_dev_config->pins[i] == BUTTON_GND)
		{
			Board_PinSetMode(i, BOARD_GPIO_INPUT_PULLUP, BOARD_GPIO_SPEED_10MHZ);
		}
		else if (p_dev_config->pins[i] == BUTTON_VCC)
		{
			Board_PinSetMode(i, BOARD_GPIO_INPUT_PULLDOWN, BOARD_GPIO_SPEED_10MHZ);
		}
		else if (p_dev_config->pins[i] == BUTTON_COLUMN)
		{
			Board_PinSetMode(i, BOARD_GPIO_INPUT_PULLUP, BOARD_GPIO_SPEED_10MHZ);
		}
		else if (p_dev_config->pins[i] == BUTTON_ROW)
		{
			Board_PinSetMode(i, BOARD_GPIO_OUTPUT_OPENDRAIN, BOARD_GPIO_SPEED_10MHZ);
		}
		else if (p_dev_config->pins[i] == AXIS_ANALOG)
		{
			Board_PinSetMode(i, BOARD_GPIO_INPUT_ANALOG, BOARD_GPIO_SPEED_10MHZ);
		}
		else if (p_dev_config->pins[i] == SPI_SCK && (pin_config[i].caps & PIN_CAP_SPI_SCK))
		{
			Board_PinSetMode(i, BOARD_GPIO_AF_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);
		}
		else if (p_dev_config->pins[i] == SPI_MISO && (pin_config[i].caps & PIN_CAP_SPI_MISO))
		{
			Board_PinSetMode(i, BOARD_GPIO_INPUT_FLOATING, BOARD_GPIO_SPEED_50MHZ);
		}
		else if (p_dev_config->pins[i] == SPI_MOSI && (pin_config[i].caps & PIN_CAP_SPI_MOSI))
		{
			Board_PinSetMode(i, BOARD_GPIO_AF_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);

			SPI_Start();
		}
		else if (p_dev_config->pins[i] == I2C_SCL && (pin_config[i].caps & PIN_CAP_I2C_SCL))
		{
			I2C_Start();

			Board_PinSetMode(i, BOARD_GPIO_AF_OPENDRAIN, BOARD_GPIO_SPEED_50MHZ);
		}
		else if (p_dev_config->pins[i] == I2C_SDA && (pin_config[i].caps & PIN_CAP_I2C_SDA))
		{
			Board_PinSetMode(i, BOARD_GPIO_AF_OPENDRAIN, BOARD_GPIO_SPEED_50MHZ);
		}
		else if (p_dev_config->pins[i] == TLE5011_CS ||
						 p_dev_config->pins[i] == TLE5012_CS ||
						 p_dev_config->pins[i] == MCP3201_CS ||
						 p_dev_config->pins[i] == MCP3202_CS ||
						 p_dev_config->pins[i] == MCP3204_CS ||
						 p_dev_config->pins[i] == MCP3208_CS ||
						 p_dev_config->pins[i] == MLX90363_CS ||
						 p_dev_config->pins[i] == MLX90393_CS ||
						 p_dev_config->pins[i] == AS5048A_CS)
		{
			Board_PinSetMode(i, BOARD_GPIO_OUTPUT_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);
			Board_PinWrite(i, 1);
		}
		else if (p_dev_config->pins[i] == TLE5011_GEN && (pin_config[i].caps & PIN_CAP_TLE5011_GEN))
		{
			Generator_Init();	// 4MHz output at PB6 pin
		}
		else if (p_dev_config->pins[i] == SHIFT_REG_CLK)
		{
			Board_PinSetMode(i, BOARD_GPIO_OUTPUT_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);
			Board_PinWrite(i, 0);
		}
		else if (p_dev_config->pins[i] == SHIFT_REG_LATCH)
		{
			Board_PinSetMode(i, BOARD_GPIO_OUTPUT_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);
			Board_PinWrite(i, 1);
		}
		else if (p_dev_config->pins[i] == SHIFT_REG_DATA)
		{
			Board_PinSetMode(i, BOARD_GPIO_INPUT_FLOATING, BOARD_GPIO_SPEED_50MHZ);
		}
		else if (p_dev_config->pins[i] == LED_PWM)
		{
			Board_PinSetMode(i, BOARD_GPIO_AF_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);
		}
		else if (p_dev_config->pins[i] == LED_SINGLE)
		{
			Board_PinSetMode(i, BOARD_GPIO_OUTPUT_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);
		}
		else if (p_dev_config->pins[i] == LED_ROW)
		{
			Board_PinSetMode(i, BOARD_GPIO_OUTPUT_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);
			Board_PinWrite(i, 0);
		}
		else if (p_dev_config->pins[i] == LED_COLUMN)
		{
			Board_PinSetMode(i, BOARD_GPIO_OUTPUT_OPENDRAIN, BOARD_GPIO_SPEED_50MHZ);
			Board_PinWrite(i, 1);
		}
		else if (p_dev_config->pins[i] == FAST_ENCODER && (pin_config[i].caps & PIN_CAP_FAST_ENCODER))
		{
			Board_PinSetMode(i, BOARD_GPIO_INPUT_PULLUP, BOARD_GPIO_SPEED_50MHZ);
		}
		else if (p_dev_config->pins[i] == NOT_USED)
		{
			Board_PinSetMode(i, BOARD_GPIO_INPUT_PULLDOWN, BOARD_GPIO_SPEED_2MHZ);
		}
		else if (p_dev_config->pins[i] == LED_RGB_WS2812B && (pin_config[i].caps & PIN_CAP_LED_RGB))
		{
			ws2812b_Init(ARGB_WS2812B);
		}
		else if (p_dev_config->pins[i] == LED_RGB_PL9823 && (pin_config[i].caps & PIN_CAP_LED_RGB))
		{
			ws2812b_Init(ARGB_PL9823);
		}
		else if (p_dev_config->pins[i] == UART_TX && (pin_config[i].caps & PIN_CAP_UART_TX))
		{
			UART_Start();

			Board_PinSetMode(i, BOARD_GPIO_AF_PUSHPULL, BOARD_GPIO_SPEED_50MHZ);
		}
	}

#if defined(DEBUG) && defined(BOARD_F103_BLUEPILL)
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
    GPIO_Init(GPIOC, &GPIO_InitStructure);
#endif
}


