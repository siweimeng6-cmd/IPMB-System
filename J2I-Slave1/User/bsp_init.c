#include "bsp_init.h"
#include "FreeRTOS.h"
#include ".\i2c\bsp_eeprom.h"
#include ".\ipmb\ipmb_board_identity.h"


TaskHandle_t Sensor_Task_Handle = NULL;
TaskHandle_t UART5_Task_Handle = NULL;
TaskHandle_t Power_Ctrl_Task_Handle = NULL;


xTaskHandle xUart4Semaphore    = NULL;
xTaskHandle xUart5Semaphore    = NULL;


static void NVIC_Configuration(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;
  
  NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
  
  NVIC_InitStructure.NVIC_IRQChannel = COM_USART5_IRQ;	
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
	
	NVIC_InitStructure.NVIC_IRQChannel = DEBUG_USART_IRQ;	
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);
	
  NVIC_InitStructure.NVIC_IRQChannel = FAN_TACH_TIM_IRQ;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 6;
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
  NVIC_Init(&NVIC_InitStructure);

	/* I2C1/I2C2 (IPMB-A/B) 的 NVIC 配置由 I2C_Slave_Init() 内部的
	 * I2C_Slave_NVIC_Config() 统一管理(见 .\ipmb\bsp_i2c_slave.c),
	 * 这里不再重复配置, 避免同一中断线被配置两次。 */
}
/***********************************************************************
  * @ 函数名  ： SemaphoreTakeCreate
  * @ 功能说明： 创建任务二值量
  * @ 参数    ：   
  * @ 返回值  ： 无
  *********************************************************************/
void SemaphoreTakeCreate(void)
{
		vSemaphoreCreateBinary(xUart4Semaphore);
		xSemaphoreTake(xUart4Semaphore,(portTickType)0);
		vSemaphoreCreateBinary(xUart5Semaphore);
		xSemaphoreTake(xUart5Semaphore,(portTickType)0);
		/* xI2C1_Semaphore/xI2C2_Semaphore 改由 I2C_Slave_Init() 内部创建
		 * (见 .\ipmb\bsp_i2c_slave.c), 这里不再创建 */
}
/***********************************************************************
  * @ 函数名  ： BSP_Init
  * @ 功能说明： 板级外设初始化，所有板子上的初始化均可放在这个函数里面
  * @ 参数    ：   
  * @ 返回值  ： 无
  *********************************************************************/
void BSP_Init(void)
{

	NVIC_Configuration();

	USART_Config(DEBUG_USARTx,DEBUG_USART_BAUDRATE);
	USART_Config(COM_USART5  ,DEBUG_USART_BAUDRATE);
	printf("/**********************************************/ \r\n");
	printf("init finished\r\n");
	printf("last build date：2025.6.11\r\n");
	printf("only use to test HKV-6UD2K-J2I-BMC\r\n");
	//printf(">>> 本固件为 OTA 升级测试版本 <<<\r\n");
	printf("/**********************************************/ \r\n");
	
	#if MO_IIC
	Init_MO_I2C();
	EEPROM_SelfTest();
	Runtime_Init();
	BoardIdentity_Init();
	#else
	Init_HARD_I2C();
	#endif

	ADCx_Init();

	gpio_init();          //GA0..GA4+GAP 等输入引脚在这里配置好，必须在 slave_addr_detect() 之前
	slave_addr_detect();   //读卡槽地址到 GA_VALUE，必须在 init_ipmb_i2c() 之前

	IPMB_Slave_FRUInit();
	IPMB_Slave_SensorInit();

	#if TEST_IIC1_MONI	//使用PB6,PB7做模拟IIC，测试隔离芯片，初始化调用Init_MO_I2C()

	#else								//使用PB6,PB7做硬件IIC
	init_ipmb_i2c();
	#endif

	Fan_PWM_Init();   //TIM8 CH3/CH4(PC8/PC9) 双路风扇PWM,上电默认100%占空比(故障安全)

	//pwm_init();

	SemaphoreTakeCreate();

}


/***********************************************************************
  * @ 函数名  ： AppTaskCreate
  * @ 功能说明： 为了方便管理，所有的任务创建函数都放在这个函数里面
  * @ 参数    ： 无  
  * @ 返回值  ： 无
  **********************************************************************/
void AppTaskCreate(void)
{
  BaseType_t xReturn = pdPASS;/* 定义一个创建信息返回值，默认为pdPASS */
  
  taskENTER_CRITICAL();           //进入临界区

	xReturn = xTaskCreate((TaskFunction_t )Power_Ctrl_Task, /* 任务入口函数 */
                        (const char*    )"Power_Ctrl_Task",/* 任务名字 */
                        (uint16_t       )512,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )2,	    /* 任务的优先级 */
                        (TaskHandle_t*  )&Power_Ctrl_Task_Handle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    printf("创建Power_Ctrl_Task任务成功!\r\n");

	xReturn = xTaskCreate((TaskFunction_t )Fan_Ctrl_Task, /* 任务入口函数 */
                        (const char*    )"Fan_Ctrl_Task",/* 任务名字 */
                        (uint16_t       )512,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )2,	    /* 任务的优先级 */
                        (TaskHandle_t*  )&Fan_Ctrl_Task_Handle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    printf("创建Fan_Ctrl_Task任务成功!\r\n");

	xReturn = xTaskCreate((TaskFunction_t )Sensor_Task, /* 任务入口函数 */
                        (const char*    )"Sensor_Task",/* 任务名字 */
                        (uint16_t       )512,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )2,	    /* 任务的优先级 */
                        (TaskHandle_t*  )&Sensor_Task_Handle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    printf("创建Sensor_Task任务成功!\r\n");
	
	xReturn = xTaskCreate((TaskFunction_t )UART5_Task, /* 任务入口函数 */
                        (const char*    )"UART5_Task",/* 任务名字 */
                        (uint16_t       )512,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )2,	    /* 任务的优先级 */
                        (TaskHandle_t*  )&UART5_Task_Handle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    printf("创建UART5_Task任务成功!\r\n");
  
		xReturn = xTaskCreate((TaskFunction_t )IPMB_A_Task, /* 任务入口函数 */
                        (const char*    )"IPMB_A_Task",/* 任务名字 */
                        (uint16_t       )768,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )2,	    /* 任务的优先级 */
                        (TaskHandle_t*  )&IPMB_A_TaskHandle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    printf("创建IPMB_A_Task任务成功!\r\n");

	xReturn = xTaskCreate((TaskFunction_t )IPMB_B_Task, /* 任务入口函数 */
                        (const char*    )"IPMB_B_Task",/* 任务名字 */
                        (uint16_t       )768,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )2,	    /* 任务的优先级 */
                        (TaskHandle_t*  )&IPMB_B_TaskHandle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    printf("创建IPMB_B_Task任务成功!\r\n");

	xReturn = xTaskCreate((TaskFunction_t )IPMB_Arbitration_Task, /* 任务入口函数 */
                        (const char*    )"IPMB_Arb_Task",/* 任务名字 */
                        (uint16_t       )256,   /* 任务栈大小 */
                        (void*          )NULL,	/* 任务入口函数参数 */
                        (UBaseType_t    )2,	    /* 任务的优先级 */
                        (TaskHandle_t*  )&IPMB_Arbitration_TaskHandle);/* 任务控制块指针 */
  if(pdPASS == xReturn)
    printf("创建IPMB_Arb_Task任务成功!\r\n");

  vTaskDelete(AppTaskCreate_Handle); //删除AppTaskCreate任务
  taskEXIT_CRITICAL();            //退出临界区
}

/***********************************************************************
  * @ 函数名  ： Delay_ms
  * @ 功能说明： 操作系统延时delay
  * @ 参数    ： 无  
  * @ 返回值  ： 无
  **********************************************************************/
void Delay_ms(uint32_t count)
{
       u32 ticks;
       u32 told,tnow,reload,tcnt=0;
       if((0x0001&(SysTick->CTRL)) ==0)    //定时器未工作
              vPortSetupTimerInterrupt();  //初始化定时器
 
       reload = SysTick->LOAD;                     //获取重装载寄存器值
       ticks = count * (SystemCoreClock / 1000);  //计数时间值
       
       vTaskSuspendAll();//阻止OS调度，防止打断us延时
       told=SysTick->VAL;  //获取当前数值寄存器值（开始时数值）
       while(1)
       {
              tnow=SysTick->VAL; //获取当前数值寄存器值
              if(tnow!=told)  //当前值不等于开始值说明已在计数
              {         
                     if(tnow<told)  //当前值小于开始数值，说明未计到0
                          tcnt+=told-tnow; //计数值=开始值-当前值
 
                     else     //当前值大于开始数值，说明已计到0并重新计数
                            tcnt+=reload-tnow+told;   //计数值=重装载值-当前值+开始值  （
                                                      //已从开始值计到0） 
 
                     told=tnow;   //更新开始值
                     if(tcnt>=ticks)break;  //时间超过/等于要延迟的时间,则退出.
              } 
       }  
       xTaskResumeAll();	//恢复OS调度

}

/***********************************************************************
  * @ 函数名  ： Delay_ms
  * @ 功能说明： 操作系统延时delay
  * @ 参数    ： 无  
  * @ 返回值  ： 无
  **********************************************************************/
void Delay_us(uint32_t count)
{
       u32 ticks;
       u32 told,tnow,reload,tcnt=0;
       if((0x0001&(SysTick->CTRL)) ==0)    //定时器未工作
              vPortSetupTimerInterrupt();  //初始化定时器
 
       reload = SysTick->LOAD;                     //获取重装载寄存器值
       ticks = count * (SystemCoreClock / 1000000);  //计数时间值
       
       vTaskSuspendAll();//阻止OS调度，防止打断us延时
       told=SysTick->VAL;  //获取当前数值寄存器值（开始时数值）
       while(1)
       {
              tnow=SysTick->VAL; //获取当前数值寄存器值
              if(tnow!=told)  //当前值不等于开始值说明已在计数
              {         
                     if(tnow<told)  //当前值小于开始数值，说明未计到0
                          tcnt+=told-tnow; //计数值=开始值-当前值
 
                     else     //当前值大于开始数值，说明已计到0并重新计数
                            tcnt+=reload-tnow+told;   //计数值=重装载值-当前值+开始值  （
                                                      //已从开始值计到0） 
 
                     told=tnow;   //更新开始值
                     if(tcnt>=ticks)break;  //时间超过/等于要延迟的时间,则退出.
              } 
       }  
       xTaskResumeAll();	//恢复OS调度

}