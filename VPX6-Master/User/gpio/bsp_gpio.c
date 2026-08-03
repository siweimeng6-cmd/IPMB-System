#include "bsp_gpio.h"
#include "bsp_init.h"


void GPIO_INPUT_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	
	/*��������GPIO�ڵ�ʱ��*/
	RCC_AHB1PeriphClockCmd(GPIO_GA_CLK | RACK_ID0_CLK | RACK_ID1_CLK | RACK_ID3_CLK | RACK_ID5_CLK, ENABLE);
	
	GPIO_InitStructure.GPIO_Pin = GPIO_GA0_PIN; 
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN;  							 //��������Ϊ����ģʽ
  GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL;  					 //�������Ų�����Ҳ������
	GPIO_Init(GPIO_GA0_PORT, &GPIO_InitStructure);     				 	 //��ʼ��GA0
  
	GPIO_InitStructure.GPIO_Pin = GPIO_GA1_PIN; 
	GPIO_Init(GPIO_GA1_PORT, &GPIO_InitStructure);  
	
	GPIO_InitStructure.GPIO_Pin = GPIO_GA2_PIN; 
	GPIO_Init(GPIO_GA2_PORT, &GPIO_InitStructure); 
	
	GPIO_InitStructure.GPIO_Pin = GPIO_GA3_PIN; 
	GPIO_Init(GPIO_GA3_PORT, &GPIO_InitStructure); 
	
	GPIO_InitStructure.GPIO_Pin = GPIO_GA4_PIN; 
	GPIO_Init(GPIO_GA4_PORT, &GPIO_InitStructure); 
	
	GPIO_InitStructure.GPIO_Pin = GPIO_GAP_PIN; 
	GPIO_Init(GPIO_GAP_PORT, &GPIO_InitStructure); 

	/* RACK_ID0~RACK_ID5 (PD5/PE0/PE1/PB12/PB13/PI3)  */
	GPIO_InitStructure.GPIO_Pin = RACK_ID0_PIN;
	GPIO_Init(RACK_ID0_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = RACK_ID1_PIN;
	GPIO_Init(RACK_ID1_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = RACK_ID2_PIN;
	GPIO_Init(RACK_ID2_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = RACK_ID3_PIN;
	GPIO_Init(RACK_ID3_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = RACK_ID4_PIN;
	GPIO_Init(RACK_ID4_PORT, &GPIO_InitStructure);

	GPIO_InitStructure.GPIO_Pin = RACK_ID5_PIN;
	GPIO_Init(RACK_ID5_PORT, &GPIO_InitStructure);

	
}

/*
**************************************************************************************************
*	�� �� ��: slave_addr_detect
*	����˵��: ���۵�ַѡ��,Ĭ��ֵ�ɼ�����Ϊ�ߵ�ƽ
*	��    ��: GA0---GAp,���忨�����˵��Ϊ������
*	�� �� ֵ: ��
**************************************************************************************************
*/
void slave_addr_detect(void)
{
	int  i			   	= 0;
	int  GA_VALUE		= 0;
	uint8_t GA_STATUS[6]	= {0, 0, 0, 0, 0, 0};
	
		/******��ȡ�趨�ܽŵ���������****************************/
	GA_STATUS[0] = GPIO_ReadInputDataBit(GPIO_GA0_PORT,GPIO_GA0_PIN);
	GA_STATUS[1] = GPIO_ReadInputDataBit(GPIO_GA1_PORT,GPIO_GA1_PIN);
	GA_STATUS[2] = GPIO_ReadInputDataBit(GPIO_GA2_PORT,GPIO_GA2_PIN);
	GA_STATUS[3] = GPIO_ReadInputDataBit(GPIO_GA3_PORT,GPIO_GA3_PIN);
	GA_STATUS[4] = GPIO_ReadInputDataBit(GPIO_GA4_PORT,GPIO_GA4_PIN);
	GA_STATUS[5] = GPIO_ReadInputDataBit(GPIO_GAP_PORT,GPIO_GAP_PIN);
	
	for(i=0;i<6;i++)
		GA_VALUE |=(GA_STATUS[i]<<i);//���ݿ��۵ĵ�ַ��Ϣ��ȷ���ӿ���I2C��ַ,ѭ�����ν����ж�
	
	printf("GA STATUS:%d\r\n",GA_VALUE);

}

/*
**************************************************************************************************
*	  : print_slot_addr
*	˵: GA0~GA4+GAPλԭʼ
*	    :
*	  ֵ:
**************************************************************************************************
*/
void print_slot_addr(void)
{
	uint8_t ga0  = GPIO_ReadInputDataBit(GPIO_GA0_PORT, GPIO_GA0_PIN);
	uint8_t ga1  = GPIO_ReadInputDataBit(GPIO_GA1_PORT, GPIO_GA1_PIN);
	uint8_t ga2  = GPIO_ReadInputDataBit(GPIO_GA2_PORT, GPIO_GA2_PIN);
	uint8_t ga3  = GPIO_ReadInputDataBit(GPIO_GA3_PORT, GPIO_GA3_PIN);
	uint8_t ga4  = GPIO_ReadInputDataBit(GPIO_GA4_PORT, GPIO_GA4_PIN);
	uint8_t gap  = GPIO_ReadInputDataBit(GPIO_GAP_PORT, GPIO_GAP_PIN);
	printf("GA   : GA0=%d GA1=%d GA2=%d GA3=%d GA4=%d GAP=%d\n",
		   ga0, ga1, ga2, ga3, ga4, gap);
}
void print_rack_addr(void)
{
	uint8_t rid0 = GPIO_ReadInputDataBit(RACK_ID0_PORT, RACK_ID0_PIN);
	uint8_t rid1 = GPIO_ReadInputDataBit(RACK_ID1_PORT, RACK_ID1_PIN);
	uint8_t rid2 = GPIO_ReadInputDataBit(RACK_ID2_PORT, RACK_ID2_PIN);
	uint8_t rid3 = GPIO_ReadInputDataBit(RACK_ID3_PORT, RACK_ID3_PIN);
	uint8_t rid4 = GPIO_ReadInputDataBit(RACK_ID4_PORT, RACK_ID4_PIN);
	uint8_t rid5 = GPIO_ReadInputDataBit(RACK_ID5_PORT, RACK_ID5_PIN);
	printf("RACK : RID0=%d RID1=%d RID2=%d RID3=%d RID4=%d RID5=%d\r\n",
		   rid0, rid1, rid2, rid3, rid4, rid5);
}

/*
**************************************************************************************************
*	  : GPIO_OUTPUT_Config
*	˵: GPIOųʼ(Ĭߵƽ)
*	    :
*	  ֵ:
**************************************************************************************************
*/
void GPIO_OUTPUT_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/*GPIOڵʱ*/
	RCC_AHB1PeriphClockCmd(BMC_CPLD_PWR_BTN_CLK | BMC_OUT_SYS_RST_CLK | LED_BMC_ALERT_CLK, ENABLE);

	/*BMC_CPLD_PWR_BTN(PA12)Ϊ,Ĭߵƽ*/
	GPIO_InitStructure.GPIO_Pin   = BMC_CPLD_PWR_BTN_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;    							//
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;  							//ٶ50MHz
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;      							//
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;   							//
	GPIO_Init(BMC_CPLD_PWR_BTN_PORT, &GPIO_InitStructure);

	GPIO_SetBits(BMC_CPLD_PWR_BTN_PORT, BMC_CPLD_PWR_BTN_PIN);    //Ĭߵƽ

	/*BMC_OUT_SYS_RST(PE2)Ϊ,Ĭߵƽ(λ͵ƽЧ)*/
	GPIO_InitStructure.GPIO_Pin = BMC_OUT_SYS_RST_PIN;
	GPIO_Init(BMC_OUT_SYS_RST_PORT, &GPIO_InitStructure);

	GPIO_SetBits(BMC_OUT_SYS_RST_PORT, BMC_OUT_SYS_RST_PIN);    //Ĭߵƽ




	/* LED_BMC_ALERT# (PE14) — 状态指示灯，低电平有效，默认熄灭(高电平) */
	GPIO_InitStructure.GPIO_Pin = LED_BMC_ALERT_PIN;
	GPIO_Init(LED_BMC_ALERT_PORT, &GPIO_InitStructure);

	GPIO_SetBits(LED_BMC_ALERT_PORT, LED_BMC_ALERT_PIN);	/* 默认高电平，LED熄灭 */
}

/*
**************************************************************************************************
*	  函 数 名: IPMB_Bus_Enable_Init
*	功能说明: NCA9511_DMSR 总线隔离器使能:IPMB-A(PH3)、IPMB-B(PH6)推挽输出,默认拉高(使能)
*	形    参: 无
*	返 回 值: 无
**************************************************************************************************
*/
void IPMB_Bus_Enable_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;

	/* 使能 GPIOH 时钟 */
	RCC_AHB1PeriphClockCmd(BMC_IPMBA_EN_CLK | BMC_IPMBB_EN_CLK, ENABLE);

	/* BMC_IPMBA_EN (PH3) — 默认高电平(使能 NCA9511_DMSR) */
	GPIO_InitStructure.GPIO_Pin   = BMC_IPMBA_EN_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	GPIO_Init(BMC_IPMBA_EN_PORT, &GPIO_InitStructure);
	GPIO_SetBits(BMC_IPMBA_EN_PORT, BMC_IPMBA_EN_PIN);

	/* BMC_IPMBB_EN (PH6) — 默认高电平(使能 NCA9511_DMSR) */
	GPIO_InitStructure.GPIO_Pin   = BMC_IPMBB_EN_PIN;
	GPIO_Init(BMC_IPMBB_EN_PORT, &GPIO_InitStructure);
	GPIO_SetBits(BMC_IPMBB_EN_PORT, BMC_IPMBB_EN_PIN);
}

/**************************************************************************************************
 * R14 BMC_GPIO2 / R15 BMC_GPIO3 / N11 BMC_CPLD_GPIO2 电源按钮控制
 * - R15 (PB15) 默认上拉,低脉冲 <2s 短按=开机,>=2s 长按=强制关机
 * - N11 (PE13) 默认高电平,下降沿=CPLD 软关机指示
 * - R14 (PB14) 默认低电平,延时 20ms 后翻转到目标电平
 * - EXTI 边沿事件启动软去抖,TIM6 每 1ms 推进状态机
 **************************************************************************************************/

/* R15 PB15 状态机 */
typedef enum {
	PWRBTN_STATE_IDLE = 0,         /* 空闲(默认高电平) */
	PWRBTN_STATE_DEBOUNCING,       /* 检测到下降沿,软去抖中 */
	PWRBTN_STATE_PRESSED,          /* 去抖通过,正在测量低脉冲宽度 */
	PWRBTN_STATE_REPORTED          /* 已上报,等待引脚回到高电平(防重报) */
} PWRBTN_State_t;

static volatile PWRBTN_State_t  s_pwrbtn_state        = PWRBTN_STATE_IDLE;
static volatile uint16_t        s_pwrbtn_debounce_ms  = 0;   /* 去抖计数 */
static volatile uint16_t        s_pwrbtn_low_ms       = 0;   /* 当前累积低电平时长 */
static volatile uint8_t         s_pwrbtn_reported_long = 0;  /* 长按是否已报过 */

/* N11 PE13 状态机(简单去抖后即报) */
static volatile uint8_t         s_cpld_debounce_ms    = 0;
static volatile uint8_t         s_cpld_falling_pending = 0;  /* EXTI 标记:已捕获下降沿 */

/* R14 PB14 20ms 延时输出控制(由 TIM6 ISR 翻转到目标电平) */
typedef enum {
	BMC_GPIO2_TARGET_IDLE = 0,
	BMC_GPIO2_TARGET_HIGH,
	BMC_GPIO2_TARGET_LOW
} BMC_GPIO2_Target_t;

static volatile BMC_GPIO2_Target_t s_gpio2_target    = BMC_GPIO2_TARGET_IDLE;
static volatile uint16_t           s_gpio2_delay_ms  = 0;

/* TIM6 节拍 1ms:APB1 定时器时钟 84 MHz(PCLK1=42MHz × 2) -> PSC=83,ARR=999 */
#define PWR_TIM_PRESCALER   (84 - 1)   /* 84MHz / 84 = 1MHz */
#define PWR_TIM_PERIOD      (1000 - 1) /* 1MHz / 1000 = 1kHz = 1ms */

/*
**************************************************************************************************
*	  : GPIO_Power_Config
*	˵: R14(OUT)/R15(IN)/N11(IN) 初始化 + EXTI 配置 + TIM6 1ms 节拍启动
*	    :
*	  ֵ: (NVIC 抢占优先级统一在 bsp_init.c::NVIC_Configuration() 中配为 5)
**************************************************************************************************
*/
void GPIO_Power_Config(void)
{
	GPIO_InitTypeDef  GPIO_InitStructure;
	EXTI_InitTypeDef  EXTI_InitStructure;
	TIM_TimeBaseInitTypeDef TIM_InitStructure;

	/* 1. 使能 GPIO 时钟 */
	RCC_AHB1PeriphClockCmd(BMC_GPIO2_CLK | BMC_GPIO3_CLK | BMC_CPLD_GPIO2_CLK, ENABLE);
	/* SYSCFG 时钟(EXTI 需要) */
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);
	/* TIM6 时钟 */
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM6, ENABLE);

	/* 2. R14 BMC_GPIO2 (PB14) 推挽输出,默认低电平 */
	GPIO_InitStructure.GPIO_Pin   = BMC_GPIO2_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_NOPULL;
	GPIO_Init(BMC_GPIO2_PORT, &GPIO_InitStructure);
	GPIO_ResetBits(BMC_GPIO2_PORT, BMC_GPIO2_PIN);   /* 上电默认低电平(通知电源板断电) */

	/* 3. R15 BMC_GPIO3 (PB15) 上拉输入(默认高电平) */
	GPIO_InitStructure.GPIO_Pin   = BMC_GPIO3_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
	GPIO_Init(BMC_GPIO3_PORT, &GPIO_InitStructure);

	/* 4. N11 BMC_CPLD_GPIO2 (PE13) 上拉输入(默认高电平) */
	GPIO_InitStructure.GPIO_Pin   = BMC_CPLD_GPIO2_PIN;
	GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IN;
	GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
	GPIO_Init(BMC_CPLD_GPIO2_PORT, &GPIO_InitStructure);

	/* 5. R15 PB15 -> EXTI15,下降沿(进入按压)中断 */
	SYSCFG_EXTILineConfig(BMC_GPIO3_EXTI_PORTSOURCE, BMC_GPIO3_EXTI_PINSOURCE);
	EXTI_InitStructure.EXTI_Line    = BMC_GPIO3_EXTI_LINE;
	EXTI_InitStructure.EXTI_Mode    = EXTI_Mode_Interrupt;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;     /* 下降沿=按压开始 */
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	/* 6. N11 PE13 -> EXTI13,下降沿(CPLD 软关机指示)中断 */
	SYSCFG_EXTILineConfig(BMC_CPLD_GPIO2_EXTI_PORTSOURCE, BMC_CPLD_GPIO2_EXTI_PINSOURCE);
	EXTI_InitStructure.EXTI_Line    = BMC_CPLD_GPIO2_EXTI_LINE;
	EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
	EXTI_InitStructure.EXTI_LineCmd = ENABLE;
	EXTI_Init(&EXTI_InitStructure);

	/* 7. TIM6 1ms 周期 */
	TIM_InitStructure.TIM_Prescaler         = PWR_TIM_PRESCALER;
	TIM_InitStructure.TIM_CounterMode       = TIM_CounterMode_Up;
	TIM_InitStructure.TIM_Period            = PWR_TIM_PERIOD;
	TIM_InitStructure.TIM_ClockDivision     = TIM_CKD_DIV1;
	TIM_InitStructure.TIM_RepetitionCounter = 0;
	TIM_TimeBaseInit(TIM6, &TIM_InitStructure);
	TIM_ClearITPendingBit(TIM6, TIM_IT_Update);
	TIM_ITConfig(TIM6, TIM_IT_Update, ENABLE);
	TIM_Cmd(TIM6, ENABLE);

	/* 注:EXTI15_10 和 TIM6_DAC 的 NVIC 优先级在 bsp_init.c::NVIC_Configuration() 中配置为 5 */
}

/*
**************************************************************************************************
*	  : Set_BMC_GPIO2_With_20msDelay
*	˵: 设置 R14 目标电平,延时 20ms 后由 TIM6 ISR 翻转
*	    : target   0=拉低,1=拉高
*	  ֵ:
**************************************************************************************************
*/
static void Set_BMC_GPIO2_With_20msDelay(uint8_t target)
{
	s_gpio2_target   = target ? BMC_GPIO2_TARGET_HIGH : BMC_GPIO2_TARGET_LOW;
	s_gpio2_delay_ms = BMC_GPIO_OUTPUT_DELAY_MS;
}

/*
**************************************************************************************************
*	  : PowerButton_EXTI_IRQHandler
*	˵: PB15 / PE13 边沿事件入口(由 stm32f4xx_it.c::EXTI15_10_IRQHandler 转发)
*	    : 仅记录"边沿已发生";实际判定在 TIM6 ISR 的状态机里完成
*	  ֵ:
**************************************************************************************************
*/
void PowerButton_EXTI_IRQHandler(void)
{
	/* R15 PB15 下降沿:启动软去抖 + 重置长按已报标志 */
	if(EXTI_GetITStatus(BMC_GPIO3_EXTI_LINE) != RESET)
	{
		if(s_pwrbtn_state == PWRBTN_STATE_IDLE)
		{
			s_pwrbtn_state       = PWRBTN_STATE_DEBOUNCING;
			s_pwrbtn_debounce_ms = 0;
			s_pwrbtn_low_ms      = 0;
			s_pwrbtn_reported_long = 0;
		}
		EXTI_ClearITPendingBit(BMC_GPIO3_EXTI_LINE);
	}

	/* N11 PE13 下降沿:启动软去抖 */
	if(EXTI_GetITStatus(BMC_CPLD_GPIO2_EXTI_LINE) != RESET)
	{
		s_cpld_falling_pending = 1;
		s_cpld_debounce_ms     = 0;
		EXTI_ClearITPendingBit(BMC_CPLD_GPIO2_EXTI_LINE);
	}

	/* 当前 EXTI ISR 只更新状态变量,真正的队列投递在 TIM6 ISR 中完成;
	 * 此处无需 portEND_SWITCHING_ISR。 */
}

/*
**************************************************************************************************
*	  : PowerButton_TIM6_IRQHandler
*	˵: 1ms 节拍:推进 R15 状态机 / PE13 去抖 / R14 延时输出
*	    : 由 stm32f4xx_it.c::TIM6_DAC_IRQHandler 转发
*	  ֵ:
**************************************************************************************************
*/
void PowerButton_TIM6_IRQHandler(void)
{
	GPIO_Cmd_t cmd;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	uint8_t  pin_level;

	if(TIM_GetITStatus(TIM6, TIM_IT_Update) == RESET)
		return;
	TIM_ClearITPendingBit(TIM6, TIM_IT_Update);

	/* TIM6 在 BSP_Init() 中启动,可能比 xGpioCmdQueue 创建早一拍;在队列就绪前不要投递 */
	if(xGpioCmdQueue == NULL)
	{
		portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
		return;
	}

	/* === 1) R15 PB15 状态机推进 === */
	pin_level = GPIO_ReadInputDataBit(BMC_GPIO3_PORT, BMC_GPIO3_PIN);

	switch(s_pwrbtn_state)
	{
		case PWRBTN_STATE_IDLE:
			/* 空闲:等待 EXTI 下降沿(已在 EXTI ISR 中切换到 DEBOUNCING) */
			break;

		case PWRBTN_STATE_DEBOUNCING:
			s_pwrbtn_debounce_ms++;
			if(pin_level == Bit_RESET)   /* 仍为低,去抖通过 */
			{
				if(s_pwrbtn_debounce_ms >= BMC_GPIO_DEBOUNCE_MS)
				{
					s_pwrbtn_state       = PWRBTN_STATE_PRESSED;
					s_pwrbtn_low_ms      = 0;
				}
			}
			else                          /* 去抖期间回到高:视为抖动,放弃 */
			{
				s_pwrbtn_state = PWRBTN_STATE_IDLE;
			}
			break;

		case PWRBTN_STATE_PRESSED:
			if(pin_level == Bit_RESET)
			{
				s_pwrbtn_low_ms++;

				/* 异常保护:超过阈值仍未释放,直接报一次长按 */
				if(!s_pwrbtn_reported_long && s_pwrbtn_low_ms >= BMC_GPIO3_LONG_FORCE_MS)
				{
					s_pwrbtn_reported_long = 1;
					cmd = GPIO_CMD_PWRBTN_LONG_PRESS;
					xQueueSendFromISR(xGpioCmdQueue, &cmd, &xHigherPriorityTaskWoken);
				}
			}
			else
			{
				/* 上升沿:本次按压结束,根据累积时长上报 */
				if(!s_pwrbtn_reported_long)
				{
					cmd = (s_pwrbtn_low_ms < BMC_GPIO3_SHORT_THRESHOLD_MS)
					      ? GPIO_CMD_PWRBTN_SHORT_PRESS
					      : GPIO_CMD_PWRBTN_LONG_PRESS;
					xQueueSendFromISR(xGpioCmdQueue, &cmd, &xHigherPriorityTaskWoken);
					s_pwrbtn_reported_long = 1;  /* 防止随后"超时强制报"重复 */
				}
				s_pwrbtn_state = PWRBTN_STATE_REPORTED;
			}
			break;

		case PWRBTN_STATE_REPORTED:
			/* 等引脚回到高电平后回 IDLE(防抖) */
			if(pin_level == Bit_SET)
			{
				s_pwrbtn_state = PWRBTN_STATE_IDLE;
			}
			break;

		default:
			s_pwrbtn_state = PWRBTN_STATE_IDLE;
			break;
	}

	/* === 2) N11 PE13 软去抖 + 触发 === */
	if(s_cpld_falling_pending)
	{
		pin_level = GPIO_ReadInputDataBit(BMC_CPLD_GPIO2_PORT, BMC_CPLD_GPIO2_PIN);
		if(pin_level == Bit_RESET)
		{
			s_cpld_debounce_ms++;
			if(s_cpld_debounce_ms >= BMC_GPIO_DEBOUNCE_MS)
			{
				s_cpld_falling_pending = 0;
				cmd = GPIO_CMD_SOFT_PWRDN_FROM_CPLD;
				xQueueSendFromISR(xGpioCmdQueue, &cmd, &xHigherPriorityTaskWoken);
			}
		}
		else
		{
			/* 去抖期间回到高:放弃 */
			s_cpld_falling_pending = 0;
		}
	}

	/* === 3) R14 PB14 延时翻转 === */
	if(s_gpio2_target != BMC_GPIO2_TARGET_IDLE && s_gpio2_delay_ms > 0)
	{
		s_gpio2_delay_ms--;
		if(s_gpio2_delay_ms == 0)
		{
			if(s_gpio2_target == BMC_GPIO2_TARGET_HIGH)
			{
				GPIO_SetBits(BMC_GPIO2_PORT, BMC_GPIO2_PIN);
				printf("BMC_GPIO2 (PB14) -> HIGH\r\n");
			}
			else
			{
				GPIO_ResetBits(BMC_GPIO2_PORT, BMC_GPIO2_PIN);
				printf("BMC_GPIO2 (PB14) -> LOW\r\n");
			}
			s_gpio2_target = BMC_GPIO2_TARGET_IDLE;
		}
	}

	portEND_SWITCHING_ISR(xHigherPriorityTaskWoken);
}

/*
**************************************************************************************************
*	  : BMC_CPLD_PWR_BTN_Control
*	˵: BMC_CPLD_PWR_BTN(PA12)λ,100ms͵ƽȻٴ
*	    :
*	  ֵ:
**************************************************************************************************
*/
/* PWR_BTN 状态机：避免延时阻塞 GPIO_Task 的 LED 心跳 */
typedef enum {
	PWR_BTN_STATE_IDLE = 0,    /* 空闲 */
	PWR_BTN_STATE_PULL_LOW_ON, /* 已拉低,等 300ms 后拉高 -> power on */
	PWR_BTN_STATE_PULL_LOW_OFF /* 已拉低,等 5s   后拉高 -> power off */
} PWR_BTN_State_t;

static PWR_BTN_State_t s_pwr_btn_state = PWR_BTN_STATE_IDLE;
static uint16_t        s_pwr_btn_ticks = 0;                 /* 已累计的 300ms 周期数 */
#define PWR_BTN_ON_HOLD_TICKS   2                            /* 300ms  / 300ms = 1  -> power on  */
#define PWR_BTN_OFF_HOLD_TICKS  17                           /* 5000ms / 300ms ≈ 17 -> power off */

void BMC_CPLD_PWR_BTN_Control(void)
{
	/* 兼容旧接口:等同于 power off(5s) */
	BMC_CPLD_PWR_BTN_PowerOff();
}

void BMC_CPLD_PWR_BTN_PowerOn(void)
{
	/* 300ms 低脉冲 -> 开机 */
	GPIO_ResetBits(BMC_CPLD_PWR_BTN_PORT, BMC_CPLD_PWR_BTN_PIN);  /* 拉低 PA12 */
	s_pwr_btn_state = PWR_BTN_STATE_PULL_LOW_ON;
	s_pwr_btn_ticks = 0;
	Set_BMC_GPIO2_With_20msDelay(1);   /* 20ms 后拉高 R14,通知电源板上电 */
	printf("BMC_CPLD PWR_BTN power on (300ms)...\r\n");
}

void BMC_CPLD_PWR_BTN_PowerOff(void)
{
	/* 5s 低脉冲 -> 关机 */
	GPIO_ResetBits(BMC_CPLD_PWR_BTN_PORT, BMC_CPLD_PWR_BTN_PIN);
	s_pwr_btn_state = PWR_BTN_STATE_PULL_LOW_OFF;
	s_pwr_btn_ticks = 0;
	Set_BMC_GPIO2_With_20msDelay(0);   /* 20ms 后拉低 R14,通知电源板断电 */
	printf("BMC_CPLD PWR_BTN power off (5s)...\r\n");
}

/*
**************************************************************************************************
*	  : BMC_OUT_SYS_RST_Reset
*	˵: BMC_OUT_SYS_RST(PE2)λ,100ms͵ƽȻٴ
*	    :
*	  ֵ:
**************************************************************************************************
*/
void BMC_OUT_SYS_RST_Reset(void)
{
	GPIO_ResetBits(BMC_OUT_SYS_RST_PORT, BMC_OUT_SYS_RST_PIN);
	//delay_1ms(100);
	vTaskDelay(100 / portTICK_PERIOD_MS);  /* 延时100ms */
	GPIO_SetBits(BMC_OUT_SYS_RST_PORT, BMC_OUT_SYS_RST_PIN);

	printf("BMC_OUT_SYS_RST reset done.\r\n");

}

/* GPIO  */
xQueueHandle   xGpioCmdQueue   = NULL;
TaskHandle_t   GPIO_Task_Handle = NULL;

/*
**************************************************************************************************
*	  : GPIO_Task
*	˵: GPIO,ȴλ,յִλ
*	    : parameter
*	  ֵ:
**************************************************************************************************
*/
void GPIO_Task(void* parameter)
{
	GPIO_Cmd_t cmd;
	const TickType_t xBlinkDelay = pdMS_TO_TICKS(10);   /* 10ms 轮询:及时响应 EXTI/TIM6 投递的电源按钮事件 */
	static uint8_t led_state = 0;
	static uint16_t blink_divider = 0;

	while(1)
	{
		if(xQueueReceive(xGpioCmdQueue, &cmd, xBlinkDelay) == pdTRUE)
		{
			switch(cmd)
			{
			case GPIO_CMD_POWER_ON_BMC_CPLD_PWR_BTN:
				BMC_CPLD_PWR_BTN_PowerOn();
				Fan_Set_All_Duty(30); /* 开机时风扇转速 30% */
	
				break;
			case GPIO_CMD_POWER_OFF_BMC_CPLD_PWR_BTN:
				Fan_Set_All_Duty(0);
				BMC_CPLD_PWR_BTN_PowerOff();

				break;
			case GPIO_CMD_RESET_BMC_OUT_SYS_RST:
				BMC_OUT_SYS_RST_Reset();
				break;
			case GPIO_CMD_PWRBTN_SHORT_PRESS:
				/* R15 短按(<2s):延时 20ms 后拉高 R14(通知电源板上电) */
				printf("BMC_GPIO3 (PB15) short press -> power on\r\n");
				Fan_Set_All_Duty(30); /* 开机时风扇转速 30% */

				Set_BMC_GPIO2_With_20msDelay(1);
                break;   
			case GPIO_CMD_PWRBTN_LONG_PRESS:
				/* R15 长按(>=2s):延时 20ms 后拉低 R14(通知电源板断电) */
				printf("BMC_GPIO3 (PB15) long press -> force power off\r\n");
				Set_BMC_GPIO2_With_20msDelay(0);

				Fan_Set_All_Duty(0);

				break;
			case GPIO_CMD_SOFT_PWRDN_FROM_CPLD:
				/* N11 PE13 软关机指示:延时 20ms 后拉低 R14(通知电源板断电) */
				printf("BMC_CPLD_GPIO2 (PE13) soft power down\r\n");
				Set_BMC_GPIO2_With_20msDelay(0);
				Fan_Set_All_Duty(0);

				break;
			default:
				break;
			}
		}
		else
		{
			/* 10ms 定时:累计 30 次翻转一次 LED_BMC_ALERT# (= 300ms 心跳) */
			blink_divider++;
			if(blink_divider >= 30)
			{
				blink_divider = 0;
				if(led_state)
				{
					GPIO_SetBits(LED_BMC_ALERT_PORT, LED_BMC_ALERT_PIN);    /* 高电平，LED熄灭 */
					led_state = 0;
				}
				else
				{
					GPIO_ResetBits(LED_BMC_ALERT_PORT, LED_BMC_ALERT_PIN);  /* 低电平，LED点亮 */
					led_state = 1;
				}

				/* PWR_BTN 状态机推进:每 300ms 计一次,计满对应 ticks 后拉高 */
				if(s_pwr_btn_state == PWR_BTN_STATE_PULL_LOW_ON)
				{
					s_pwr_btn_ticks++;
					if(s_pwr_btn_ticks >= PWR_BTN_ON_HOLD_TICKS)
					{
						GPIO_SetBits(BMC_CPLD_PWR_BTN_PORT, BMC_CPLD_PWR_BTN_PIN);
						s_pwr_btn_state = PWR_BTN_STATE_IDLE;
						printf("BMC_CPLD PWR_BTN power on done.\r\n");
					}
				}
				else if(s_pwr_btn_state == PWR_BTN_STATE_PULL_LOW_OFF)
				{
					s_pwr_btn_ticks++;
					if(s_pwr_btn_ticks >= PWR_BTN_OFF_HOLD_TICKS)
					{
						GPIO_SetBits(BMC_CPLD_PWR_BTN_PORT, BMC_CPLD_PWR_BTN_PIN);
						s_pwr_btn_state = PWR_BTN_STATE_IDLE;
						printf("BMC_CPLD PWR_BTN power off done.\r\n");
					}
				}
			}
		}
	}
}