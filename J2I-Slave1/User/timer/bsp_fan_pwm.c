#include "bsp_init.h"

/*
*********************************************************************************************************
*	函 数 名: Fan_PWM_GPIO_Config
*	功能说明: 配置PC8/PC9为TIM8_CH3/CH4复用推挽输出
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void Fan_PWM_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_APB2PeriphClockCmd(FAN_PWM_GPIO_CLK, ENABLE);

	GPIO_InitStructure.GPIO_Pin   = FAN_PWM_CH3_PIN | FAN_PWM_CH4_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(FAN_PWM_GPIO_PORT, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	函 数 名: Fan_PWM_TIM_Mode_Config
*	功能说明: 配置TIM8 CH3/CH4为PWM输出模式,24kHz,上电默认100%占空比(故障安全)
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void Fan_PWM_TIM_Mode_Config(void)
{
	TIM_TimeBaseInitTypeDef TIM_TimeBaseStructure;
	TIM_OCInitTypeDef       TIM_OCInitStructure;

	FAN_PWM_TIM_APBxClock_FUN(FAN_PWM_TIM_CLK, ENABLE);

	TIM_TimeBaseStructure.TIM_Period            = FAN_PWM_TIM_PERIOD - 1;
	TIM_TimeBaseStructure.TIM_Prescaler         = FAN_PWM_TIM_PRESCALER - 1;
	TIM_TimeBaseStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
	TIM_TimeBaseStructure.TIM_CounterMode       = TIM_CounterMode_Up;
	TIM_TimeBaseStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(FAN_PWM_TIM, &TIM_TimeBaseStructure);

	TIM_OCInitStructure.TIM_OCMode      = TIM_OCMode_PWM1;
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
	TIM_OCInitStructure.TIM_OCPolarity  = TIM_OCPolarity_High;
	TIM_OCInitStructure.TIM_Pulse       = FAN_PWM_TIM_PERIOD; /* CCR>ARR恒为高,上电默认100%占空比(故障安全) */

	TIM_OC3Init(FAN_PWM_TIM, &TIM_OCInitStructure);
	TIM_OC3PreloadConfig(FAN_PWM_TIM, TIM_OCPreload_Enable);

	TIM_OC4Init(FAN_PWM_TIM, &TIM_OCInitStructure);
	TIM_OC4PreloadConfig(FAN_PWM_TIM, TIM_OCPreload_Enable);

	TIM_Cmd(FAN_PWM_TIM, ENABLE);
	TIM_CtrlPWMOutputs(FAN_PWM_TIM, ENABLE); /* TIM8是高级定时器,必须置MOE位输出才会真正出现在PC8/PC9上 */
}

/*
*********************************************************************************************************
*	函 数 名: Fan_PWM_Init
*	功能说明: 风扇PWM初始化入口
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void Fan_PWM_Init(void)
{
	Fan_PWM_GPIO_Config();
	Fan_PWM_TIM_Mode_Config();
}

/*
*********************************************************************************************************
*	函 数 名: Fan_PWM_SetDuty
*	功能说明: 设置指定风扇通道的PWM占空比
*	形    参：channel: FAN_CH1/FAN_CH2   duty_percent: 0~100
*	返 回 值: 无
*********************************************************************************************************
*/
void Fan_PWM_SetDuty(uint8_t channel, uint8_t duty_percent)
{
	uint16_t ccr;

	if(duty_percent > 100) duty_percent = 100;
	ccr = (uint16_t)duty_percent * (FAN_PWM_TIM_PERIOD / 100);

	if(channel == FAN_CH1)
		TIM_SetCompare3(FAN_PWM_TIM, ccr);
	else if(channel == FAN_CH2)
		TIM_SetCompare4(FAN_PWM_TIM, ccr);
}
