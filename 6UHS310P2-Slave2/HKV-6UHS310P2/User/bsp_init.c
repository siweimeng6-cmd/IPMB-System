#include "bsp_init.h"
#include "task_usart.h"
#include "bsp_adc.h"
#include "bsp_mo_i2c.h"
#include "bsp_i2c.h"
#include ".\usart\bsp_usart.h"
#include ".\timer\bsp_pwm.h"
#include ".\ipmb\ipmb_slave.h"
#include ".\ipmb\task_ipmb_master.h"      /* IPMB_Master_Task (条件编译) */

extern void vPortSetupTimerInterrupt( void );

stPRINTF_BUF_t stPrintf_Buf;

static void NVIC_Configuration(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;
  
  NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
    
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
    USART_Config();
    bsp_gpio_init();
    BASIC_TIM_Mode_Config();
    ADCx_Init();
    PWM_Configuration();
    Init_MO_I2C();
    /* IPMB 协议栈初始化 */
    IPMB_Slave_FRUInit();
    IPMB_Slave_SensorInit();
    init_ipmb_i2c();   /* I2C1/I2C2 硬件使能 (按 IPMB_A_AS_MASTER 决定角色) */
}

/***********************************************************************
  * @ 函数名  ： Delay_ms
  * @ 功能说明： 操作系统延时delay
  * @ 参数    ： 无  
  * @ 返回值  ： 无
  **********************************************************************/
void delay_1ms(uint32_t count)
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
  * @ 函数名  ： AppTaskCreate
  * @ 功能说明： 为了方便管理，所有的任务创建函数都放在这个函数里面
  * @ 参数    ： 无  
  * @ 返回值  ： 无
  **********************************************************************/
void AppTaskCreate(void)
{
  BaseType_t xReturn = pdPASS;/* 定义一个创建信息返回值，默认为pdPASS */
  
  taskENTER_CRITICAL();           //进入临界区   
  
/**************************************************************************************************/  
  xReturn = xTaskCreate((TaskFunction_t )Sensor_Task,
                        (const char*    )"Sensor_Task",
                        (uint16_t       )1024,
                        (void*          )NULL,
                        (UBaseType_t    )2,
                        (TaskHandle_t*  )&Sensor_Task_Handle);
  if(pdPASS == xReturn)
  { 
  }   
  else
    printf("创建Sensor_Task任务失败!\r\n");
 /* ********************************************************************************************* */
  xReturn = xTaskCreate((TaskFunction_t )GPIO_Task,
                        (const char*    )"GPIO_Task",
                        (uint16_t       )512,  // 增加栈大小到512
                        (void*          )NULL,
                        (UBaseType_t    )1,
                        (TaskHandle_t*  )&GPIO_Task_Handle);
  if(pdPASS == xReturn)
  {
  }
  else
    printf("创建GPIO_Task 任务失败!\r\n");

  /* IPMB-A (I2C1) 任务: 主机模式时改跑 IPMB_Master_Task, 从机模式保持 IPMB_A_Task */
#if IPMB_A_AS_MASTER
  xReturn = xTaskCreate((TaskFunction_t)IPMB_Master_Task,
                        (const char*   )"IPMB_Master",
                        (uint16_t      )1024,
                        (void*         )NULL,
                        (UBaseType_t   )3,
                        (TaskHandle_t* )&IPMB_Master_TaskHandle);
  if(pdPASS != xReturn)
    printf("创建IPMB_Master_Task 任务失败!\r\n");
#else
  xReturn = xTaskCreate((TaskFunction_t)IPMB_A_Task,
                        (const char*   )"IPMB_A_Task",
                        (uint16_t      )768,
                        (void*         )NULL,
                        (UBaseType_t   )3,
                        (TaskHandle_t* )&IPMB_A_TaskHandle);
  if(pdPASS != xReturn)
    printf("创建IPMB_A_Task 任务失败!\r\n");
#endif

  /* IPMB-B (I2C2) 任务: 永远保留 (无论主从, 都做业务板从机) */
  xReturn = xTaskCreate((TaskFunction_t)IPMB_B_Task,
                        (const char*   )"IPMB_B_Task",
                        (uint16_t      )768,
                        (void*         )NULL,
                        (UBaseType_t   )3,
                        (TaskHandle_t* )&IPMB_B_TaskHandle);
  if(pdPASS != xReturn)
    printf("创建IPMB_B_Task 任务失败!\r\n");

#if !IPMB_A_AS_MASTER
  /* IPMB-A/B 总线仲裁任务: IPMB_A_AS_MASTER 自测模式下 I2C1 不是从机, 无仲裁意义 */
  xReturn = xTaskCreate((TaskFunction_t)IPMB_Arbitration_Task,
                        (const char*   )"IPMB_Arb_Task",
                        (uint16_t      )256,
                        (void*         )NULL,
                        (UBaseType_t   )3,
                        (TaskHandle_t* )&IPMB_Arbitration_TaskHandle);
  if(pdPASS != xReturn)
    printf("创建IPMB_Arbitration_Task 任务失败!\r\n");
#endif

  vTaskDelete(AppTaskCreate_Handle); //删除AppTaskCreate任务
  taskEXIT_CRITICAL();//退出临界区
}

