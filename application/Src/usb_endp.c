/**
  ******************************************************************************
  * @file    usb_endp.c
  * @author  MCD Application Team
  * @version V4.1.0
  * @date    26-May-2017
  * @brief   Endpoint routines
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; COPYRIGHT(c) 2017 STMicroelectronics</center></h2>
  *
  * Redistribution and use in source and binary forms, with or without modification,
  * are permitted provided that the following conditions are met:
  *   1. Redistributions of source code must retain the above copyright notice,
  *      this list of conditions and the following disclaimer.
  *   2. Redistributions in binary form must reproduce the above copyright notice,
  *      this list of conditions and the following disclaimer in the documentation
  *      and/or other materials provided with the distribution.
  *   3. Neither the name of STMicroelectronics nor the names of its contributors
  *      may be used to endorse or promote products derived from this software
  *      without specific prior written permission.
  *
  * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
  *
  ******************************************************************************
  */


/* Includes ------------------------------------------------------------------*/

#include "usb_endp.h"
#include "usb_hw.h"
#include "usb_lib.h"
#include "usb_istr.h"
#include "usb_pwr.h"
#include "usb_cdc_conf.h"
#include "simhub.h"
#include "leds.h"
#include "led_effects.h"

#include "config.h"
#include "crc16.h"

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
volatile extern uint8_t bootloader;
volatile extern int32_t joy_millis;
volatile extern int32_t configurator_millis;
volatile extern int64_t encoder_ticks;
volatile extern int64_t adc_ticks;
volatile extern int64_t sensors_ticks;
volatile extern int64_t buttons_ticks;

__IO uint8_t EP1_PrevXferComplete = 1;
__IO uint8_t EP2_PrevXferComplete = 1;
__IO uint8_t EP3_PrevXferComplete = 1;
__IO uint8_t EP4_PrevXferComplete = 1;
__IO uint8_t EP5_PrevXferComplete = 1;

static volatile uint8_t receive_buffer[CDC_DATA_SIZE];
static volatile uint32_t receive_length = 0;

/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/*******************************************************************************
* Function Name  : EP1_OUT_Callback.
* Description    : EP1 OUT Callback Routine.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void EP1_OUT_Callback(void)
{
	SetEPRxStatus(ENDP1, EP_RX_VALID);
}

/*******************************************************************************
* Function Name  : EP2_OUT_Callback.
* Description    : EP2 OUT Callback Routine.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
/* Phase 4D: chip-agnostic OUT-dispatch lives in application/Src/usb_app.c.
 * Forward declare locally to avoid pulling in the full board_usb.h /
 * usb_app.h surface from this F1-USB-stack file. */
extern void App_HidOutDispatch(const uint8_t *hid_buf);

void EP2_OUT_Callback(void)
{
	uint8_t hid_buf[64];

	USB_SIL_Read(EP2_OUT, hid_buf);
	App_HidOutDispatch(hid_buf);

	/* F1-only EP rearm. F411's USBD_CUSTOM_HID class auto-rearms after
	 * OutEvent returns. */
	SetEPRxStatus(ENDP2, EP_RX_VALID);
}


/*******************************************************************************
* Function Name  : EP4_OUT_Callback.
* Description    : EP4 OUT Callback Routine.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void EP4_OUT_Callback(void)
{
	receive_length = USB_SIL_Read(CDC_DATA_OUT_ENDP_ADR, (uint8_t *)receive_buffer);
	uint16_t free_size = MAX_RING_BIF_SIZE;
	
	if (receive_length > 0)
	{
		free_size = SH_ProcessIncomingData((uint8_t *)receive_buffer, receive_length);
	}
	if (free_size > SH_PACKET_SIZE)
	{
		SetEPRxValid(CDC_DATA_OUT_ENDP_NUM);
	}
}

/*******************************************************************************
* Function Name  : EP1_IN_Callback.
* Description    : EP1 IN Callback Routine.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void EP1_IN_Callback(void)
{
  EP1_PrevXferComplete = 1;
}

/*******************************************************************************
* Function Name  : EP2_IN_Callback.
* Description    : EP2 IN Callback Routine.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void EP2_IN_Callback(void)
{
  EP2_PrevXferComplete = 1;
}
/*******************************************************************************
* Function Name  : EP4_IN_Callback.
* Description    : EP4 IN Callback Routine.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void EP5_IN_Callback(void)
{
  EP5_PrevXferComplete = 1;
}

/*******************************************************************************
* Function Name  : SH_ProcessEndpData.
* Description    : SH Process Endp Data.
* Input          : None.
* Output         : None.
* Return         : None.
*******************************************************************************/
void SH_ProcessEndpData(void)
{
	if (SH_BufferFreeSize() > SH_PACKET_SIZE)
	{
		SetEPRxValid(CDC_DATA_OUT_ENDP_NUM);
	}
}

/*******************************************************************************
* Function Name  : USB_CUSTOM_HID_SendReport.
* Description    : 
* Input          : None.
* Output         : None.
* Return         : 0 if success otherwise -1.
*******************************************************************************/
int8_t USB_CUSTOM_HID_SendReport(uint8_t EP_num, uint8_t * data, uint8_t length)
{
	if ((EP_num == 1) && (EP1_PrevXferComplete) && (bDeviceState == CONFIGURED))
	{
			USB_SIL_Write(EP1_IN, data, length);
			SetEPTxValid(ENDP1);
			EP1_PrevXferComplete = 0;
			return 0;
	}
	else if ((EP_num == 2) && (EP2_PrevXferComplete) && (bDeviceState == CONFIGURED))
	{
			USB_SIL_Write(EP2_IN, data, length);
			SetEPTxValid(ENDP2);
			EP2_PrevXferComplete = 0;
			return 0;
	}
	return -1;
}

/*******************************************************************************
* Function Name  : Send DATA .
* Description    : send the data received from the STM32 to the PC through USB
* Input          : None.
* Output         : None.
* Return         : 0 if success otherwise -1.			// sdelal kak sverhu. ebanii v rot nahuia -1 vmesto 1 vo vremia success
*******************************************************************************/
int8_t CDC_Send_DATA (uint8_t *ptrBuffer, uint8_t send_length)
{
  /*if max buffer is Not reached*/
  if(EP5_PrevXferComplete && bDeviceState == CONFIGURED && send_length < CDC_DATA_SIZE)     
  {
    /* send  packet*/
		USB_SIL_Write(CDC_DATA_IN_ENDP_ADR, (unsigned char*)ptrBuffer, send_length);
		// should be in this order
		EP5_PrevXferComplete = 0;
		SetEPTxValid(CDC_DATA_IN_ENDP_NUM);
  }
  else
  {
    return 0;
  }
  return -1;
}

/*******************************************************************************
* Function Name  : Is Ready To Send DATA .
* Description    : Checking the port for readiness to send files.
* Input          : None.
* Output         : None.
* Return         : 1 if success otherwise 0.
*******************************************************************************/
uint8_t CDC_IsReadeToSend()
{
	return EP5_PrevXferComplete;
}
/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
