 #include "bsp_pwm.h"

TIM_ICUserValueTypeDef TIM_ICUserValueStructure_TACH1 = {0,0,0,0,0,0,0};
TIM_ICUserValueTypeDef TIM_ICUserValueStructure_TACH2 = {0,0,0,0,0,0,0};
TIM_ICUserValueTypeDef TIM_ICUserValueStructure_TACH3 = {0,0,0,0,0,0,0};
TIM_ICUserValueTypeDef TIM_ICUserValueStructure_TACH4 = {0,0,0,0,0,0,0};

/* TIM5 互斥:TACH3(PH11/CH2) 与 TACH4(PH10/CH1) 共用 TIM5,而测量起点(case 0)
 * 要把共享的 CNT 清零 —— 两路并发测量会互相清掉对方的计数基准,导致先启动的
 * 一路周期算错(2026-07-09 定位的第3路 RPM 异常根因)。改为同一时刻只允许一路
 * 在测,另一路等它数值锁存(case 2)后再启动。0=空闲,3=TACH3占用,4=TACH4占用。
 * 该变量只在 TIM5_IRQHandler 单一中断上下文里读写,无并发竞争。 */
static volatile uint8_t s_tim5_owner = 0;

/*																						
*********************************************************************************************************
*	�� �� ��: pwm_gpio_config
*	����˵��: PWM GPIO��ʼ��
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void pwm_gpio_config(void) 
{
	/*����һ��GPIO_InitTypeDef���͵Ľṹ��*/
	GPIO_InitTypeDef GPIO_InitStructure;

	/*������ص�GPIO����ʱ��*/
	RCC_AHB2PeriphClockCmd (RCC_APB2Periph_TIM8, ENABLE); 
	RCC_AHB2PeriphClockCmd (RCC_APB2Periph_TIM9, ENABLE); 
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOE | RCC_AHB1Periph_GPIOI,ENABLE);
	/* ��ʱ��ͨ�����Ÿ��� */
	GPIO_PinAFConfig(OCPWM1_GPIO_PORT,OCPWM1_CHANNEL_PINSOURCE,OCPWM1_AF); 
	GPIO_PinAFConfig(OCPWM2_GPIO_PORT,OCPWM2_CHANNEL_PINSOURCE,OCPWM2_AF); 
	GPIO_PinAFConfig(OCPWM3_GPIO_PORT,OCPWM3_CHANNEL_PINSOURCE,OCPWM3_AF); 
	GPIO_PinAFConfig(OCPWM4_GPIO_PORT,OCPWM4_CHANNEL_PINSOURCE,OCPWM4_AF); 


  
	/* ��ʱ��ͨ���������� */															   
	GPIO_InitStructure.GPIO_Pin = OCPWM1_CHANNEL_PIN;	
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;    
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; 
	GPIO_Init(OCPWM1_GPIO_PORT, &GPIO_InitStructure);
	
	GPIO_InitStructure.GPIO_Pin = OCPWM2_CHANNEL_PIN;
	GPIO_Init(OCPWM2_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = OCPWM3_CHANNEL_PIN;
	GPIO_Init(OCPWM3_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = OCPWM4_CHANNEL_PIN;
	GPIO_Init(OCPWM4_GPIO_PORT, &GPIO_InitStructure);
	
}

/*																						
*********************************************************************************************************
*	�� �� ��: pwm_output_Config
*	����˵��: PWM���
						SYSCLK��ϵͳʱ�ӣ� = 168MHz
						AHB ����ʱ��(HCLK=SYSCLK) =	168MHz
						APB1 ����ʱ��(PCLK1=SYSCLK/4) =	42MHz
						APB2 ����ʱ��(PCLK2=SYSCLK/2) =	84MHz
						PLL ��ʱ�� = 168MHz
						PE6 TIM9_CH2 			PE5 TIM9_CH1
						PC8	TIM8_CH3			PI6 TIM8_CH2
*	��    �Σ�uint32_t TIM_Period,uint32_t TIM_Pulse
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void pwm_output_Config(void)
{
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	TIM_OCInitTypeDef  TIM_OCInitStructure;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM9, ENABLE);// ����TIMx_CLK,x[2,3,4,5,12,13,14] 
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_TIM8, ENABLE);// ����TIMx_CLK,x[2,3,4,5,12,13,14] 

	TIM_TimeBaseStructure.TIM_Period = OUTPUT_CNT-1;//�ۼ� TIM_Period�������һ�����»����ж�,����ʱ����0������8399����Ϊ8400�Σ�Ϊһ����ʱ����

	// ͨ�ÿ��ƶ�ʱ��ʱ��ԴTIMxCLK = HCLK/2=84MHz 
	// �趨��ʱ��Ƶ��Ϊ=TIMxCLK/(TIM_Prescaler+1)=100KHz
	TIM_TimeBaseStructure.TIM_Prescaler = APB2_TIM_Prescaler-1;	
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1;// ����ʱ�ӷ�Ƶ
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up;// ������ʽ
	TIM_TimeBaseInit(TIM8, &TIM_TimeBaseStructure);// ��ʼ����ʱ��TIMx, x[2,3,4,5,12,13,14] 


	/*PWMģʽ����*/
	/* PWM1 Mode configuration: Channel1 */
	TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;//����ΪPWMģʽ1
	TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;	
	TIM_OCInitStructure.TIM_Pulse = UP_CNT-1;
	TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_High;//����ʱ������ֵС��CCR1_ValʱΪ�ߵ�ƽ
	TIM_OC3Init(TIM8, &TIM_OCInitStructure);//PC8	TIM8_CH3
	TIM_OC2Init(TIM8, &TIM_OCInitStructure);//PI6 TIM8_CH2


	/*ʹ��ͨ��1����*/
	TIM_OC3PreloadConfig(TIM8, TIM_OCPreload_Enable);
	TIM_OC2PreloadConfig(TIM8, TIM_OCPreload_Enable);

	// ʹ�ܶ�ʱ��
	TIM_Cmd(TIM8, ENABLE);	
	TIM_CtrlPWMOutputs(TIM8, ENABLE);//STM32F407�ĸ߼���ʱ��TIM1��TIM8��Ҫ����ɲ���������Ĵ�����BDTR����ʹ���������TIM_CtrlPWMOutputs��

	TIM_OC2Init(TIM9, &TIM_OCInitStructure);//PE6 TIM9_CH2
	TIM_OC1Init(TIM9, &TIM_OCInitStructure);//PE5 TIM9_CH1
	TIM_OC2PreloadConfig(TIM9, TIM_OCPreload_Enable);
	TIM_OC1PreloadConfig(TIM9, TIM_OCPreload_Enable);
	TIM_TimeBaseInit(TIM9, &TIM_TimeBaseStructure);// ��ʼ����ʱ��TIMx, x[2,3,4,5,12,13,14] 
	TIM_Cmd(TIM9, ENABLE);	
}

/*																						
*********************************************************************************************************
*	�� �� ��: pwm_gpio_disable
*	����˵��: ���������ڶ�ͨ��ʱ�����⼸��ͨ��ͬʱ���룬�Բɼ��źŲ���������������ʵ������Ŀǰ������Ҫ
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void pwm_gpio_disable(uint16_t output_channel) 
{
	GPIO_InitTypeDef GPIO_InitStructure;

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;    
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz; 
	if(output_channel == OCPWM1_CHANNEL)
	{
		GPIO_InitStructure.GPIO_Pin = OCPWM1_CHANNEL_PIN;
		GPIO_Init(OCPWM1_GPIO_PORT, &GPIO_InitStructure);		
	}
	else if(output_channel == OCPWM2_CHANNEL)
	{
		GPIO_InitStructure.GPIO_Pin = OCPWM2_CHANNEL_PIN;	
		GPIO_Init(OCPWM2_GPIO_PORT, &GPIO_InitStructure);
	}
	else if(output_channel == OCPWM3_CHANNEL)
	{
		GPIO_InitStructure.GPIO_Pin = OCPWM3_CHANNEL_PIN;	
		GPIO_Init(OCPWM3_GPIO_PORT, &GPIO_InitStructure);
	}
	else if(output_channel == OCPWM4_CHANNEL)
	{
		GPIO_InitStructure.GPIO_Pin = OCPWM4_CHANNEL_PIN;
		GPIO_Init(OCPWM4_GPIO_PORT, &GPIO_InitStructure);
	}
}
/*																						
*********************************************************************************************************
*	�� �� ��: pwm_input_Config
*	����˵��: PWM���벶������
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void pwm_intput_gpio_Config(void)
{
	/*����һ��GPIO_InitTypeDef���͵Ľṹ��*/
	GPIO_InitTypeDef GPIO_InitStructure;

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOB | RCC_AHB1Periph_GPIOI | RCC_AHB1Periph_GPIOH, ENABLE);	


	GPIO_InitStructure.GPIO_Pin 	= INPUT_PWM1_CHANNEL_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP ;   // TACH open-drain, need pull-up
	GPIO_Init(INPUT_PWM1_GPIO_PORT, &GPIO_InitStructure);	

	GPIO_InitStructure.GPIO_Pin =  INPUT_PWM2_CHANNEL_PIN;
	GPIO_Init(INPUT_PWM2_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin =  INPUT_PWM3_CHANNEL_PIN;
	GPIO_Init(INPUT_PWM3_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin =  INPUT_PWM4_CHANNEL_PIN;
	GPIO_Init(INPUT_PWM4_GPIO_PORT, &GPIO_InitStructure);

	GPIO_PinAFConfig(INPUT_PWM1_GPIO_PORT, INPUT_PWM1_CHANNEL_PINSOURCE, INPUT_PWM1_AF);
	GPIO_PinAFConfig(INPUT_PWM2_GPIO_PORT, INPUT_PWM2_CHANNEL_PINSOURCE, INPUT_PWM2_AF);
	GPIO_PinAFConfig(INPUT_PWM3_GPIO_PORT, INPUT_PWM3_CHANNEL_PINSOURCE, INPUT_PWM3_AF);
	GPIO_PinAFConfig(INPUT_PWM4_GPIO_PORT, INPUT_PWM4_CHANNEL_PINSOURCE, INPUT_PWM4_AF);

}
/*																						
*********************************************************************************************************
*	�� �� ��: pwm_input_Config
*	����˵��: PWM���벶������
						SYSCLK��ϵͳʱ�ӣ� = 168MHz
						AHB ����ʱ��(HCLK=SYSCLK) =	168MHz
						APB1 ����ʱ��(PCLK1=SYSCLK/4) =	42MHz
						APB2 ����ʱ��(PCLK2=SYSCLK/2) =	84MHz
						PLL ��ʱ�� =	168MHz
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void pwm_input_Config(uint16_t channel)
{
	uint16_t TIM_IT_CCx;
	TIM_TypeDef *  TIMx;
	TIM_TimeBaseInitTypeDef  TIM_TimeBaseStructure;
	GPIO_InitTypeDef GPIO_InitStructure;
	TIM_ICInitTypeDef  TIM_ICInitStructure;

	
	if(channel == INPUT1_CHANNEL)//PB9 TIM4 CH4 TACH1
	{
		TIMx = TIM4;
		RCC_APB1PeriphClockCmd(INPUT_PWM1_TIM_CLK, ENABLE);
		TIM_IT_CCx =TIM_IT_CC4;
		TIM_ICInitStructure.TIM_Channel = TIM_Channel_4;
		TIM_TimeBaseStructure.TIM_Prescaler= APB1_TIM_Prescaler-1;//PSC = 1us = 1MHz
	}
	else if(channel == INPUT2_CHANNEL)//PI2 TIM8 CH4 TACH2
	{
		TIMx = TIM8;
		RCC_APB2PeriphClockCmd(INPUT_PWM2_TIM_CLK, ENABLE);
		TIM_IT_CCx =TIM_IT_CC4;
		TIM_ICInitStructure.TIM_Channel = TIM_Channel_4;
		TIM_TimeBaseStructure.TIM_Prescaler= APB2_TIM_Prescaler-1;//PSC = 1us = 1MHz
	}
	else if(channel == INPUT3_CHANNEL)//PH11 TIM5 CH2 TACH3
	{
		TIMx = TIM5;
		RCC_APB1PeriphClockCmd(INPUT_PWM3_TIM_CLK, ENABLE);
		TIM_IT_CCx =TIM_IT_CC2;
		TIM_ICInitStructure.TIM_Channel = TIM_Channel_2;
		TIM_TimeBaseStructure.TIM_Prescaler= APB1_TIM_Prescaler-1;//PSC = 1us = 1MHz
	}
	else if(channel == INPUT4_CHANNEL)//PH10 TIM5 CH1 TACH4
	{
		TIMx = TIM5;
		RCC_APB1PeriphClockCmd(INPUT_PWM4_TIM_CLK, ENABLE);
		TIM_IT_CCx =TIM_IT_CC1;
		TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
		TIM_TimeBaseStructure.TIM_Prescaler= APB1_TIM_Prescaler-1;//PSC = 1us = 1MHz
	}
 
	
	/*--------------------ʱ���ṹ��ʼ��-------------------------*/
	TIM_TimeBaseStructure.TIM_Period=INPUT_CNT-1;//ARR=65535us
	TIM_TimeBaseStructure.TIM_ClockDivision=TIM_CKD_DIV1;//��Ƶ����Ϊ0��������Ƶ��	
	TIM_TimeBaseStructure.TIM_CounterMode=TIM_CounterMode_Up;//CNTΪ���ϼ���ģʽ��		
	TIM_TimeBaseStructure.TIM_RepetitionCounter=0;	
	TIM_TimeBaseInit(TIMx, &TIM_TimeBaseStructure);

	/*-------------------���벶��ṹ���ʼ��------------------*/
	TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;//�����ش���
	TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;//IC1ֱ������TI1FP1
	TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;//�������PWM�źŲ���Ƶ
	TIM_ICInitStructure.TIM_ICFilter = 0x0;
	TIM_ICInit(TIMx, &TIM_ICInitStructure);//�˴�һ��Ҫʹ��TIM_ICInit��������TIM_PWMIConfig

	//TIM_SelectInputTrigger(TIM1, TIM_TS_TI1FP1);//ѡ�����벶��Ĵ����źţ�	
	//TIM_SelectInputTrigger(TIM1, TIM_TS_ETRF);//ѡ�����벶��Ĵ����źţ�	

	// ѡ���ģʽ
//	TIM_SelectSlaveMode(TIMx, TIM_SlaveMode_Reset);// PWM����ģʽʱ����ģʽ���빤���ڸ�λģʽ��������ʼʱ��������CNT����λ���㣻
//	TIM_SelectMasterSlaveMode(TIMx,TIM_MasterSlaveMode_Enable); 

	TIM_ITConfig(TIMx,TIM_IT_CCx| TIM_IT_Update, ENABLE);// ʹ�ܲ����жϣ�����ж���Ҫ��Ե���������ͨ����TI1FP1��
	TIM_ClearITPendingBit(TIMx, TIM_IT_CCx);// ����жϱ�־λ

	TIM_Cmd(TIMx, ENABLE);// ��������ʼ����

}

/*																						
*********************************************************************************************************
*	�� �� ��: TACH1_get
*	����˵��: 
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TACH1_get(void)//PB9 TIM4 CH4
{
		switch(TIM_ICUserValueStructure_TACH1.Get_State) 
		{
			case 0:
				TIM_ICUserValueStructure_TACH1.Get_State = 1;	
				TIM_SetCounter ( TIM4, 0 );// ��������0
				TIM_ICUserValueStructure_TACH1.Timer_Cnt = 0;					
				TIM_ICUserValueStructure_TACH1.Capture_CcrValue_UP = 0;// �沶��ȽϼĴ�����ֵ�ı�����ֵ��0	
				TIM_ICUserValueStructure_TACH1.Capture_CcrValue_DOWN = 0;
				TIM_OC4PolarityConfig(TIM4  , TIM_ICPolarity_Falling);// ����һ�β���������֮�󣬾ͰѲ����������Ϊ�½���
			break;
			case 1:					// �½��ز����ж�
				TIM_ICUserValueStructure_TACH1.Get_State = 2;
				TIM_ICUserValueStructure_TACH1.Capture_CcrValue_UP = TIM_GetCapture4(TIM4);// ��ȡ����ȽϼĴ�����ֵ�����ֵ���ǲ��񵽵ĸߵ�ƽ��ʱ���ֵ
				TIM_ICUserValueStructure_TACH1.Capture_Period_UP = TIM_ICUserValueStructure_TACH1.Timer_Cnt;	
				TIM_OC4PolarityConfig(TIM4, TIM_ICPolarity_Rising);// ���ڶ��β����½���֮�󣬾ͰѲ����������Ϊ�����أ��ÿ����µ�һ�ֲ���		
				break;
			case 2:	
				TIM_ICUserValueStructure_TACH1.Get_State = 3;
				TIM_ICUserValueStructure_TACH1.Capture_CcrValue_DOWN = TIM_GetCapture4(TIM4);
				TIM_ICUserValueStructure_TACH1.Capture_Period_DOWN = TIM_ICUserValueStructure_TACH1.Timer_Cnt;
				break;
			case 3:	
				TIM_ICUserValueStructure_TACH1.Capture_FinishFlag = 1;// ������ɱ�־��1	
				break;
		}	
}


/*																						
*********************************************************************************************************
*	�� �� ��: TACH2_get
*	����˵��: 
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TACH2_get(void)//PI2 TIM8 CH4 TACH2
{
		switch(TIM_ICUserValueStructure_TACH2.Get_State) 
		{
			case 0:
				TIM_ICUserValueStructure_TACH2.Get_State = 1;	
				TIM_SetCounter ( TIM8, 0 );// ��������0
				TIM_ICUserValueStructure_TACH2.Timer_Cnt = 0;					
				TIM_ICUserValueStructure_TACH2.Capture_CcrValue_UP = 0;// �沶��ȽϼĴ�����ֵ�ı�����ֵ��0	
				TIM_ICUserValueStructure_TACH2.Capture_CcrValue_DOWN = 0;
				TIM_OC4PolarityConfig(TIM8  , TIM_ICPolarity_Falling);// ����һ�β���������֮�󣬾ͰѲ����������Ϊ�½���
			break;
			case 1:					// �½��ز����ж�
				TIM_ICUserValueStructure_TACH2.Get_State = 2;
				TIM_ICUserValueStructure_TACH2.Capture_CcrValue_UP = TIM_GetCapture4(TIM8);// ��ȡ����ȽϼĴ�����ֵ�����ֵ���ǲ��񵽵ĸߵ�ƽ��ʱ���ֵ
				TIM_ICUserValueStructure_TACH2.Capture_Period_UP = TIM_ICUserValueStructure_TACH2.Timer_Cnt;	
				TIM_OC4PolarityConfig(TIM8 , TIM_ICPolarity_Rising);// ���ڶ��β����½���֮�󣬾ͰѲ����������Ϊ�����أ��ÿ����µ�һ�ֲ���		
				break;
			case 2:	
				TIM_ICUserValueStructure_TACH2.Get_State = 3;
				TIM_ICUserValueStructure_TACH2.Capture_CcrValue_DOWN = TIM_GetCapture4(TIM8);
				TIM_ICUserValueStructure_TACH2.Capture_Period_DOWN = TIM_ICUserValueStructure_TACH2.Timer_Cnt;
				break;
			case 3:	
				TIM_ICUserValueStructure_TACH2.Capture_FinishFlag = 1;// ������ɱ�־��1	
				break;
		}	
}

/*																						
*********************************************************************************************************
*	�� �� ��: TACH3_get
*	����˵��: 
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TACH3_get(void)//PH11 TIM5 CH2
{
		switch(TIM_ICUserValueStructure_TACH3.Get_State) 
		{
			case 0:
				if (s_tim5_owner == 4) break;   /* TACH4 正在测(要独占TIM5计数器),本边沿先让过,下个上升沿再试 */
				s_tim5_owner = 3;               /* 占用 TIM5,禁止 TACH4 在本次测量期间清计数器 */
				TIM_ICUserValueStructure_TACH3.Get_State = 1;
				TIM_SetCounter ( TIM5, 0 );// ��������0
				TIM_ICUserValueStructure_TACH3.Timer_Cnt = 0;
				TIM_ICUserValueStructure_TACH3.Capture_CcrValue_UP = 0;// �沶��ȽϼĴ�����ֵ�ı�����ֵ��0
				TIM_ICUserValueStructure_TACH3.Capture_CcrValue_DOWN = 0;
				TIM_OC2PolarityConfig(TIM5  , TIM_ICPolarity_Falling);// ����һ�β���������֮�󣬾ͰѲ����������Ϊ�½���
			break;
			case 1:					// �½��ز����ж�
				TIM_ICUserValueStructure_TACH3.Get_State = 2;
				TIM_ICUserValueStructure_TACH3.Capture_CcrValue_UP = TIM_GetCapture2(TIM5);// ��ȡ����ȽϼĴ�����ֵ�����ֵ���ǲ��񵽵ĸߵ�ƽ��ʱ���ֵ
				TIM_ICUserValueStructure_TACH3.Capture_Period_UP = TIM_ICUserValueStructure_TACH3.Timer_Cnt;	
				TIM_OC2PolarityConfig(TIM5 , TIM_ICPolarity_Rising);// ���ڶ��β����½���֮�󣬾ͰѲ����������Ϊ�����أ��ÿ����µ�һ�ֲ���		
				break;
			case 2:
				TIM_ICUserValueStructure_TACH3.Get_State = 3;
				TIM_ICUserValueStructure_TACH3.Capture_CcrValue_DOWN = TIM_GetCapture2(TIM5);
				TIM_ICUserValueStructure_TACH3.Capture_Period_DOWN = TIM_ICUserValueStructure_TACH3.Timer_Cnt;
				s_tim5_owner = 0;               /* 数值已锁存,释放 TIM5 让 TACH4 可以启动测量 */
				break;
			case 3:	
				TIM_ICUserValueStructure_TACH3.Capture_FinishFlag = 1;// ������ɱ�־��1	
				break;
		}	
}

/*																						
*********************************************************************************************************
*	�� �� ��: TACH4_get
*	����˵��: PB0 TIM3 CH3
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
static void TACH4_get(void)//PH10 TIM5 CH1
{
	switch(TIM_ICUserValueStructure_TACH4.Get_State) 
	{
	case 0:
		if (s_tim5_owner == 3) break;   /* TACH3 正在测(要独占TIM5计数器),本边沿先让过,下个上升沿再试 */
		s_tim5_owner = 4;               /* 占用 TIM5,禁止 TACH3 在本次测量期间清计数器 */
		TIM_ICUserValueStructure_TACH4.Get_State = 1;// ��������0
		TIM_SetCounter(TIM5, 0);// ��������0
		TIM_ICUserValueStructure_TACH4.Timer_Cnt = 0;
		TIM_ICUserValueStructure_TACH4.Capture_CcrValue_UP = 0;// �沶��ȽϼĴ�����ֵ�ı�����ֵ��0
		TIM_ICUserValueStructure_TACH4.Capture_CcrValue_DOWN = 0;
		TIM_OC1PolarityConfig(TIM5  , TIM_ICPolarity_Falling);// ����һ�β���������֮�󣬾ͰѲ����������Ϊ�½���
	break;
	case 1:					// �½��ز����ж�
		TIM_ICUserValueStructure_TACH4.Get_State = 2;
		TIM_ICUserValueStructure_TACH4.Capture_CcrValue_UP = TIM_GetCapture1(TIM5);// ��ȡ����ȽϼĴ�����ֵ�����ֵ���ǲ��񵽵ĸߵ�ƽ��ʱ���ֵ
		TIM_ICUserValueStructure_TACH4.Capture_Period_UP = TIM_ICUserValueStructure_TACH4.Timer_Cnt;	
		TIM_OC1PolarityConfig(TIM5 , TIM_ICPolarity_Rising);// ���ڶ��β����½���֮�󣬾ͰѲ����������Ϊ�����أ��ÿ����µ�һ�ֲ���		
		break;
	case 2:
		TIM_ICUserValueStructure_TACH4.Get_State = 3;
		TIM_ICUserValueStructure_TACH4.Capture_CcrValue_DOWN = TIM_GetCapture1(TIM5);
		TIM_ICUserValueStructure_TACH4.Capture_Period_DOWN = TIM_ICUserValueStructure_TACH4.Timer_Cnt;
		s_tim5_owner = 0;               /* 数值已锁存,释放 TIM5 让 TACH3 可以启动测量 */
		break;
	case 3:	
		TIM_ICUserValueStructure_TACH4.Capture_FinishFlag = 1;// ������ɱ�־��1	
		break;
	}	
}



/*																						
*********************************************************************************************************
*	�� �� ��: PWM_Configuration
*	����˵��: ��ʼ������ͨ�ö�ʱ��
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void PWM_Configuration(void)
{
	pwm_intput_gpio_Config();
	pwm_input_Config(INPUT1_CHANNEL);
	pwm_input_Config(INPUT2_CHANNEL);
	pwm_input_Config(INPUT3_CHANNEL);
	pwm_input_Config(INPUT4_CHANNEL);

	// ★ PWM输出必须后初始化，确保TIM8的period=OUTPUT_CNT-1不被输入捕获覆盖
	pwm_gpio_config();
	pwm_output_Config();

	// ★ 保存各通道实际定时器period值和tick速率,供RPM计算使用
	// TIM8与PWM共享,period被pwm_output_Config覆盖为OUTPUT_CNT
	TIM_ICUserValueStructure_TACH1.timer_period = INPUT_CNT;   // TIM4: 1000
	TIM_ICUserValueStructure_TACH1.timer_tick_us = 1;          // APB1: 84MHz/84=1MHz, 1tick=1µs

	TIM_ICUserValueStructure_TACH2.timer_period = OUTPUT_CNT;  // TIM8: 200 (与PWM共享)
	TIM_ICUserValueStructure_TACH2.timer_tick_us = 4;          // APB2: 168MHz/42=4MHz, 1tick=0.25µs

	TIM_ICUserValueStructure_TACH3.timer_period = INPUT_CNT;   // TIM5: 1000
	TIM_ICUserValueStructure_TACH3.timer_tick_us = 1;          // APB1: 84MHz/84=1MHz

	TIM_ICUserValueStructure_TACH4.timer_period = INPUT_CNT;   // TIM5: 1000
	TIM_ICUserValueStructure_TACH4.timer_tick_us = 1;          // APB1: 84MHz/84=1MHz

}
/*																						
*********************************************************************************************************
*	�� �� ��: TIM8_UP_TIM13_IRQHandler
*	����˵��: 
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TIM8_UP_TIM13_IRQHandler(void)
{
		if ( TIM_GetITStatus ( TIM8, TIM_IT_Update) != RESET )               
		{		
			if(TIM_ICUserValueStructure_TACH2.Capture_FinishFlag ==0 )
			{
				TIM_ICUserValueStructure_TACH2.Timer_Cnt++;
			}			
			TIM_ClearITPendingBit ( TIM8, TIM_IT_Update ); 		
		}
	
}
/*																						
*********************************************************************************************************
*	�� �� ��: TIM8_CC_IRQHandler
*	����˵��: 
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TIM8_CC_IRQHandler(void)
{
		if(TIM_GetITStatus(TIM8, TIM_IT_CC4) != RESET)
		{
			TACH2_get();
			TIM_ClearITPendingBit(TIM8,TIM_IT_CC4);// �����ʱ������жϱ�־λ		
		}
	
}
/*																						
*********************************************************************************************************
*	�� �� ��: TIM5_IRQHandler
*	����˵��: 
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TIM5_IRQHandler(void)
{
		if ( TIM_GetITStatus ( TIM5, TIM_IT_Update) != RESET )
		{
			if(TIM_ICUserValueStructure_TACH4.Capture_FinishFlag ==0 )
			{
				TIM_ICUserValueStructure_TACH4.Timer_Cnt++;
			}
			if(TIM_ICUserValueStructure_TACH3.Capture_FinishFlag ==0 )
			{
				TIM_ICUserValueStructure_TACH3.Timer_Cnt++;
			}

			/* 防饿死保护:占用方若在测量中途风扇停转(不再来边沿),会一直占着
			 * TIM5 使另一路无法启动。占用期间 Timer_Cnt 从 0 计起(1次溢出=1ms),
			 * 超过 1000(≈1s,远大于任何真实 tach 周期)就强制复位该路状态机、
			 * 恢复上升沿极性并释放,让另一路继续;停转那路等边沿恢复后自会重测。 */
			if (s_tim5_owner == 3 && TIM_ICUserValueStructure_TACH3.Timer_Cnt > 1000)
			{
				TIM_ICUserValueStructure_TACH3.Get_State = 0;
				TIM_OC2PolarityConfig(TIM5, TIM_ICPolarity_Rising);
				s_tim5_owner = 0;
			}
			else if (s_tim5_owner == 4 && TIM_ICUserValueStructure_TACH4.Timer_Cnt > 1000)
			{
				TIM_ICUserValueStructure_TACH4.Get_State = 0;
				TIM_OC1PolarityConfig(TIM5, TIM_ICPolarity_Rising);
				s_tim5_owner = 0;
			}

			TIM_ClearITPendingBit ( TIM5, TIM_IT_Update );
		}
		
		if(TIM_GetITStatus(TIM5, TIM_IT_CC1) != RESET)
		{
			TACH4_get();
			TIM_ClearITPendingBit(TIM5,TIM_IT_CC1);// �����ʱ������жϱ�־λ		
		}
		else if(TIM_GetITStatus(TIM5, TIM_IT_CC2) != RESET)
		{
			TACH3_get();
			TIM_ClearITPendingBit(TIM5,TIM_IT_CC2);// �����ʱ������жϱ�־λ		
		}
}
/*																						
*********************************************************************************************************
*	�� �� ��: TIM4_IRQHandler
*	����˵��: 
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void TIM4_IRQHandler(void)
{
		if ( TIM_GetITStatus ( TIM4, TIM_IT_Update) != RESET )               
		{		
			if(TIM_ICUserValueStructure_TACH1.Capture_FinishFlag ==0 )
			{
				TIM_ICUserValueStructure_TACH1.Timer_Cnt++;
			}			
			TIM_ClearITPendingBit ( TIM4, TIM_IT_Update ); 		
		}
		
		if(TIM_GetITStatus(TIM4, TIM_IT_CC4) != RESET)
		{
			TACH1_get();
			TIM_ClearITPendingBit(TIM4,TIM_IT_CC4);// �����ʱ������жϱ�־λ		
		}
}

