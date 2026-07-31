#ifndef __BSP_PWM_H
#define	__BSP_PWM_H

#include "bsp_init.h"

#define            PWM_TIM                   			TIM8
#define            PWM_TIM_APBxClock_FUN     			RCC_APB2PeriphClockCmd
#define            PWM_TIM_CLK               			RCC_APB2Periph_TIM8
#define            PWM_TIM_Period            			9
#define            PWM_TIM_Prescaler         			71

// TIM8 ����Ƚ�ͨ��1
#define            PWM_TIM_CH1_GPIO_CLK      			RCC_APB2Periph_GPIOC
#define            PWM_TIM_CH1_PORT          			GPIOC
#define            PWM_TIM_CH1_PIN           			GPIO_Pin_6

// TIM8 ����Ƚ�ͨ��3
#define            PWM_TIM_CH3_GPIO_CLK      			RCC_APB2Periph_GPIOC
#define            PWM_TIM_CH3_PORT          			GPIOC
#define            PWM_TIM_CH3_PIN          			GPIO_Pin_8

/***************************************���벶��****************************************/
#define            PWM_TECH_TIM                   TIM1
#define            PWM_TECH_TIM_APBxClock_FUN     RCC_APB2PeriphClockCmd
#define            PWM_TECH_TIM_CLK               RCC_APB2Periph_TIM1
// ���PWM��Ƶ��Ϊ 72M/{ (ARR+1)*(PSC+1) }
#define            PWM_TECH_TIM_PERIOD            (1000-1)
#define            PWM_TECH_TIM_PSC               (72-1)
// TIM1 CH3
#define            PWM_TECH_TIM_CH3_GPIO_CLK      RCC_APB2Periph_GPIOA
#define            PWM_TECH_TIM_CH3_PORT          GPIOA
#define            PWM_TECH_TIM_CH3_PIN           GPIO_Pin_10

// TIM1 CH4
#define            PWM_TECH_TIM_CH4_GPIO_CLK      RCC_APB2Periph_GPIOA
#define            PWM_TECH_TIM_CH4_PORT          GPIOA
#define            PWM_TECH_TIM_CH4_PIN           GPIO_Pin_11

#define            CCR1_Val							5
#define            CCR3_Val							5

#define INPUT_CNT                              	1000
#define UP_CNT																	100

void PWM_TIM_GPIO_Config(void);
void PWM_TIM_Mode_Config(void);
void get_input_register(void);
void pwm_input_callback(void);

extern float fan1_duty, fan2_duty;   //风扇占空比(百分比 0~100)，func_pwm.c 定期刷新
#define pwm_get_duty() ((uint8_t)fan1_duty)

#endif
