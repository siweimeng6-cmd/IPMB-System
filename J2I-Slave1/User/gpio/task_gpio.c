#include "bsp_init.h"
#include <stdlib.h>   /* strtol()，用于解析Fan1/Fan2命令的占空比参数 */


xTaskHandle xExitLine2Semaphore    = NULL;
xTaskHandle xExitLine4Semaphore    = NULL; 
xTaskHandle xExitLine5Semaphore    = NULL; 
xTaskHandle xswitchhdmi3Semaphore  = NULL;

TaskHandle_t HDMI1_SWICTH_Task_Handle = NULL;
TaskHandle_t HDMI2_SWICTH_Task_Handle = NULL;
TaskHandle_t HDMI3_SWICTH_Task_Handle = NULL;
TaskHandle_t HDMI4_SWICTH_Task_Handle = NULL;
TaskHandle_t get_P5V_Task_Handle = NULL;

uint8_t hdmi_state_flag =0x00;		//HDMI0工作，初始态为HDMI0
uint8_t pre_p5v_state = 0;
uint8_t now_p5v_state = 0;
uint8_t p5v_good = 0;
static uint8_t hdp_input_cnt = 0;

/*																						
*********************************************************************************************************
*	函 数 名: HDMI1_SWICTH_Task(void* parameter)
*	功能说明: HDMI1对应PC2,外部中断，10ms定时防抖，HDMI指示灯低电平有效
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void HDMI1_SWICTH_Task(void* parameter)
{
	while(1)
	{
			if(xSemaphoreTake(xExitLine2Semaphore,(TickType_t)0) == pdTRUE)				//HDMI1对应PB2
			{
			}

	}
}

/*
*********************************************************************************************************
*	函 数 名: Fan_ParseDutyArg
*	功能说明: 从"Fan1 75"这类命令的参数部分解析出0-100之间的占空比整数。
*	          arg必须以'\0'结尾；strtol在第一个非数字字符处停止，天然忽略末尾的\r\n。
*	形    参：arg: 命令前缀之后的剩余字符串   out_val: 解析结果(已限幅0-100)
*	返 回 值: 1=解析到合法数字  0=没有数字参数(格式错误)
*********************************************************************************************************
*/
static uint8_t Fan_ParseDutyArg(const char *arg, uint8_t *out_val)
{
	char *endptr;
	long val;

	while(*arg == ' ') arg++;
	if(*arg == '\0') return 0;

	val = strtol(arg, &endptr, 10);
	if(endptr == arg) return 0;

	if(val < 0)   val = 0;
	if(val > 100) val = 100;
	*out_val = (uint8_t)val;
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: Power_Ctrl_Task
*	功能说明: 监控主板电源状态反馈信号(PB5)，低电平=已上电，高电平=已下电(硬件下电或软关机完成)；
*	          同时解析调试串口(UART4)输入的"Power on"/"Power off"/"Fan1"/"Fan2"/"Fan auto"等命令，
*	          软控制PWR_CTRL(PA4)及风扇手动占空比覆盖
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void Power_Ctrl_Task(void* parameter)
{
	uint8_t cur_level;
	uint8_t stable_level = 1;              //默认按“下电”处理，等首次稳定读数后刷新
	uint8_t debounce_cnt = 0;
	const uint8_t DEBOUNCE_THRESHOLD = 3;  //连续3次(约150ms)读数一致才确认状态变化
	uint8_t led_cnt = 0;
	uint8_t led_state = 0;                 //状态灯(PA15)当前电平，2Hz闪烁(周期500ms)
	uint16_t startup_delay_cnt = 0;
	const uint16_t STARTUP_DELAY_THRESHOLD = 200;  //200*50ms=10000ms，MCU上电满10s后才开始监测PB5
	uint8_t startup_done = 0;

	while(1)
	{
		if(xSemaphoreTake(xUart4Semaphore,(TickType_t)0) == pdTRUE)
		{
			/* 本地定长、'\0'结尾副本，仅供Fan1/Fan2数字参数解析使用；
			 * recv_buf本身不保证以'\0'结尾，直接对它调用strtol存在越界读到陈旧字节的风险。
			 * 已有的Power on/Power off/Reset分支继续沿用strncmp+固定长度，不受影响。 */
			char cmd_buf[16];
			uint16_t cmd_len = stUart4_recv_Data.recv_data_len;
			if(cmd_len >= sizeof(cmd_buf)) cmd_len = sizeof(cmd_buf) - 1;
			memcpy(cmd_buf, stUart4_recv_Data.recv_buf, cmd_len);
			cmd_buf[cmd_len] = '\0';

			if(strncmp((char *)stUart4_recv_Data.recv_buf,"Power on",8) == 0)
			{
				Power_On();
				mainboard_power_state = 1;
				power_state_confirmed = 1;
				printf("Power on命令已收到，主板上电\r\n");
			}
			else if(strncmp((char *)stUart4_recv_Data.recv_buf,"Power off",9) == 0)
			{
				Power_Off();
				mainboard_power_state = 0;
				power_state_confirmed = 1;
				printf("Power off命令已收到，主板下电\r\n");
			}
			else if(strncmp((char *)stUart4_recv_Data.recv_buf,"Reset",5) == 0)
			{
				MCU_Reset_Pulse();
				printf("Reset命令已收到，PA12已输出复位脉冲\r\n");
			}
			else if(strncmp(cmd_buf,"Fan1",4) == 0)
			{
				uint8_t duty;
				if(Fan_ParseDutyArg(cmd_buf + 4, &duty))
				{
					Fan_SetManualDuty(FAN_CH1, duty);
					printf("Fan1命令已收到，风扇1已切换为手动模式，占空比=%u%% \r\n", duty);
				}
				else
					printf("Fan1命令格式错误，应为Fan1 <0-100>\r\n");
			}
			else if(strncmp(cmd_buf,"Fan2",4) == 0)
			{
				uint8_t duty;
				if(Fan_ParseDutyArg(cmd_buf + 4, &duty))
				{
					Fan_SetManualDuty(FAN_CH2, duty);
					printf("Fan2命令已收到，风扇2已切换为手动模式，占空比=%u%% \r\n", duty);
				}
				else
					printf("Fan2命令格式错误，应为Fan2 <0-100>\r\n");
			}
			else if(strncmp(cmd_buf,"Fan auto",8) == 0)
			{
				Fan_ClearManualOverride();
				printf("Fan auto命令已收到，风扇1/2已恢复自动温控模式\r\n");
			}
			stUart4_recv_Data.recv_data_len = 0;
		}

		if(ipmb_reset_flag)          //IPMB Set FRU Activation(cmd 0x0C)复位子命令请求的异步复位脉冲
		{
			ipmb_reset_flag = 0;
			MCU_Reset_Pulse();
			printf("IPMB Reset请求已处理，PA12已输出复位脉冲\r\n");
		}

		if(!startup_done)
		{
			startup_delay_cnt++;
			if(startup_delay_cnt >= STARTUP_DELAY_THRESHOLD)
			{
				startup_done = 1;
				power_state_confirmed = 1;   //PB5硬件反馈已开始生效，mainboard_power_state从此可信
				printf("PB5监测已启动(MCU上电满10s)\r\n");
			}
		}
		else
		{
			cur_level = GPIO_ReadInputDataBit(PAYLD_SYSPOWER_GPIO_PORT, PAYLD_SYSPOWER_GPIO_PIN);

			if(cur_level != stable_level)
			{
				debounce_cnt++;
				if(debounce_cnt >= DEBOUNCE_THRESHOLD)
				{
					stable_level = cur_level;
					debounce_cnt = 0;
					mainboard_power_state = (stable_level == 0) ? 1 : 0;  //低=上电(1)，高=下电(0)
					//printf("PB5%%d\r\n", stable_level == 0 ? "low" : "high", stable_level);

					if(stable_level == 0)
					{
						Power_On();		//PB5低 → 驱动PA4低，镜像上电
						printf("mainboard power state changed: ON (PA4 driven low)\r\n");

					}
					else
					{
						Power_Off();	//PB5高 → 驱动PA4高，镜像下电
						printf("mainboard power state changed: OFF (PA4 driven high)\r\n");
                       
					}
				}
			}
			else
			{
				debounce_cnt = 0;
			}
		}

		if(mainboard_power_state)    //主板上电时才闪烁，下电后常灭
		{
			led_cnt++;
			if(led_cnt >= 5)         //50ms*5=250ms，2Hz闪烁(500ms一个完整周期)
			{
				led_cnt = 0;
				led_state = !led_state;
				GPIO_WriteBit(MCU_STATUS_LED_GPIO_PORT, MCU_STATUS_LED_GPIO_PIN, (BitAction)led_state);
			}
		}
		else
		{
			led_cnt = 0;
			led_state = 0;
			GPIO_WriteBit(MCU_STATUS_LED_GPIO_PORT, MCU_STATUS_LED_GPIO_PIN, (BitAction)1);
		}

		vTaskDelay(50);
	}
}





