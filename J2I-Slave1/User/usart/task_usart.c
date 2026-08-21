#include "rtos_task.h"
#include "bsp_init.h"
#include ".\\gpio\\bsp_gpio.h"
#include ".\\i2c\\bsp_eeprom.h"
#include ".\\ipmb\\ipmb_sensor.h"  /* IPMB_Sensor_CheckThresholds,2026-08-21新增 */
#include "task_fw_update.h"
#include <stdlib.h>   /* strtof()，用于解析主机上报的"CPU_TEMP="温度参数 */

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

		printf(">>> 本固件为 OTA 升级测试版本 <<<\r\n");
		Board_ADDR90_temp();
		Board_ADDR92_temp();
		Board_ADDR94_temp();

		ADC_Calculation();
		IPMB_Sensor_CheckThresholds();   /* 2026-08-21新增:紧跟在温度/ADC刷新之后,
		                                   * 读最新数据做报警/断电阈值检测 */
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

		{
			/* PB5(PAYLD_SYSPOWER)是主板电源状态硬件反馈脚: 低=已上电 高=已下电。
			 * mainboard_power_state 是防抖确认后的软状态, power_state_confirmed=0
			 * 表示MCU上电未满10s、PB5还没开始监测, 此时软状态不可信 */
			char pwr_buf[64];
			uint8_t pb5 = GPIO_ReadInputDataBit(PAYLD_SYSPOWER_GPIO_PORT, PAYLD_SYSPOWER_GPIO_PIN);
			sprintf(pwr_buf, "PB5:%u(%s) Power:%s%s\r\n",
			        (unsigned int)pb5, pb5 ? "high=off" : "low=on",
			        mainboard_power_state ? "ON" : "OFF",
			        power_state_confirmed ? "" : "(unconfirmed)");
			if ((strlen((char *)stPrintf_Buf.buf) + strlen(pwr_buf)) < PRINTF_BUF_SIZE)
			{
				strcat((char *)stPrintf_Buf.buf, pwr_buf);
			}
		}

		{
			char cpu_temp_buf[32];
			int  cpu_t;
			uint8_t fresh = Fan_GetCpuTempDebug(&cpu_t);
			sprintf(cpu_temp_buf, "CpuTemp:%dC(%s)\r\n", cpu_t, fresh ? "fresh" : "stale");
			if ((strlen((char *)stPrintf_Buf.buf) + strlen(cpu_temp_buf)) < PRINTF_BUF_SIZE)
			{
				strcat((char *)stPrintf_Buf.buf, cpu_temp_buf);
			}
		}

		#if MO_IIC
		/* 【2026-08-20移植自BPCOME-3502】累计运行时长:只走本地debug串口,不进
		 * stPrintf_Buf/stCpu_Buf——那条路径会在主机发'?'查询时把内容转发给主机,
		 * 这个字段目前只是本地调试信息,不对外暴露 */
		Runtime_Task_Update();
		printf("[RUNTIME] 累计运行时间: %u小时%u分钟(距下次写入EEPROM还剩%u分钟)\r\n",
			(unsigned int)(g_runtime_total_minutes / 60), (unsigned int)(g_runtime_total_minutes % 60),
			(unsigned int)g_runtime_countdown_minutes);
		#endif

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
			if(stUart5_recv_Data.recv_buf[0] == 0xA0)
				{
					/* IPMI Basic Mode 成帧的固件升级命令(参照友商 ipmudtool 协议),
					 * 见 task_fw_update.c */
					FwUpdate_HandleFrame(stUart5_recv_Data.recv_buf, stUart5_recv_Data.recv_data_len);
					stUart5_recv_Data.recv_data_len = 0;
				}
				else if(stUart5_recv_Data.recv_buf[0] == '?')
				{
					printf("CPU获取板信息\r\n");
					/* 按 size 发送, 不依赖 '\0' 结尾: stCpu_Buf.buf 从未清零过,
					 * 若本轮内容比上一轮短, Usart_SendString 会把上一轮残留的
					 * 尾部垃圾也一起发出去 */
					Usart_SendArray( COM_USART5, stCpu_Buf.buf, stCpu_Buf.size);
					stCpu_Buf.size=0;
					stUart5_recv_Data.recv_data_len = 0;
				}
				else if(stUart5_recv_Data.recv_data_len >= 9 &&
				        strncmp((char *)stUart5_recv_Data.recv_buf, "CPU_TEMP=", 9) == 0)
				{
					/* 主机(飞腾D2000)每秒上报一次的CPU温度, 格式"CPU_TEMP=45.2C\n";
					 * recv_buf不保证'\0'结尾, 先按recv_data_len截断拷贝到本地定长缓冲区再补'\0' */
					char temp_buf[32];
					char *endptr;
					float t;
					uint16_t len = stUart5_recv_Data.recv_data_len;
					if(len >= sizeof(temp_buf)) len = sizeof(temp_buf) - 1;
					memcpy(temp_buf, stUart5_recv_Data.recv_buf, len);
					temp_buf[len] = '\0';

					t = strtof(temp_buf + 9, &endptr);
					if(endptr != temp_buf + 9)
						Fan_UpdateCpuReportedTemp((int)t);

					stUart5_recv_Data.recv_data_len = 0;
				}
				else//既不是升级帧、？、也不是CPU_TEMP上报，不处理直接清空缓存
					stUart5_recv_Data.recv_data_len = 0;
#endif
		}
	}
}

