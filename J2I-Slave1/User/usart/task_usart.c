#include "rtos_task.h"
#include "bsp_init.h"
#include ".\\gpio\\bsp_gpio.h"

stPRINTF_BUF_t stPrintf_Buf;
stPRINTF_BUF_t stCpu_Buf;
uint8_t power_on_cnt=0;

/*
*********************************************************************************************************
*	函 数 名: Sensor_Task
*	功能说明: Sensor_Task任务主体
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void Sensor_Task(void* parameter)
{
	uint8_t i;
	uint8_t ga0, ga1, ga2, ga3, ga4, gap, ga_raw;
	char ga_buf[96];
	char fan_buf[48];
	while (1)
  {
		Board_ADDR90_temp();
		Board_ADDR92_temp();
		Board_ADDR94_temp();

		ADC_Calculation();
//		pwm_func();

		/* Read each GA pin in this cycle.  CPEX encoding is active-low:
		 * high/open=0 and low/GND=1 for the calculated slot number. */
		ga0 = GPIO_ReadInputDataBit(MCU_GA0_GPIO_PORT, MCU_GA0_GPIO_PIN);
		ga1 = GPIO_ReadInputDataBit(MCU_GA1_GPIO_PORT, MCU_GA1_GPIO_PIN);
		ga2 = GPIO_ReadInputDataBit(MCU_GA2_GPIO_PORT, MCU_GA2_GPIO_PIN);
		ga3 = GPIO_ReadInputDataBit(MCU_GA3_GPIO_PORT, MCU_GA3_GPIO_PIN);
		ga4 = GPIO_ReadInputDataBit(MCU_GA4_GPIO_PORT, MCU_GA4_GPIO_PIN);
		gap = GPIO_ReadInputDataBit(MCU_GAP_GPIO_PORT, MCU_GAP_GPIO_PIN);
		ga_raw = (uint8_t)(ga0 | (ga1 << 1) | (ga2 << 2) |
		                   (ga3 << 3) | (ga4 << 4));
		sprintf(ga_buf, "GA0:%u GA1:%u GA2:%u GA3:%u GA4:%u GAP:%u Slot:%u\r\n",
		        (unsigned int)ga0, (unsigned int)ga1, (unsigned int)ga2,
		        (unsigned int)ga3, (unsigned int)ga4, (unsigned int)gap,
		        (unsigned int)((~ga_raw) & 0x1F));
		if ((strlen((char *)stPrintf_Buf.buf) + strlen(ga_buf)) < PRINTF_BUF_SIZE)
		{
			strcat((char *)stPrintf_Buf.buf, ga_buf);
		}

		sprintf(fan_buf, "Fan1:%u%% Fan2:%u%%\r\n",
		        (unsigned int)Fan_GetCurrentDuty(FAN_CH1), (unsigned int)Fan_GetCurrentDuty(FAN_CH2));
		if ((strlen((char *)stPrintf_Buf.buf) + strlen(fan_buf)) < PRINTF_BUF_SIZE)
		{
			strcat((char *)stPrintf_Buf.buf, fan_buf);
		}

		stPrintf_Buf.size = strlen((char *)stPrintf_Buf.buf);
		printf("\r\n");
		printf("/*******************TEST DATA********************/");
		printf("\r\n");
		Usart_SendString( DEBUG_USARTx, (char *)stPrintf_Buf.buf);
		printf("/************************************************/");
		printf("\r\n");
		memcpy(stCpu_Buf.buf,stPrintf_Buf.buf,stPrintf_Buf.size);
		stCpu_Buf.size = stPrintf_Buf.size;
		memset(stPrintf_Buf.buf, 0, stPrintf_Buf.size);					//打印记录完成，清除缓存，避免任务溢出
		stPrintf_Buf.size = 0;

		vTaskDelay(2000);
	}
}

/*																						
*********************************************************************************************************
*	函 数 名: Sensor_Task
*	功能说明: Sensor_Task任务主体
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void UART5_Task(void* parameter)
{	
	uint8_t i;
	while (1)
  {
		if(xSemaphoreTake(xUart5Semaphore,(TickType_t)0) == pdTRUE)
		{
#if 0
				printf("usart5 receive code:");
				for(i=0;i<stUart5_recv_Data.recv_data_len;i++)
					printf("%c",stUart5_recv_Data.recv_buf[i]);
				printf("\r\n");
				stUart5_recv_Data.recv_data_len = 0;
#else
			if(stUart5_recv_Data.recv_buf[0] == '?')
				{
					printf("CPU获取板信息\r\n");
					Usart_SendString( COM_USART5, (char *)stCpu_Buf.buf);
					stCpu_Buf.size=0;
					stUart5_recv_Data.recv_data_len = 0;
				}	
				else//如果不是？，不处理直接清空缓存
					stUart5_recv_Data.recv_data_len = 0;
#endif
		}
	}
}

