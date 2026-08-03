#ifndef __TASK_SLOT_POLL_H
#define __TASK_SLOT_POLL_H

#include "FreeRTOS.h"
#include "task.h"

/*
*********************************************************************************************************
*	模 块 名: task_slot_poll.c
*	功能说明: 网页控制台改造(2026-07-27)新增——多槽位后台轮询,替换原来只轮询单一从机的
*	          Web_Sensor_Poll_Task(task_web_sensor.c,已停用不再编译进调用链,详见该文件头部说明)。
*	          每3秒轮转发现表
*	          g_slave_addrs[] 里的下一个槽位,对它发 Get Device ID(仅新槽位首次)+
*	          Get Board Status + 7个 Get Sensor Reading,结果存进 task_slot_cache.c 的
*	          g_slot_cache[],供 bsp_httpd.c 直接读取渲染 JSON。
*	注    意: 复用原 Web_Sensor_Poll_Task 的专属 mailbox 机制(xIPMB_WebRspQueue/2,
*	          IPMB_WEB_HOST_ADDR_7BIT),不新建队列,任务创建时栈大小/优先级也保持一致
*	          (bsp_init.c 里原地替换,堆占用净变化为零)。
*********************************************************************************************************
*/

extern TaskHandle_t IPMB_Slot_Poll_Task_Handle;

void IPMB_Slot_Poll_Task(void *parameter);

#endif
