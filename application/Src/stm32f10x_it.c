/**
  ******************************************************************************
  * @file    Project/STM32F10x_StdPeriph_Template/stm32f10x_it.c 
  * @author  MCD Application Team
  * @version V3.5.0
  * @date    08-April-2011
  * @brief   Main Interrupt Service Routines.
  *          This file provides template for all exceptions handler and 
  *          peripherals interrupt service routine.
  ******************************************************************************
  * @attention
  *
  * THE PRESENT FIRMWARE WHICH IS FOR GUIDANCE ONLY AIMS AT PROVIDING CUSTOMERS
  * WITH CODING INFORMATION REGARDING THEIR PRODUCTS IN ORDER FOR THEM TO SAVE
  * TIME. AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY
  * DIRECT, INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING
  * FROM THE CONTENT OF SUCH FIRMWARE AND/OR THE USE MADE BY CUSTOMERS OF THE
  * CODING INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
  *
  * <h2><center>&copy; COPYRIGHT 2011 STMicroelectronics</center></h2>
  ******************************************************************************
  */

/* Includes ------------------------------------------------------------------*/
#include "stm32f10x_it.h"
#include "stm32f10x_usart.h"

#include "usb_istr.h"
#include "usb_lib.h"
#include "periphery.h"
#include "analog.h"
#include "encoders.h"
#include "tle5011.h"
#include "tle5012.h"
#include "mcp320x.h"
#include "mlx90363.h"
#include "mlx90393.h"
#include "as5048a.h"
#include "ads1115.h"
#include "as5600.h"
#include "sensor_dispatch.h"
#include "config.h"
#include "uart.h"

/** @addtogroup STM32F10x_StdPeriph_Template
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

#define ADC_PERIOD_TICKS										4					// 1 tick = 500us
#define SENSORS_PERIOD_TICKS								4
//#define BUTTONS_PERIOD_TICKS								4
//#define ENCODERS_PERIOD_TICKS								1
#define UART_PERIOD_TICKS										20

/* Private variables ---------------------------------------------------------*/

/* Per-tick report buffers + time counters relocated to
 * application/Src/usb_app.c during Phase 4D so the F411 build can run
 * the same Board_TickISR body. The definitions there are extern-visible;
 * declare here only the ones still referenced by F103-only IRQ handlers
 * below (DMA1_Channel*, I2C2_ER_IRQHandler etc.). */
extern volatile int status;
extern dev_config_t dev_config;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/******************************************************************************/
/*            Cortex-M3 Processor Exceptions Handlers                         */
/******************************************************************************/

/**
  * @brief  This function handles NMI exception.
  * @param  None
  * @retval None
  */
void NMI_Handler(void)
{
}

/**
  * @brief  This function handles Hard Fault exception.
  * @param  None
  * @retval None
  */
void HardFault_Handler(void)
{
  /* Go to infinite loop when Hard Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Memory Manage exception.
  * @param  None
  * @retval None
  */
void MemManage_Handler(void)
{
  /* Go to infinite loop when Memory Manage exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Bus Fault exception.
  * @param  None
  * @retval None
  */
void BusFault_Handler(void)
{
  /* Go to infinite loop when Bus Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles Usage Fault exception.
  * @param  None
  * @retval None
  */
void UsageFault_Handler(void)
{
  /* Go to infinite loop when Usage Fault exception occurs */
  while (1)
  {
  }
}

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
}

/**
  * @brief  This function handles Debug Monitor exception.
  * @param  None
  * @retval None
  */
void DebugMon_Handler(void)
{
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
	if (TimingDelay != 0x00)										
  {
    TimingDelay--;
  }
}

/******************************************************************************/
/*                 STM32F10x Peripherals Interrupt Handlers                   */
/*  Add here the Interrupt Handler for the used peripheral(s) (PPP), for the  */
/*  available peripheral interrupt handler's name please refer to the startup */
/*  file (startup_stm32f10x_xx.s).                                            */
/******************************************************************************/


/* Main tick body relocated to application/Src/usb_app.c (Phase 4D)
 * so both F103 and F411 run the same code. The board's TIM2 IRQ
 * handler in board/<chip>/Src/board_tick.c calls Board_TickISR which
 * is now defined in usb_app.c. */

/* SPI/I2C DMA-completion sensor chain dispatch lives in
 * application/Src/sensor_dispatch.c so the F411 IT file can call the
 * same bodies. The F103 IRQs below clear DMA flags + disable the
 * channel (StdPeriph) and delegate. */

void DMA1_Channel2_IRQHandler(void)
{
	if (DMA_GetITStatus(DMA1_IT_TC2)) {
		DMA_ClearITPendingBit(DMA1_IT_TC2);
		DMA_Cmd(DMA1_Channel2, DISABLE);
		Sensor_OnSpiRxComplete();
	}
}

void DMA1_Channel3_IRQHandler(void)
{
	if (DMA_GetITStatus(DMA1_IT_TC3)) {
		DMA_ClearITPendingBit(DMA1_IT_TC3);
		DMA_Cmd(DMA1_Channel3, DISABLE);
		Sensor_OnSpiTxComplete();
	}
}

void DMA1_Channel4_IRQHandler(void)
{
	if (DMA_GetFlagStatus(DMA1_FLAG_TC4)) {
		DMA_ClearFlag(DMA1_FLAG_TC4);
		I2C_DMACmd(I2C2, DISABLE);
		DMA_Cmd(DMA1_Channel4, DISABLE);
		Sensor_OnI2cTxComplete();
	}
}

void DMA1_Channel5_IRQHandler(void)
{
	if (DMA_GetFlagStatus(DMA1_FLAG_TC5)) {
		DMA_ClearFlag(DMA1_FLAG_TC5);
		I2C_DMACmd(I2C2, DISABLE);
		DMA_Cmd(DMA1_Channel5, DISABLE);
		Sensor_OnI2cRxComplete();
	}
}

// I2C error
void I2C2_ER_IRQHandler(void)
{
	__IO uint32_t SR1Register =0;

	/* Read the I2C2 status register */
	SR1Register = I2C2->SR1;
	/* If AF = 1 */
	if ((SR1Register & 0x0400) == 0x0400)
	{
		I2C2->SR1 &= 0xFBFF;
		SR1Register = 0;
	}
	/* If ARLO = 1 */
	if ((SR1Register & 0x0200) == 0x0200)
	{
		I2C2->SR1 &= 0xFBFF;
		SR1Register = 0;
	}
	/* If BERR = 1 */
	if ((SR1Register & 0x0100) == 0x0100)
	{
		I2C2->SR1 &= 0xFEFF;
		SR1Register = 0;
	}

	/* If OVR = 1 */
	if ((SR1Register & 0x0800) == 0x0800)
	{
		I2C2->SR1 &= 0xF7FF;
		SR1Register = 0;
	}
		
	// Reset I2C
	I2C2->CR1 |= I2C_CR1_SWRST;
	I2C2->CR1 &= ~I2C_CR1_SWRST;
	I2C_Start();
}



/**
* @brief This function handles USB low priority or CAN RX0 interrupts.
*/
void USB_LP_CAN1_RX0_IRQHandler(void)
{	
	USB_Istr();
}

/**
  * @}
  */ 


/******************* (C) COPYRIGHT 2011 STMicroelectronics *****END OF FILE****/
