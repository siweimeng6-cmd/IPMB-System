#ifndef  __BSP_INIT_H
#define	 __BSP_INIT_H


#include "stm32f10x.h"
#include <stdio.h>
#include "stdbool.h"
#include "string.h"

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


#include ".\timer\bsp_timer.h"
#include ".\timer\bsp_fan_pwm.h"
#include ".\gpio\bsp_gpio.h"
#include ".\i2c\bsp_mo_i2c.h"
#include ".\adc\bsp_adc.h"
#include "rtos_task.h"
#include "./i2c/bsp_i2c.h"
#include ".\ipmb\ipmb_slave.h"



#define _MACROSILICON_TECH_MS928X_H_

//Debug print
#define PRINT_DEBUG( debug_flag, __msg )          do{ if(debug_flag) printf __msg;}while(0)
#define PRINT_ERROR( __msg )                      printf __msg

#define DEBUG_OFF                                 0
#define DEBUG_ON                                  1

#ifndef NULL
#define NULL ((void*)0)
#endif

#define  __CODE
#define  __XDATA
#define  __DATA
#define  __IDATA
#define  __NEAR
#ifndef  __IO
#define  __IO
#endif

#define PRINTF_BUF_SIZE     		 512
typedef struct
{
	unsigned short size;
	unsigned char  buf[PRINTF_BUF_SIZE];
}stPRINTF_BUF_t, *pstPRINTF_BUF_t;


extern stPRINTF_BUF_t stPrintf_Buf;


extern TaskHandle_t AppTaskCreate_Handle;

void delay_1ms(uint32_t count);
void AppTaskCreate(void);/* ���ڴ������� */
void BSP_Init(void);/* ���ڳ�ʼ�����������Դ */
void Delay_ms(uint32_t count);
void Delay_us(uint32_t count);


#endif 

