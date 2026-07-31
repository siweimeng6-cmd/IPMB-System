#ifndef __TASK_USART_H
#define	__TASK_USART_H

#include "stm32f4xx.h"
#include <stdio.h>

#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "bsp_pwm.h"
#include "bsp_usart.h"

extern TaskHandle_t USART1_Task_Handle;
extern TaskHandle_t USART2_Task_Handle;
extern TaskHandle_t USART3_Task_Handle;
extern TaskHandle_t USART4_Task_Handle;
extern TaskHandle_t USART5_Task_Handle;
extern TaskHandle_t USART6_Task_Handle;

extern xTaskHandle xUsart1Semaphore; 
extern xTaskHandle xUsart2Semaphore; 
extern xTaskHandle xUsart3Semaphore; 
extern xTaskHandle xUsart4Semaphore; 
extern xTaskHandle xUsart5Semaphore; 
extern xTaskHandle xUsart6Semaphore; 

void USART1_Task(void* pvParameters);
void USART2_Task(void* pvParameters);
void USART3_Task(void* pvParameters);
void USART4_Task(void* pvParameters);
void USART5_Task(void* pvParameters);
void USART6_Task(void* pvParameters);

extern TaskHandle_t IPMB_PEM_Poll_Task_Handle;
void IPMB_PEM_Poll_Task(void* pvParameters);

#endif
