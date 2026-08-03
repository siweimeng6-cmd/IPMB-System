#ifndef __BSP_INIT_H
#define	__BSP_INIT_H


#include "stm32f10x.h"
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"


#include ".\gpio\bsp_gpio.h"
#include ".\timer\bsp_timer.h"
#include ".\timer\bsp_pwm.h"
#include ".\ipmb\ipmb_slave.h"      /* IPMB 从机协议栈 */


//Debug print
#define PRINT_DEBUG( debug_flag, __msg )          do{ if(debug_flag) printf __msg;}while(0)
#define PRINT_ERROR( __msg )                      printf __msg

#define DEBUG_OFF                                 0
#define DEBUG_ON                                  1
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
void pwm_func(void);/* �����¶ȿ��Ʒ���PWM */

#endif 

