#include ".\timer\bsp_pwm.h"

static uint8_t current_duty = 50; // 默认占空比50%

/*																					
*********************************************************************************************************
*	函 数 名: pwm_gpio_config
*	功能说明: PWM GPIO初始化
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void pwm_gpio_config(void) 
{
	/*定义一个GPIO_InitTypeDef类型的结构体*/
	GPIO_InitTypeDef GPIO_InitStructure;

	/*开启相关的GPIO外设时钟*/
	RCC_APB2PeriphClockCmd(OCPWM_GPIO_CLK, ENABLE);

	/* 定时器通道引脚配置 */															   
	// PC8/PWM7  BMC_FAN_PWM1
	GPIO_InitStructure.GPIO_Pin = OCPWM_CHANNEL_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;    
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz; 
	GPIO_Init(OCPWM_GPIO_PORT, &GPIO_InitStructure);
	
}

/*
*********************************************************************************************************
*	函 数 名: pwm_output_Config
*	功能说明: PWM输出
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void pwm_output_Config(void)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;

	// 开启定时器时钟
	RCC_APB2PeriphClockCmd(OCPWM_TIM_CLK, ENABLE);

	// 配置TIM8 (PWM7)
	TIM_TimeBaseStructure.TIM_Period = OUTPUT_CNT - 1;
	TIM_TimeBaseStructure.TIM_Prescaler = APB2_TIM_Prescaler;
	TIM_TimeBaseStructure.TIM_ClockDivision = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode = TIM_CounterMode_Up;
	TIM_TimeBaseInit(OCPWM_TIM, &TIM_TimeBaseStructure);

	/*PWM模式配置*/
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_Pulse = (OUTPUT_CNT * current_duty) / 100 - 1;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;

	// 配置TIM8通道3 (PWM7)
	TIM_OC3Init(OCPWM_TIM, &TIM_OCInitStructure);
	TIM_OC3PreloadConfig(OCPWM_TIM, TIM_OCPreload_Enable);

	// 使能定时器
	TIM_Cmd(OCPWM_TIM, ENABLE);
	
	// 高级定时器需要使能主输出
	TIM_CtrlPWMOutputs(OCPWM_TIM, ENABLE);
}

/*																					
*********************************************************************************************************
*	函 数 名: pwm_set_duty
*	功能说明: 设置PWM占空比
*	形    参：duty - 占空比(0-100)
*	返 回 值: 无
*********************************************************************************************************
*/
void pwm_set_duty(uint8_t duty)
{
	if(duty > 100) duty = 100;
	if(duty < 0) duty = 0;
	
	current_duty = duty;
	
	// 设置PWM脉冲宽度
	TIM_SetCompare3(OCPWM_TIM, (OUTPUT_CNT * duty) / 100 - 1);
}

/*																					
*********************************************************************************************************
*	函 数 名: pwm_get_duty
*	功能说明: 获取当前PWM占空比
*	形    参：无
*	返 回 值: 当前占空比(0-100)
*********************************************************************************************************
*/
uint8_t pwm_get_duty(void)
{
	return current_duty;
}

/*																					
*********************************************************************************************************
*	函 数 名: PWM_Configuration
*	功能说明: 初始化PWM
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void PWM_Configuration(void)
{
	pwm_gpio_config();  
	pwm_output_Config();
}
