#include "bsp_init.h"

//#define NO_TIMER
uint8_t out_pwr_cnt = 0;
uint8_t reset_pwr_cnt = 0;
uint8_t out_pwr_cnt_flag = 0;
uint8_t reset_pwr_cnt_flag = 0;
uint8_t timer_open_flag = 0;//避免定时器开启后，再次收到指定调用定时器。先启用定时器计数的指令会提前关闭中断

TaskHandle_t CAN1_Task_Handle = NULL;
TaskHandle_t CAN2_Task_Handle = NULL;

xTaskHandle xCan1Semaphore    = NULL; 
xTaskHandle xCan2Semaphore    = NULL; 
/*																						
*********************************************************************************************************
*	函 数 名: CAN1_Task
*	功能说明: CAN1_Task任务主体
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void CAN1_Task(void* parameter)
{	
	uint8_t i;
  while (1)
  {
		if(xSemaphoreTake(xCan1Semaphore,(TickType_t)portMAX_DELAY) == pdTRUE)
		{
			printf("CAN1 receive:");
			for(i=0;i<8;i++)
				printf("0x%x ",Rx1Message.Data[i]);
			printf("\r\n");
		}
  }
}

/*																						
*********************************************************************************************************
*	函 数 名: CAN1_Task
*	功能说明: CAN1_Task任务主体
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void CAN2_Task(void* parameter)
{	
	uint16_t i;
  while (1)
  {
			if(xSemaphoreTake(xCan2Semaphore,(TickType_t)portMAX_DELAY) == pdTRUE)
			{
				printf("CAN2 receive:");
				for(i=0;i<8;i++)
					printf("0x%x ",Rx2Message.Data[i]);
				printf("\r\n");

			}
				
  }
}