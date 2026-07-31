#include "bsp_init.h"
#include ".\gpio\bsp_gpio.h"


uint8_t line2_low = 0;
uint8_t line4_low = 0;
uint8_t line5_low = 0;
int  GA_VALUE		= 0;
int  RACK_ID		= 0;

uint8_t mainboard_power_state = 0;   // 驱动PA15状态灯; 由 Power_Ctrl_Task 据PB5硬件反馈更新,
                                      // 也由 ipmb_fru.c 的上电/下电/软关机指令立即同步更新, 0=off, 1=on
uint8_t power_state_confirmed = 0;   // 0=尚未确认(MCU刚上电，PB5防抖/IPMB均未更新过，不可信) 1=已确认
                                      // 由 Power_Ctrl_Task(PB5确认)或UART/IPMB电源命令置1，见 task_fan.c 里
                                      // Fan_Ctrl_Task 用它避免刚上电头几秒把 mainboard_power_state 的占位默认值
                                      // 误判成"真下电"、错误地把还在运行的主板风扇强制停转
volatile uint8_t ipmb_reset_flag = 0;  // 由 ipmb_fru.c 置位，Power_Ctrl_Task 异步消费

/*																						
*********************************************************************************************************
*	函 数 名: bsp_output_gpio_init
*	功能说明: 
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void bsp_output_gpio_init(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;

		RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA , ENABLE);
	
		GPIO_InitStructure.GPIO_Pin = MCU_PWR_CTL_GPIO_PIN | MCU_OUT_SYSRESET_GPIO_PIN
																	|IPMBA_EN_GPIO_PIN | IPMBB_EN_GPIO_PIN | MCU_STATUS_LED_GPIO_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOA, &GPIO_InitStructure);

		GPIO_ResetBits(MCU_PWR_CTL_GPIO_PORT, MCU_PWR_CTL_GPIO_PIN);					//PA4拉低
		GPIO_SetBits(MCU_OUT_SYSRESET_GPIO_PORT, MCU_OUT_SYSRESET_GPIO_PIN);	//PA12拉高
		GPIO_SetBits(IPMBA_EN_GPIO_PORT, IPMBA_EN_GPIO_PIN);
		GPIO_SetBits(IPMBB_EN_GPIO_PORT, IPMBB_EN_GPIO_PIN);
		GPIO_ResetBits(MCU_STATUS_LED_GPIO_PORT, MCU_STATUS_LED_GPIO_PIN);		//PA15初始熄灭

}

void bsp_input_gpio_init(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;

		RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOC, ENABLE);

		/* CPEX GA mapping: GA0..GA3=PA8..PA11, GA4/GAP=PC6/PC7.
		 * 必须用内部上拉输入: 背板只把"应为0"的 GA 线接地, "应为1"的线保持开路,
		 * 靠单板内部上拉才能稳定读成高电平。浮空输入会导致开路 GA 线电平不定、
		 * 从机地址随上电漂移(与参考工程 HKV-6UHS310P2 保持一致)。 */
		GPIO_InitStructure.GPIO_Pin = MCU_GA0_GPIO_PIN | MCU_GA1_GPIO_PIN |
									 MCU_GA2_GPIO_PIN | MCU_GA3_GPIO_PIN;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(GPIOA, &GPIO_InitStructure);

		GPIO_InitStructure.GPIO_Pin = MCU_GA4_GPIO_PIN | MCU_GAP_GPIO_PIN;
		GPIO_Init(GPIOC, &GPIO_InitStructure);
	
//		GPIO_InitStructure.GPIO_Pin = MCU_RACK_ID0_GPIO_PIN | MCU_RACK_ID1_GPIO_PIN | MCU_RACK_ID2_GPIO_PIN |
//																	MCU_RACK_ID3_GPIO_PIN | MCU_RACK_ID4_GPIO_PIN | MCU_RACK_ID5_GPIO_PIN;	
//		GPIO_Init(GPIOB, &GPIO_InitStructure);
}


/*
*********************************************************************************************************
*	函 数 名: payld_power_gpio_init
*	功能说明: 配置主板电源状态反馈信号(PB5)为输入
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void payld_power_gpio_init(void)
{
		GPIO_InitTypeDef GPIO_InitStructure;

		RCC_APB2PeriphClockCmd( RCC_APB2Periph_GPIOB , ENABLE);

		GPIO_InitStructure.GPIO_Pin   = PAYLD_SYSPOWER_GPIO_PIN;
		GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_IPU;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
		GPIO_Init(PAYLD_SYSPOWER_GPIO_PORT, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	函 数 名: Power_On / Power_Off
*	功能说明: 通过PWR_CTRL(PA4,低电平有效)软控制主板上电/下电
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void Power_On(void)
{
		GPIO_ResetBits(MCU_PWR_CTL_GPIO_PORT, MCU_PWR_CTL_GPIO_PIN);	//PA4拉低=命令主板上电
}

void Power_Off(void)
{
		GPIO_SetBits(MCU_PWR_CTL_GPIO_PORT, MCU_PWR_CTL_GPIO_PIN);		//PA4拉高=命令主板下电/软关机
}

/*
*********************************************************************************************************
*	函 数 名: MCU_Reset_Pulse
*	功能说明: 对PA12(MCU_OUT_SYSRESET)输出一个低电平复位脉冲(100ms)，空闲态为高电平
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void MCU_Reset_Pulse(void)
{
		GPIO_ResetBits(MCU_OUT_SYSRESET_GPIO_PORT, MCU_OUT_SYSRESET_GPIO_PIN);	//拉低，触发复位
		vTaskDelay(100);
		GPIO_SetBits(MCU_OUT_SYSRESET_GPIO_PORT, MCU_OUT_SYSRESET_GPIO_PIN);	//拉高，复位释放
}

void gpio_init(void)
{
		bsp_output_gpio_init();
		bsp_input_gpio_init();
		payld_power_gpio_init();

}

/*
**************************************************************************************************
*	函 数 名: slave_addr_detect
*	功能说明: 卡槽地址选择,默认值采集可能为高电平
*	形    参: GA0---GAp,本板卡相对来说作为从器件
*	返 回 值: 无
**************************************************************************************************
*/
void slave_addr_detect(void)
{
	int  i			   	= 0;
	uint8_t GA_STATUS[6]	= {0, 0, 0, 0, 0, 0};
	uint8_t MCU_RACK_ID[6]	= {0, 0, 0, 0, 0, 0};

	GA_VALUE = 0;   /* 全局变量, 下面用 |= 累加, 入口先清零避免二次调用累积错误 */

		/******读取设定管脚的输入数据****************************/
	GA_STATUS[0] = GPIO_ReadInputDataBit(MCU_GA0_GPIO_PORT,MCU_GA0_GPIO_PIN);
	GA_STATUS[1] = GPIO_ReadInputDataBit(MCU_GA1_GPIO_PORT,MCU_GA1_GPIO_PIN);
	GA_STATUS[2] = GPIO_ReadInputDataBit(MCU_GA2_GPIO_PORT,MCU_GA2_GPIO_PIN);
	GA_STATUS[3] = GPIO_ReadInputDataBit(MCU_GA3_GPIO_PORT,MCU_GA3_GPIO_PIN);
	GA_STATUS[4] = GPIO_ReadInputDataBit(MCU_GA4_GPIO_PORT,MCU_GA4_GPIO_PIN);
	GA_STATUS[5] = GPIO_ReadInputDataBit(MCU_GAP_GPIO_PORT,MCU_GAP_GPIO_PIN);
	
	for(i=0;i<6;i++)
		GA_VALUE |=(GA_STATUS[i]<<i);//根据卡槽的地址信息来确定子卡的I2C地址,循环六次进行判断
	
	printf("GA raw:0x%02X, Slot:%d\r\n", GA_VALUE, (~GA_VALUE) & 0x1F);
	
//	MCU_RACK_ID[0] = GPIO_ReadInputDataBit(MCU_RACK_ID0_GPIO_PORT,MCU_RACK_ID0_GPIO_PIN);
//	MCU_RACK_ID[1] = GPIO_ReadInputDataBit(MCU_RACK_ID1_GPIO_PORT,MCU_RACK_ID1_GPIO_PIN);
//	MCU_RACK_ID[2] = GPIO_ReadInputDataBit(MCU_RACK_ID2_GPIO_PORT,MCU_RACK_ID2_GPIO_PIN);
//	MCU_RACK_ID[3] = GPIO_ReadInputDataBit(MCU_RACK_ID3_GPIO_PORT,MCU_RACK_ID3_GPIO_PIN);
//	MCU_RACK_ID[4] = GPIO_ReadInputDataBit(MCU_RACK_ID4_GPIO_PORT,MCU_RACK_ID4_GPIO_PIN);
//	MCU_RACK_ID[5] = GPIO_ReadInputDataBit(MCU_RACK_ID5_GPIO_PORT,MCU_RACK_ID5_GPIO_PIN);
//	
//	for(i=0;i<6;i++)
//		RACK_ID |=(MCU_RACK_ID[i]<<i);//根据卡槽的地址信息来确定子卡的I2C地址,循环六次进行判断
//	
//	printf("MCU_RACK_ID:%d\r\n",RACK_ID);
	
}





