/* FreeRTOS头文件 */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
/* 开发板硬件bsp头文件 */
#include "bsp_init.h"

static TaskHandle_t AppTaskCreate_Handle = NULL;
static void AppTaskCreate_entry(void); // 用于创建任务

/* 网页控制台改造(2026-07-27)阶段0诊断任务:启动10秒后(任务都已跑起来、栈用量趋于稳定)
 * 打印一次剩余堆大小 + 关键IPMB任务的栈高水位,给后续新增任务/缓存的内存预算提供真实基线,
 * 而不是依赖 FreeRTOSConfig.h 里"约47KB"这种过时注释。打印一次后自行删除,不常驻。 */
static void Mem_Diag_Task(void *parameter)
{
	(void)parameter;
	vTaskDelay(10000);

	printf(">>[MEM] +10s free heap = %u bytes\r\n", (unsigned)xPortGetFreeHeapSize());
	printf(">>[MEM] stack high-water(remaining, in words): I2C1_Task=%u I2C2_Task=%u IPMB_Slot_Poll=%u IPMB_PEM_Poll=%u\r\n",
		(unsigned)uxTaskGetStackHighWaterMark(I2C1_Task_Handle),
		(unsigned)uxTaskGetStackHighWaterMark(I2C2_Task_Handle),
		(unsigned)uxTaskGetStackHighWaterMark(IPMB_Slot_Poll_Task_Handle),
		(unsigned)uxTaskGetStackHighWaterMark(IPMB_PEM_Poll_Task_Handle));

	vTaskDelete(NULL);
}

/*
*********************************************************************************************************
*	函 数 名: main
*	功能说明: 主函数
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
int main(void)
{
  BaseType_t xReturn = pdPASS; /* 定义一个创建信息返回值，默认为pdPASS */

  /* 开发板硬件初始化 */
  BSP_Init();
  printf("初始化成功！init sucess!\r\n");
  delay_1ms(500);

  // eeprom_test();

  /* 创建AppTaskCreate任务 */
  xReturn = xTaskCreate((TaskFunction_t)AppTaskCreate_entry,    /* 任务入口函数 */
                        (const char *)"AppTaskCreate",          /* 任务名字 */
                        (uint16_t)1024,                         /* 任务栈大小 */
                        (void *)NULL,                           /* 任务入口函数参数 */
                        (UBaseType_t)1,                         /* 任务的优先级 */
                        (TaskHandle_t *)&AppTaskCreate_Handle); /* 任务控制块指针 */

  /* 启动任务调度 */
  if (pdPASS == xReturn)
    vTaskStartScheduler(); /* 启动任务，开启调度 */
  else
    return -1;

  while (1)
    ; /* 正常不会执行到这里 */
}

/*
*********************************************************************************************************
*	函 数 名: AppTaskCreate
*	功能说明: 为了方便管理，所有的任务创建函数都放在这个函数里面
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void AppTaskCreate_entry(void)
{
  taskENTER_CRITICAL(); // 进入临界区

  printf(">>[MEM] before task_create() free heap = %u bytes\r\n", (unsigned)xPortGetFreeHeapSize());

  task_create();

  printf(">>[MEM] after  task_create() free heap = %u bytes\r\n", (unsigned)xPortGetFreeHeapSize());

  xTaskCreate((TaskFunction_t)Mem_Diag_Task, (const char *)"Mem_Diag", (uint16_t)192,
              (void *)NULL, (UBaseType_t)1, (TaskHandle_t *)NULL);

  vTaskDelete(AppTaskCreate_Handle); // 删除AppTaskCreate任务
  taskEXIT_CRITICAL();               // 退出临界区
}
