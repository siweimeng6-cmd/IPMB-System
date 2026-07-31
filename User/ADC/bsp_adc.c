#include "bsp_adc.h"
#include "bsp_init.h"

__IO uint16_t ADC_ConvertedValue[ADC_CONVERT_CHANNEL*MAX_ADC_COLL_CNTS]={0};
stADC_DATA_t  stADC_Data;
/*
*********************************************************************************************************
*	函 数 名: ADC_GPIO_Config
*	功能说明: ADC GPIO初始化
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void ADC_GPIO_Config(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;	

	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOF,ENABLE);// 使能 GPIO 时钟

	GPIO_InitStructure.GPIO_Pin = P12V_HOT_MONITOR_GPIO_PIN | VCC3V_GPIO_PIN | VCC_DDR_GPIO_PIN | VCC_CORE_GPIO_PIN | VCC_X100_GPIO_PIN;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;	
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;//不上拉不下拉
	GPIO_Init(GPIOF, &GPIO_InitStructure);
}
/*
*********************************************************************************************************
*	函 数 名: ADC_DMA_Config
*	功能说明: DMA Init 结构体参数 初始化
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void ADC_DMA_Config(void)
{
	DMA_InitTypeDef DMA_InitStructure;

	/****ADC3使用DMA2，数据流0，通道0，这个是手册固定死的*******/
	RCC_AHB1PeriphClockCmd(ADC_DMA_CLK, ENABLE);   // 开启DMA时钟
	DMA_DeInit(ADC_DMA_STREAM);

	DMA_InitStructure.DMA_PeripheralBaseAddr = COM_ADC_DR_ADDR;// 外设基址为：ADC 数据寄存器地址
	DMA_InitStructure.DMA_Memory0BaseAddr = (uint32_t)ADC_ConvertedValue;// 存储器地址，实际上就是一个内部SRAM的变量	
	DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralToMemory;// 数据传输方向为外设到存储器	
	DMA_InitStructure.DMA_BufferSize = ADC_CONVERT_CHANNEL*MAX_ADC_COLL_CNTS;// 缓冲区大小为，指一次传输的数据量
	DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;// 外设寄存器只有一个，地址不用递增
	DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable;// 存储器地址固定,地址递增
	DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;// 外设数据大小为半字，即两个字节 
	DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;// 存储器数据大小也为半字，跟外设数据大小相同

	DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;// 循环传输模式
	DMA_InitStructure.DMA_Priority = DMA_Priority_High;// DMA 传输通道优先级为高，当使用一个DMA通道时，优先级设置不影响
	DMA_InitStructure.DMA_FIFOMode = DMA_FIFOMode_Disable;// 禁止DMA FIFO	，使用直连模式	
	DMA_InitStructure.DMA_FIFOThreshold = DMA_FIFOThreshold_HalfFull;// FIFO 大小，FIFO模式禁止时，这个不用配置
	DMA_InitStructure.DMA_MemoryBurst = DMA_MemoryBurst_Single;
	DMA_InitStructure.DMA_PeripheralBurst = DMA_PeripheralBurst_Single;  
	DMA_InitStructure.DMA_Channel = ADC_DMA_CHANNEL;// 选择 DMA 通道，通道存在于流中
	DMA_Init(ADC_DMA_STREAM, &DMA_InitStructure);//初始化DMA流，流相当于一个大的管道，管道里面有很多通道

	DMA_Cmd(ADC_DMA_STREAM, ENABLE);// 使能DMA流

}

/*
*********************************************************************************************************
*	函 数 名: ADC_Config
*	功能说明: ADC 结构体参数初始化
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void ADC_Config(void)
{
	ADC_InitTypeDef ADC_InitStructure;
	ADC_CommonInitTypeDef ADC_CommonInitStructure;

	RCC_APB2PeriphClockCmd(COM_ADC_CLK , ENABLE);// 开启ADC时钟


	ADC_CommonInitStructure.ADC_Mode = ADC_Mode_Independent;// 独立ADC模式
	ADC_CommonInitStructure.ADC_Prescaler = ADC_Prescaler_Div4;// 时钟为fpclk x分频	
	ADC_CommonInitStructure.ADC_DMAAccessMode = ADC_DMAAccessMode_Disabled;// 禁止DMA直接访问模式	
	ADC_CommonInitStructure.ADC_TwoSamplingDelay = ADC_TwoSamplingDelay_20Cycles;// 采样时间间隔	
	ADC_CommonInit(&ADC_CommonInitStructure);

	//ADC_StructInit(&ADC_InitStructure);	
	ADC_InitStructure.ADC_Resolution = ADC_Resolution_12b;// ADC 分辨率
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;// 扫描模式，多通道采集需要	
	ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;// 连续转换	
	ADC_InitStructure.ADC_ExternalTrigConvEdge = ADC_ExternalTrigConvEdge_None;//禁止外部边沿触发
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T1_CC1;//外部触发通道，本例子使用软件触发，此值随便赋值即可
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;//数据右对齐	
	ADC_InitStructure.ADC_NbrOfConversion = ADC_CONVERT_CHANNEL;//转换通道 4个  
	ADC_Init(ADC3 , &ADC_InitStructure);


	// 配置 ADC 通道转换顺序和采样时间周期
	ADC_RegularChannelConfig(ADC3, P12V_HOT_MONITOR_CHANNEL, 1, ADC_SampleTime_3Cycles);
	ADC_RegularChannelConfig(ADC3, VCC3V_CHANNEL, 2, ADC_SampleTime_3Cycles); 
	ADC_RegularChannelConfig(ADC3, VCC_DDR_CHANNEL, 3, ADC_SampleTime_3Cycles); 
	ADC_RegularChannelConfig(ADC3, VCC_CORE_CHANNEL, 4, ADC_SampleTime_3Cycles); 
	ADC_RegularChannelConfig(ADC3, VCC_X100_CHANNEL, 5, ADC_SampleTime_3Cycles); 


	ADC_DMARequestAfterLastTransferCmd(ADC3, ENABLE);// 使能DMA请求 after last transfer (Single-ADC mode)
	ADC_DMACmd(ADC3, ENABLE);// 使能ADC DMA
	ADC_Cmd(ADC3, ENABLE);// 使能ADC
	ADC_SoftwareStartConv(ADC3);//开始adc转换，软件触发
}

/*
*********************************************************************************************************
*	函 数 名: ADC_Config
*	功能说明: ADC配置
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void Bsp_ADC_Init(void)
{
	ADC_GPIO_Config();
	ADC_DMA_Config();
	ADC_Config();	
}

/*
*********************************************************************************************************
** name   : ADC_Filter
** input  : None
** output : None
** discr  : ADC_Filter
*********************************************************************************************************
*/
void ADC_Filter( void )
{
	uint32_t adc_aver_sum[ADC_CONVERT_CHANNEL]={0};//ADC转换和
	uint16_t i = 0, j = 0;
	
	for(i = 0; i < (MAX_ADC_COLL_CNTS * ADC_CONVERT_CHANNEL); i += ADC_CONVERT_CHANNEL){
		for(j = 0; j < ADC_CONVERT_CHANNEL; j++)
		  adc_aver_sum[j] += ADC_ConvertedValue[i+j];
	}
	
	for(i = 0; i < ADC_CONVERT_CHANNEL; i++){
		stADC_Data.adc_filter_val[i] = adc_aver_sum[i] / MAX_ADC_COLL_CNTS;
	}
}



/*
*********************************************************************************************************
** name   : ADC_Calculation
** input  : pstADC_Val
** output : None
** discr  : 严格匹配原理图的分压/采样规则修正
*********************************************************************************************************
*/
void ADC_Calculation( stADC_DATA_t *pstADC_Val )
{
    uint8_t i = 0;
    char flash_buf[128] = {0};
    // 12位ADC最大值（浮点型避免整数除法精度丢失）
    const float ADC_12BIT_MAX = 4095.0f;  
    // ADC参考电压（确认硬件实际值，通常为3.3V，若为其他值需修改）
    const float ADC_REF_VOLTAGE = 3.3f;  
		
	
	
    for( i = 0; i < ADC_CONVERT_CHANNEL; i++ )
    {
        // 基础公式：真实电压 = (ADC采样值 / 4095) × 参考电压 × 分压还原系数
        switch(i)
        {
            case 0: // +12V_HOT（分压系数：（47k+10k）/10K = 5.7）
                stADC_Data.adc_voltage_val[i] = (stADC_Data.adc_filter_val[i] / ADC_12BIT_MAX) * ADC_REF_VOLTAGE * 5.7f;
                sprintf((char *)flash_buf,"V_12V：%.2f V\r\n", stADC_Data.adc_voltage_val[i]);
                break;
                
            case 1: // VCC3（分压系数：（2.2k+1k）/1K = 3.2）
                stADC_Data.adc_voltage_val[i] = (stADC_Data.adc_filter_val[i] / ADC_12BIT_MAX) * ADC_REF_VOLTAGE * 3.2f;
                sprintf((char *)flash_buf,"V_3.3V：%.2f V\r\n", stADC_Data.adc_voltage_val[i]);
                break;
                
            case 2: // CPU0_VDDQ（1.2V，无分压）
                stADC_Data.adc_voltage_val[i] = (stADC_Data.adc_filter_val[i] / ADC_12BIT_MAX) * ADC_REF_VOLTAGE;
                sprintf((char *)flash_buf,"V_1.2V：%.2f V\r\n", stADC_Data.adc_voltage_val[i]);
                break;
                
            case 3: // CPU0_VCORE（0.85V，无分压）
                stADC_Data.adc_voltage_val[i] = (stADC_Data.adc_filter_val[i] / ADC_12BIT_MAX) * ADC_REF_VOLTAGE;
                sprintf((char *)flash_buf,"V_0.85V：%.2f V\r\n", stADC_Data.adc_voltage_val[i]);
                break;
                
            case 4: // X100_VDD_PV08（~1V，无分压）
                stADC_Data.adc_voltage_val[i] = (stADC_Data.adc_filter_val[i] / ADC_12BIT_MAX) * ADC_REF_VOLTAGE;
                sprintf((char *)flash_buf,"V_X100：%.2f V\r\n", stADC_Data.adc_voltage_val[i]);
                break;
                
            default:
                stADC_Data.adc_voltage_val[i] = 0.0f;
                sprintf((char *)flash_buf,"未知通道：%.2f V\r\n", stADC_Data.adc_voltage_val[i]);
                break;
        }
        strncat((char *)stPrintf_Buf.buf, flash_buf, sizeof(stPrintf_Buf.buf) - strlen((char *)stPrintf_Buf.buf) - 1);	/* 有边界追加,防越界踩坏全局内存 */
        memset(flash_buf, 0, 128);
    }
}

	
