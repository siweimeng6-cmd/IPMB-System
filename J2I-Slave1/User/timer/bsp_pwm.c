#include "bsp_init.h"

uint8_t channel3_flag =0;
uint8_t channel4_flag =0;
uint8_t channel_3_configure_flag=0;
uint8_t channel_4_configure_flag=0;
uint16_t Up_Capture_Cnt,Down_Capture_Cnt,Up_Capture,Up_Capture_Cnt_Temp,Down_Capture;
uint16_t timer_cnt2,timer_cnt1 = 0;
uint16_t Get_State = 0,Get_State1 = 0;

void PWM_TIM_GPIO_Config(void) 
{
  GPIO_InitTypeDef GPIO_InitStructure;

  // 输出比较通道1 GPIO 初始化
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
  GPIO_InitStructure.GPIO_Pin =  PWM_TIM_CH1_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(PWM_TIM_CH1_PORT, &GPIO_InitStructure);

  GPIO_InitStructure.GPIO_Pin =  PWM_TIM_CH3_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(PWM_TIM_CH3_PORT, &GPIO_InitStructure);
	
	
		// 输出比较通道3 GPIO 初始化
  GPIO_InitStructure.GPIO_Pin =  PWM_TECH_TIM_CH3_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(PWM_TECH_TIM_CH3_PORT, &GPIO_InitStructure);
	
	// 输出比较通道4 GPIO 初始化
  GPIO_InitStructure.GPIO_Pin =  PWM_TECH_TIM_CH4_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(PWM_TECH_TIM_CH4_PORT, &GPIO_InitStructure);
	
}

void PWM_TIM_Mode_Config(void)
{
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	
	PWM_TIM_APBxClock_FUN(PWM_TIM_CLK,ENABLE);  // 开启定时器时钟,即内部时钟CK_INT=72M

/*--------------------时基结构体初始化-------------------------*/
	// 配置周期，这里配置为100K
	
	TIM_TimeBaseStructure.TIM_Period=PWM_TIM_Period;	// 自动重装载寄存器的值，累计TIM_Period+1个频率后产生一个更新或者中断	
	TIM_TimeBaseStructure.TIM_Prescaler= PWM_TIM_Prescaler;		// 驱动CNT计数器的时钟 = Fck_int/(psc+1)
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1;		// 时钟分频因子 ，配置死区时间时需要用到	
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up;			// 计数器计数模式，设置为向上计数
	TIM_TimeBaseStructure.TIM_RepetitionCounter=0;		// 重复计数器的值，没用到不用管
	TIM_TimeBaseInit(PWM_TIM, &TIM_TimeBaseStructure);	// 初始化定时器

	
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;	// 配置为PWM模式1
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;	// 输出使能
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;	// 输出通道电平极性配置	
	
	// 输出比较通道 1
	TIM_OCInitStructure.TIM_Pulse = CCR1_Val;
	TIM_OC1Init(PWM_TIM, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);
	
	// 输出比较通道 2
	TIM_OCInitStructure.TIM_Pulse = CCR3_Val;
	TIM_OC2Init(PWM_TIM, &TIM_OCInitStructure);
	TIM_OC2PreloadConfig(PWM_TIM, TIM_OCPreload_Enable);
	
	// 使能计数器
	TIM_Cmd(PWM_TIM, ENABLE);
}

void PWM_TIM_Init(void)
{
	PWM_TIM_GPIO_Config();
	PWM_TIM_Mode_Config();		
}

void pwm_input_Config(uint16_t channel)
{
	uint16_t TIM_IT_CCx;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_ICInitTypeDef  TIM_ICInitStructure;
	if(channel == TIM_Channel_3)
	{
		TIM_IT_CCx =TIM_IT_CC3;
	}
	else if(channel == TIM_Channel_4)
	{
		TIM_IT_CCx =TIM_IT_CC4;
	}
	
	PWM_TECH_TIM_APBxClock_FUN(PWM_TECH_TIM_CLK,ENABLE);  // 开启定时器时钟,即内部时钟CK_INT=72M

/*--------------------时基结构体初始化-------------------------*/
	TIM_TimeBaseStructure.TIM_Period=PWM_TECH_TIM_PERIOD;	// 自动重装载寄存器的值，累计TIM_Period+1个频率后产生一个更新或者中断
	TIM_TimeBaseStructure.TIM_Prescaler= PWM_TECH_TIM_PSC;		// 驱动CNT计数器的时钟 = Fck_int/(psc+1)
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1;		// 时钟分频因子 ，配置死区时间时需要用到	
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up;		// 计数器计数模式，设置为向上计数	
	TIM_TimeBaseStructure.TIM_RepetitionCounter=0;		// 重复计数器的值，没用到不用管
	TIM_TimeBaseInit(PWM_TECH_TIM, &TIM_TimeBaseStructure);	// 初始化定时器

		/*-------------------输入捕获结构体初始化------------------*/	
  TIM_ICInitStructure.TIM_Channel = channel;
  TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;     			//上升沿触发
  TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI; 			//IC1直接连接TI1FP1
  TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;           			//对输入的PWM信号不分频
  TIM_ICInitStructure.TIM_ICFilter = 0x0;
  TIM_ICInit(TIM1, &TIM_ICInitStructure);																//此处一定要使用TIM_ICInit，而不是TIM_PWMIConfig
  // 选择从模式
  TIM_SelectSlaveMode(TIM1, TIM_SlaveMode_Reset);												// PWM输入模式时，从模式必须工作在复位模式，当捕获开始时，计数器CNT被复位清零；
  TIM_SelectMasterSlaveMode(TIM1,TIM_MasterSlaveMode_Enable); 

  TIM_ITConfig(TIM1,TIM_IT_CCx| TIM_IT_Update, ENABLE);									// 使能捕获中断，这个中断主要针对的是主捕获通道（TI1FP1）
  TIM_ClearITPendingBit(TIM1, TIM_IT_CCx);															// 清除中断标志位
	
  TIM_Cmd(TIM1, ENABLE);																								// 计数器开始计数
}

/*																						
*********************************************************************************************************
*	函 数 名: pwm_input_callback
*	功能说明: 定时器回调函数
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void pwm_input_callback(void)
{
	if(TIM_GetITStatus(TIM1, TIM_IT_CC3) != RESET)
  {
		get_input_register();
		TIM_ClearITPendingBit(TIM1,TIM_IT_Update);              // 清除定时器溢出中断标志位		
	}
	else if(TIM_GetITStatus(TIM1, TIM_IT_CC4) != RESET)
  {
		get_input_register();
		TIM_ClearITPendingBit(TIM1,TIM_IT_Update);              // 清除定时器溢出中断标志位		
	}
}

/*																						
*********************************************************************************************************
*	函 数 名: get_input_register
*	功能说明: pwm输入，获取捕获定时器的寄存器值，并进行上下延捕获切换
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void get_input_register(void)
{
	static uint8_t Get_State;

	if(channel_3_configure_flag ==1)						//当前定时器1通道3进行输入捕获
	{
		switch(Get_State)    
			{
			case 0 :            
				Up_Capture_Cnt_Temp = Up_Capture_Cnt;       // 保存上一次输入捕获通道的值
				Down_Capture_Cnt =  TIM_GetCapture3(TIM1);  // 获取当前输入捕获通道的值
				Down_Capture = Down_Capture_Cnt + (timer_cnt2 * INPUT_CNT) - Up_Capture_Cnt_Temp;    // 计算脉冲宽度
				timer_cnt1 = 0;                             // 定时器计数标志量1清零
				timer_cnt2 = 0;                             // 定时器计数标志量2清零
				TIM_ClearITPendingBit(TIM1,TIM_IT_CC3);     // 清除输入捕获通道的中断标志位
				TIM_OC3PolarityConfig(TIM1,TIM_ICPolarity_Falling); // 设置输入捕获通道的极性为下降沿
				Get_State = 1;                              // 设置输入捕获通道的状态为1
				break;                                      // 跳出switch语句
			case 1:         
				Up_Capture_Cnt =  TIM_GetCapture3(TIM1);    // 获取当前输入捕获通道的值
				Up_Capture = Up_Capture_Cnt + (timer_cnt1 * INPUT_CNT) - Down_Capture_Cnt;           // 计算脉冲宽度
				timer_cnt1 = 0;                             // 定时器计数标志量1清零
				timer_cnt2 = 0;                             // 定时器计数标志量2清零
				TIM_ClearITPendingBit(TIM1,TIM_IT_CC3);     // 清除输入捕获通道的中断标志位
				TIM_OC3PolarityConfig(TIM1,TIM_ICPolarity_Rising);  // 设置输入捕获通道的极性为上升沿
				Get_State = 0;                              // 设置输入捕获通道的状态为0
				channel3_flag =1;
				break;		
			}
		}
	else if(channel_4_configure_flag ==1)							//当前定时器1通道4进行输入捕获
	{
		switch(Get_State)    
			{
			case 0 :            
				Up_Capture_Cnt_Temp = Up_Capture_Cnt;       // 保存上一次输入捕获通道的值
				Down_Capture_Cnt =  TIM_GetCapture4(TIM1);  // 获取当前输入捕获通道的值
				Down_Capture = Down_Capture_Cnt + (timer_cnt2 * INPUT_CNT) - Up_Capture_Cnt_Temp;    // 计算脉冲宽度
				timer_cnt1 = 0;                             // 定时器计数标志量1清零
				timer_cnt2 = 0;                             // 定时器计数标志量2清零
				TIM_ClearITPendingBit(TIM1,TIM_IT_CC4);     // 清除输入捕获通道的中断标志位
				TIM_OC4PolarityConfig(TIM1,TIM_ICPolarity_Falling); // 设置输入捕获通道的极性为下降沿
				Get_State = 1;                              // 设置输入捕获通道的状态为1
				break;                                      // 跳出switch语句
			case 1:         
				Up_Capture_Cnt =  TIM_GetCapture4(TIM1);    // 获取当前输入捕获通道的值
				Up_Capture = Up_Capture_Cnt + (timer_cnt1 * INPUT_CNT) - Down_Capture_Cnt;           // 计算脉冲宽度
				timer_cnt1 = 0;                             // 定时器计数标志量1清零
				timer_cnt2 = 0;                             // 定时器计数标志量2清零
				TIM_ClearITPendingBit(TIM1,TIM_IT_CC4);     // 清除输入捕获通道的中断标志位
				TIM_OC4PolarityConfig(TIM1,TIM_ICPolarity_Rising);  // 设置输入捕获通道的极性为上升沿
				Get_State = 0;                              // 设置输入捕获通道的状态为0
				channel4_flag =1;
				break;		
			}
		}
}

/*																						
*********************************************************************************************************
*	函 数 名: TIM1_CC_IRQHandler
*	功能说明: TIM1_CHANNEL1中断
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void TIM1_CC_IRQHandler(void)
{
	pwm_input_callback();

}

/*																						
*********************************************************************************************************
*	函 数 名: TIM1_UP_TIM10_IRQHandler
*	功能说明: TIM1更新中断入口，主要响应TIM_IT_Update
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void TIM1_UP_IRQHandler(void)
{

	if(TIM_GetITStatus(TIM1, TIM_IT_Update) != RESET)
	{
				timer_cnt1++;                               // 定时器计数标志量1每溢出一次加一
				timer_cnt2++;                               // 定时器计数标志量2每溢出一次加一
				if(timer_cnt1 == 10000)                     // 定时器计数标志量1溢出时清零
				{
					timer_cnt1 = 0;                         // 定时器计数标志量1清零
				}
				if(timer_cnt2 == 10000)                     // 定时器计数标志量2溢出时清零
				{
					timer_cnt2 = 0;                         // 定时器计数标志量2清零
				}
	}	
	TIM_ClearITPendingBit(TIM1, TIM_IT_Update);
}

