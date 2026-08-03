#ifndef __BSP_PWM_H
#define	__BSP_PWM_H

#include "stm32f10x.h"
#include "bsp_init.h"

/*******************************PWMÊä³ö**************************************/              
// PC8/PWM7  BMC_FAN_PWM1
#define OCPWM_GPIO_PORT        GPIOC                       
#define OCPWM_GPIO_CLK         RCC_APB2Periph_GPIOC
#define OCPWM_CHANNEL_PIN      GPIO_Pin_8 
#define OCPWM_TIM              TIM8
#define OCPWM_TIM_CLK          RCC_APB2Periph_TIM8
#define OCPWM_CHANNEL          TIM_Channel_3

/****************************************************************************/ 

#define APB1_TIM_Prescaler    71
#define APB2_TIM_Prescaler    71
#define APB1_CLK              36000000
#define APB2_CLK              72000000
#define OUTPUT_CNT            200
#define INPUT_CNT             1000
#define UP_CNT                100

void PWM_Configuration(void);
void pwm_set_duty(uint8_t duty);
uint8_t pwm_get_duty(void);

#endif /* __BSP_PWM_H */
