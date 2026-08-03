#ifndef __TASK_WEB_SENSOR_H
#define __TASK_WEB_SENSOR_H

#include <stdint.h>
#include "FreeRTOS.h"
#include "task.h"

/* 网页监控的传感器快照:每3秒由 Web_Sensor_Poll_Task 通过真实 IPMB
 * Get Sensor Reading(cmd 0x2D) 向从设备(FRU板)请求一次并刷新。
 * 数值是 IPMI 协议原始字节(无小数、无缩放,截断整数),单位见各字段注释。
 * valid_mask 每个 bit 对应一个传感器最近一轮是否读取成功,超时清 0。
 * 只有 Web_Sensor_Poll_Task 一个写者,bsp_httpd.c 只读,不需要加锁
 * (跟项目里 board_temp0/1/2、stADC_Data 现有的单写者/无锁做法一致)。 */
typedef struct {
	uint8_t temp90;      /* 90Temp(LM75A@0x90),整数摄氏度 */
	uint8_t temp92;      /* 92Temp(LM75A@0x92),整数摄氏度 */
	uint8_t v12;         /* V12(ADC PA6),整数伏特 */
	uint8_t v075;        /* V0.75(ADC PA7+PB0平均),整数伏特——已知限制:<1V截断恒为0 */
	uint8_t iout;        /* Iout(BPD20550 PMBus),整数安培 */
	uint8_t valid_mask;  /* bit0=temp90 bit1=temp92 bit2=v12 bit3=v075 bit4=iout */
} WebSensorSnapshot_t;

extern volatile WebSensorSnapshot_t g_web_sensor_snapshot;
extern TaskHandle_t Web_Sensor_Poll_Task_Handle;

void Web_Sensor_Poll_Task(void *parameter);

#endif
