#include "bsp_init.h"


/*																						
*********************************************************************************************************
*	函 数 名: COM_I2C1_Task
*	功能说明: COM_I2C1_Task任务主体
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void COM_I2C1_Task(void* parameter)
{
	uint8_t i;
	while(1)
	{
		if(xSemaphoreTake(xi2c1Semaphore,(TickType_t)0) == pdTRUE)
		{
			  printf("\n I2C1 读出成功，读取的数据为： \n");
        for(i=0; i<I2C1_DataSize; i++)
        {
            printf("%c",Read_I2C1_Addr[i]);
        }
				printf("\n\r");	
				I2C1_DataSize=0;				
		}
	}
}

/*																						
*********************************************************************************************************
*	函 数 名: COM_I2C2_Task
*	功能说明: COM_I2C2_Task任务主体
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void COM_I2C2_Task(void* parameter)
{
	uint8_t i;
	while(1)
	{
		if(xSemaphoreTake(xi2c2Semaphore,(TickType_t)0) == pdTRUE)
		{
			  printf("\n I2C2 读出成功，读取的数据为： \n");
        for(i=0; i<I2C2_DataSize; i++)
        {
            printf("%c",Read_I2C2_Addr[i]);
        }
				printf("\n\r");	
				I2C2_DataSize = 0;		
		}
	}
}