#ifndef __BSP_FAN_PWM_H
#define __BSP_FAN_PWM_H

#include "bsp_init.h"

/* PC8/PC9 = TIM8_CH3/TIM8_CH4 默认复用映射,无需AFIO重映射
 * 原理图: MCU_FAN_PWM1 -> PC8/PWM7, MCU_FAN_PWM2 -> PC9/PWM8
 * TIM8挂在APB2,72MHz(与本工程TIM1/TIM3同源)
 * Fpwm = 72MHz / (3 * 1000) = 24kHz(4线风扇标准PWM频段21~28kHz内)
 * 占空比0~100(%) <-> CCR = duty*10(0~1000),整数映射无舍入误差 */
#define            FAN_PWM_TIM                   			TIM8
#define            FAN_PWM_TIM_APBxClock_FUN     			RCC_APB2PeriphClockCmd
#define            FAN_PWM_TIM_CLK               			RCC_APB2Periph_TIM8

#define            FAN_PWM_GPIO_CLK              			RCC_APB2Periph_GPIOC
#define            FAN_PWM_GPIO_PORT             			GPIOC
#define            FAN_PWM_CH3_PIN               			GPIO_Pin_8   /* MCU_FAN_PWM1 */
#define            FAN_PWM_CH4_PIN               			GPIO_Pin_9   /* MCU_FAN_PWM2 */

#define            FAN_PWM_TIM_PRESCALER         			3       /* 分频系数;写寄存器时用 PRESCALER-1 */
#define            FAN_PWM_TIM_PERIOD            			1000    /* ARR+1;写寄存器时用 PERIOD-1 */

#define            FAN_CH1                       			0
#define            FAN_CH2                       			1
#define            FAN_CH_COUNT                  			2

void Fan_PWM_Init(void);
void Fan_PWM_SetDuty(uint8_t channel, uint8_t duty_percent);

void Fan_Ctrl_Task(void* parameter);
void Fan_SetManualDuty(uint8_t channel, uint8_t duty_percent);
void Fan_ClearManualOverride(void);
uint8_t Fan_GetCurrentDuty(uint8_t channel);

void Fan_UpdateCpuReportedTemp(int temp_c);
uint8_t Fan_GetCpuTempDebug(int *out_temp_c);  /* 仅供调试打印, 返回1=新鲜/0=过期或从未收到 */

extern TaskHandle_t Fan_Ctrl_Task_Handle;

#endif
