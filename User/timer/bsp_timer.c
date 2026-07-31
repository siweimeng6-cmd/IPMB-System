#include "bsp_init.h"


TIM_ICUserValueTypeDef TIM_ICUserValueStructure_CH1 = {0,0,0,0,0,0,0};
TIM_ICUserValueTypeDef TIM_ICUserValueStructure_CH2 = {0,0,0,0,0,0,0};
TIM_ICUserValueTypeDef TIM_ICUserValueStructure_CH3 = {0,0,0,0,0,0,0};
TIM_ICUserValueTypeDef TIM_ICUserValueStructure_CH4 = {0,0,0,0,0,0,0};


/*																						
*********************************************************************************************************
*	函 数 名: PWM_INPUT_TIM_GPIO_Config
*	功能说明: FAN_TACH
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void PWM_INPUT_TIM_GPIO_Config(void) 
{
  GPIO_InitTypeDef GPIO_InitStructure;

  // 输入捕获通道 GPIO 初始化
	RCC_APB2PeriphClockCmd(FAN_TACH_TIM_CH1_GPIO_CLK | FAN_TACH_TIM_CH2_GPIO_CLK | \
													FAN_TACH_TIM_CH3_GPIO_CLK | FAN_TACH_TIM_CH4_GPIO_CLK, ENABLE);
	
  GPIO_InitStructure.GPIO_Pin =  FAN_TACH_TIM_CH1_PIN | FAN_TACH_TIM_CH2_PIN;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
  GPIO_Init(GPIOA, &GPIO_InitStructure);	
	
	GPIO_InitStructure.GPIO_Pin =  FAN_TACH_TIM_CH3_PIN | FAN_TACH_TIM_CH4_PIN;
	GPIO_Init(GPIOB, &GPIO_InitStructure);	
	
}
/*																						
*********************************************************************************************************
*	函 数 名: PWM_OUTPUT_TIM_GPIO_Config
*	功能说明: PWM GP
出
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void PWM_OUTPUT_TIM_GPIO_Config(void) 
{
  GPIO_InitTypeDef GPIO_InitStructure;

  // 输出比较通道1 GPIO 初始化
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
	
  GPIO_InitStructure.GPIO_Pin =  PWM_OUTPUT_TIM_CH1_PIN | PWM_OUTPUT_TIM_CH2_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_Init(GPIOA, &GPIO_InitStructure);

}

/*																						
*********************************************************************************************************
*	函 数 名: pwm_TIM4_input_Config
*	功能说明: PWM输入捕获配置
						ARR ：自动重装载寄存器的值
						CLK_cnt：计数器的时钟，等于 Fck_int / (psc+1) = 72M/(psc+1)
						PWM 信号的周期 T = ARR * (1/CLK_cnt) = ARR*(PSC+1) / 72M
						占空比P=CCR/(ARR+1)
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void FAN_TACH_TIM_Mode_Config(uint16_t channel)
{
	uint16_t TIM_IT_CCx;
  // 开启定时器时钟,即内部时钟CK_INT=72M
	RCC_APB1PeriphClockCmd(FAN_TACH_TIM_CLK,ENABLE);
	
	if(channel == TIM_Channel_1)
	{
		TIM_IT_CCx =TIM_IT_CC1;	
	}
	else if(channel == TIM_Channel_2)
	{
		TIM_IT_CCx =TIM_IT_CC2;
	}
	else if(channel == TIM_Channel_3)
	{
		TIM_IT_CCx =TIM_IT_CC3;
	}
	else if(channel == TIM_Channel_4)
	{
		TIM_IT_CCx =TIM_IT_CC4;
	}

/*--------------------时基结构体初始化-------------------------*/	
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_TimeBaseStructure.TIM_Period						= Input_CNT-1 ;		// 自动重装载寄存器的值，累计TIM_Period+1个频率后产生一个更新或者中断
	TIM_TimeBaseStructure.TIM_Prescaler					= Input_TIM_Prescaler -1 ;		// 驱动CNT计数器的时钟 = Fck_int/(psc+1)
	TIM_TimeBaseStructure.TIM_ClockDivision			= TIM_CKD_DIV1;			// 时钟分频因子 ，配置死区时间时需要用到
	TIM_TimeBaseStructure.TIM_CounterMode				= TIM_CounterMode_Up;			// 计数器计数模式，设置为向上计数
	TIM_TimeBaseStructure.TIM_RepetitionCounter	= 0;		// 重复计数器的值，没用到不用管
	TIM_TimeBaseInit(FAN_TACH_TIM, &TIM_TimeBaseStructure);	// 初始化定时器

	/*--------------------输入捕获结构体初始化-------------------*/	
	TIM_ICInitTypeDef TIM_ICInitStructure;
	TIM_ICInitStructure.TIM_Channel 		= channel;// 配置输入捕获的通道，需要根据具体的GPIO来配置
	TIM_ICInitStructure.TIM_ICPolarity 	= TIM_ICPolarity_Rising;	// 输入捕获信号的极性配置
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;	// 输入通道和捕获通道的映射关系，有直连和非直连两种
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;	// 输入的需要被捕获的信号的分频系数
	TIM_ICInitStructure.TIM_ICFilter 		= 0;	// 输入的需要被捕获的信号的滤波系数
	TIM_ICInit(FAN_TACH_TIM, &TIM_ICInitStructure);	// 定时器输入捕获初始化
	
  TIM_ClearFlag(FAN_TACH_TIM, TIM_FLAG_Update | TIM_IT_CCx);		// 清除更新和捕获中断标志位
	TIM_ITConfig (FAN_TACH_TIM, TIM_IT_Update 	| TIM_IT_CCx, ENABLE );  // 开启更新和捕获中断  
	
	TIM_Cmd(FAN_TACH_TIM, ENABLE);	// 使能计数器
}
/*																						
*********************************************************************************************************
*	函 数 名:  PWM_OUTPUT_TIM_Mode_Config
*	功能说明: PWM输出配置
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void PWM_OUTPUT_TIM_Mode_Config(void)
{
  // 开启定时器时钟,即内部时钟CK_INT=72M
	PWM_OUTPUT_TIM_APBxClock_FUN(PWM_OUTPUT_TIM_CLK,ENABLE);
	

/*--------------------时基结构体初始化-------------------------*/	
  TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_TimeBaseStructure.TIM_Period  					=	OUTPUT_CNT -1;		// 自动重装载寄存器的值，累计TIM_Period+1个频率后产生一个更新或者中断
	TIM_TimeBaseStructure.TIM_Prescaler					= Output_TIM_Prescaler -1;		// 驱动CNT计数器的时钟 = Fck_int/(psc+1)
	TIM_TimeBaseStructure.TIM_ClockDivision			= TIM_CKD_DIV1;			// 时钟分频因子 ，配置死区时间时需要用到
	TIM_TimeBaseStructure.TIM_CounterMode				= TIM_CounterMode_Up;			// 计数器计数模式，设置为向上计数
	TIM_TimeBaseStructure.TIM_RepetitionCounter	= 0;		// 重复计数器的值，没用到不用管
	TIM_TimeBaseInit(PWM_OUTPUT_TIM, &TIM_TimeBaseStructure);	// 初始化定时器

	/*--------------------输出比较结构体初始化-------------------*/	
	TIM_OCInitTypeDef  TIM_OCInitStructure;
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;	// 配置为PWM模式1
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;	// 输出使能
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;	// 输出通道电平极性配置	
	
	// 输出比较通道 1
	TIM_OCInitStructure.TIM_Pulse = UP_CNT;
	TIM_OC1Init(PWM_OUTPUT_TIM, &TIM_OCInitStructure);
	TIM_OC1PreloadConfig(PWM_OUTPUT_TIM, TIM_OCPreload_Enable);
	
	// 输出比较通道 2
	TIM_OCInitStructure.TIM_Pulse = 45;
	TIM_OC2Init(PWM_OUTPUT_TIM, &TIM_OCInitStructure);
	TIM_OC2PreloadConfig(PWM_OUTPUT_TIM, TIM_OCPreload_Enable);

	TIM_Cmd(PWM_OUTPUT_TIM, ENABLE);	// 使能计数器
	TIM_CtrlPWMOutputs(PWM_OUTPUT_TIM, ENABLE);	// 主输出使能，当使用的是通用定时器时，这句不需要
}

/*																						
*********************************************************************************************************
*	函 数 名: pwm_init
*	功能说明: 
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void pwm_init(void)
{
		PWM_INPUT_TIM_GPIO_Config(); 
		FAN_TACH_TIM_Mode_Config(TIM_Channel_2);
		FAN_TACH_TIM_Mode_Config(TIM_Channel_1);

		PWM_OUTPUT_TIM_GPIO_Config();
		PWM_OUTPUT_TIM_Mode_Config();
}

/*																						
*********************************************************************************************************
*	函 数 名: channel1_get
*	功能说明: 
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void channel1_get(void)
{
		switch(TIM_ICUserValueStructure_CH1.Get_State) 
		{
			case 0:
				TIM_ICUserValueStructure_CH1.Get_State = 1;	
				TIM_SetCounter ( FAN_TACH_TIM, 0 );																															// 计数器清0
				TIM_ICUserValueStructure_CH1.Timer_Cnt = 0;					
				TIM_ICUserValueStructure_CH1.Capture_CcrValue_UP = 0;																									// 存捕获比较寄存器的值的变量的值清0	
				TIM_ICUserValueStructure_CH1.Capture_CcrValue_DOWN = 0;
				FAN_TACH_TIM_OCPolarityConfig_FUN_CH1(FAN_TACH_TIM  , TIM_ICPolarity_Falling);															// 当第一次捕获到上升沿之后，就把捕获边沿配置为下降沿
			break;
			case 1:					// 下降沿捕获中断
				TIM_ICUserValueStructure_CH1.Get_State = 2;
				TIM_ICUserValueStructure_CH1.Capture_CcrValue_UP = FAN_TACH_TIM_GetCapture_FUN_CH1(FAN_TACH_TIM);							// 获取捕获比较寄存器的值，这个值就是捕获到的高电平的时间的值
				TIM_ICUserValueStructure_CH1.Capture_Period_UP = TIM_ICUserValueStructure_CH1.Timer_Cnt;	
				FAN_TACH_TIM_OCPolarityConfig_FUN_CH1(FAN_TACH_TIM , TIM_ICPolarity_Rising);																// 当第二次捕获到下降沿之后，就把捕获边沿配置为上升沿，好开启新的一轮捕获		
				break;
			case 2:	
				TIM_ICUserValueStructure_CH1.Get_State = 3;
				TIM_ICUserValueStructure_CH1.Capture_CcrValue_DOWN = FAN_TACH_TIM_GetCapture_FUN_CH1(FAN_TACH_TIM);
				TIM_ICUserValueStructure_CH1.Capture_Period_DOWN = TIM_ICUserValueStructure_CH1.Timer_Cnt;
				break;
			case 3:	
				TIM_ICUserValueStructure_CH1.Capture_FinishFlag = 1;	      																					// 捕获完成标志置1	
				break;
		}	
}

/*																						
*********************************************************************************************************
*	函 数 名: channel2_get
*	功能说明: 
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void channel2_get(void)
{
		switch(TIM_ICUserValueStructure_CH2.Get_State) 
		{
			case 0:
				TIM_ICUserValueStructure_CH2.Get_State = 1;	
				TIM_SetCounter ( FAN_TACH_TIM, 0 );																															// 计数器清0
				TIM_ICUserValueStructure_CH2.Timer_Cnt = 0;					
				TIM_ICUserValueStructure_CH2.Capture_CcrValue_UP = 0;																									// 存捕获比较寄存器的值的变量的值清0	
				TIM_ICUserValueStructure_CH2.Capture_CcrValue_DOWN = 0;
				FAN_TACH_TIM_OCPolarityConfig_FUN_CH2(FAN_TACH_TIM  , TIM_ICPolarity_Falling);															// 当第一次捕获到上升沿之后，就把捕获边沿配置为下降沿
			break;
			case 1:					// 下降沿捕获中断
				TIM_ICUserValueStructure_CH2.Get_State = 2;
				TIM_ICUserValueStructure_CH2.Capture_CcrValue_UP = FAN_TACH_TIM_GetCapture_FUN_CH2(FAN_TACH_TIM);							// 获取捕获比较寄存器的值，这个值就是捕获到的高电平的时间的值
				TIM_ICUserValueStructure_CH2.Capture_Period_UP = TIM_ICUserValueStructure_CH2.Timer_Cnt;	
				FAN_TACH_TIM_OCPolarityConfig_FUN_CH2(FAN_TACH_TIM , TIM_ICPolarity_Rising);																// 当第二次捕获到下降沿之后，就把捕获边沿配置为上升沿，好开启新的一轮捕获		
				break;
			case 2:	
				TIM_ICUserValueStructure_CH2.Get_State = 3;
				TIM_ICUserValueStructure_CH2.Capture_CcrValue_DOWN = FAN_TACH_TIM_GetCapture_FUN_CH2(FAN_TACH_TIM);
				TIM_ICUserValueStructure_CH2.Capture_Period_DOWN = TIM_ICUserValueStructure_CH2.Timer_Cnt;
				break;
			case 3:	
				TIM_ICUserValueStructure_CH2.Capture_FinishFlag = 1;	      																					// 捕获完成标志置1	
				break;
		}	
}

/*																						
*********************************************************************************************************
*	函 数 名: channel3_get
*	功能说明: 
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void channel3_get(void)
{
		switch(TIM_ICUserValueStructure_CH3.Get_State) 
		{
			case 0:
				TIM_ICUserValueStructure_CH3.Get_State = 1;	
				TIM_SetCounter ( FAN_TACH_TIM, 0 );																															// 计数器清0
				TIM_ICUserValueStructure_CH3.Timer_Cnt = 0;					
				TIM_ICUserValueStructure_CH3.Capture_CcrValue_UP = 0;																									// 存捕获比较寄存器的值的变量的值清0	
				TIM_ICUserValueStructure_CH3.Capture_CcrValue_DOWN = 0;
				FAN_TACH_TIM_OCPolarityConfig_FUN_CH3(FAN_TACH_TIM  , TIM_ICPolarity_Falling);															// 当第一次捕获到上升沿之后，就把捕获边沿配置为下降沿
			break;
			case 1:					// 下降沿捕获中断
				TIM_ICUserValueStructure_CH3.Get_State = 2;
				TIM_ICUserValueStructure_CH3.Capture_CcrValue_UP = FAN_TACH_TIM_GetCapture_FUN_CH3(FAN_TACH_TIM);							// 获取捕获比较寄存器的值，这个值就是捕获到的高电平的时间的值
				TIM_ICUserValueStructure_CH3.Capture_Period_UP = TIM_ICUserValueStructure_CH3.Timer_Cnt;	
				FAN_TACH_TIM_OCPolarityConfig_FUN_CH3(FAN_TACH_TIM , TIM_ICPolarity_Rising);																// 当第二次捕获到下降沿之后，就把捕获边沿配置为上升沿，好开启新的一轮捕获		
				break;
			case 2:	
				TIM_ICUserValueStructure_CH3.Get_State = 3;
				TIM_ICUserValueStructure_CH3.Capture_CcrValue_DOWN = FAN_TACH_TIM_GetCapture_FUN_CH3(FAN_TACH_TIM);
				TIM_ICUserValueStructure_CH3.Capture_Period_DOWN = TIM_ICUserValueStructure_CH3.Timer_Cnt;
				break;
			case 3:	
				TIM_ICUserValueStructure_CH3.Capture_FinishFlag = 1;	      																					// 捕获完成标志置1	
				break;
		}	
}

/*																						
*********************************************************************************************************
*	函 数 名: channel4_get
*	功能说明: 
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void channel4_get(void)
{
			switch(TIM_ICUserValueStructure_CH4.Get_State) 
		{
			case 0:
				TIM_ICUserValueStructure_CH4.Get_State = 1;	
				TIM_SetCounter ( FAN_TACH_TIM, 0 );																															// 计数器清0
				TIM_ICUserValueStructure_CH4.Timer_Cnt = 0;					
				TIM_ICUserValueStructure_CH4.Capture_CcrValue_UP = 0;																									// 存捕获比较寄存器的值的变量的值清0	
				TIM_ICUserValueStructure_CH4.Capture_CcrValue_DOWN = 0;
				FAN_TACH_TIM_OCPolarityConfig_FUN_CH4(FAN_TACH_TIM  , TIM_ICPolarity_Falling);															// 当第一次捕获到上升沿之后，就把捕获边沿配置为下降沿
			break;
			case 1:					// 下降沿捕获中断
				TIM_ICUserValueStructure_CH4.Get_State = 2;
				TIM_ICUserValueStructure_CH4.Capture_CcrValue_UP = FAN_TACH_TIM_GetCapture_FUN_CH4(FAN_TACH_TIM);							// 获取捕获比较寄存器的值，这个值就是捕获到的高电平的时间的值
				TIM_ICUserValueStructure_CH4.Capture_Period_UP = TIM_ICUserValueStructure_CH4.Timer_Cnt;	
				FAN_TACH_TIM_OCPolarityConfig_FUN_CH4(FAN_TACH_TIM , TIM_ICPolarity_Rising);																// 当第二次捕获到下降沿之后，就把捕获边沿配置为上升沿，好开启新的一轮捕获		
				break;
			case 2:	
				TIM_ICUserValueStructure_CH4.Get_State = 3;
				TIM_ICUserValueStructure_CH4.Capture_CcrValue_DOWN = FAN_TACH_TIM_GetCapture_FUN_CH4(FAN_TACH_TIM);
				TIM_ICUserValueStructure_CH4.Capture_Period_DOWN = TIM_ICUserValueStructure_CH4.Timer_Cnt;
				break;
			case 3:	
				TIM_ICUserValueStructure_CH4.Capture_FinishFlag = 1;	      																					// 捕获完成标志置1	
				break;
		}	
}

/*																						
*********************************************************************************************************
*	函 数 名: TIM4_IRQHandler
*	功能说明: TIM4_IRQHandler中断
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void TIM3_IRQHandler (void)	//PB6:TIM4_CH1
{
		if ( TIM_GetITStatus ( FAN_TACH_TIM, TIM_IT_Update) != RESET )               
		{		
			if(TIM_ICUserValueStructure_CH1.Capture_FinishFlag ==0 )
			{
				TIM_ICUserValueStructure_CH1.Timer_Cnt++;
			}			
			if(TIM_ICUserValueStructure_CH2.Capture_FinishFlag ==0 )
			{
				TIM_ICUserValueStructure_CH2.Timer_Cnt++;
			}
				TIM_ClearITPendingBit ( FAN_TACH_TIM, TIM_FLAG_Update ); 		
		}
		if( TIM_GetITStatus (FAN_TACH_TIM, TIM_IT_CC1 ) != RESET)
		{
			channel1_get();
			TIM_ClearITPendingBit(FAN_TACH_TIM, TIM_IT_CC1 );              																// 清除定时器溢出中断标志位		
		}
		else if( TIM_GetITStatus (FAN_TACH_TIM, TIM_IT_CC2 ) != RESET)
		{
			channel2_get();
			TIM_ClearITPendingBit(FAN_TACH_TIM, TIM_IT_CC2 );              																// 清除定时器溢出中断标志位		
		}
		else if( TIM_GetITStatus (FAN_TACH_TIM, TIM_IT_CC3 ) != RESET)
		{
			channel3_get();
			TIM_ClearITPendingBit(FAN_TACH_TIM, TIM_IT_CC3 );              																// 清除定时器溢出中断标志位		
		}
		else if( TIM_GetITStatus (FAN_TACH_TIM, TIM_IT_CC4 ) != RESET)
		{
			channel4_get();
			TIM_ClearITPendingBit(FAN_TACH_TIM, TIM_IT_CC4 );              																// 清除定时器溢出中断标志位		
		}
}








