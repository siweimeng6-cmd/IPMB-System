#ifndef __TASK_PWM_H
#define	__TASK_PWM_H

#include "bsp_pwm.h"
#include "bsp_init.h"

extern TaskHandle_t PWM_Task_Handle ;

extern uint8_t input1_configure_flag;
extern uint8_t input2_configure_flag;
extern uint8_t input3_configure_flag;
extern uint8_t input4_configure_flag;

extern xTaskHandle timechan1getSemaphore ; 
extern xTaskHandle timechan2getSemaphore ; 


void PWM_Task(void* parameter);
void pwm_check_swtich(void);

#endif