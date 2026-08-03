#include "bsp_adc.h"
#include "bsp_init.h"
#include <string.h>
#include <stdio.h>

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
static void ADCx_GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    // 打开 ADC IO端口时钟
    ADC_GPIO_APBxClock_FUN ( RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC , ENABLE );

    // 配置 ADC IO 引脚模式
    // PA6 - ADC6  +12V_HOT_Mointor 12v  系数0.1
    GPIO_InitStructure.GPIO_Pin =  ADC_CNT1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(ADC_CNT1_PORT, &GPIO_InitStructure);

    // PA7 - ADC7  DDRPHY_M 0.75v  系数1
    GPIO_InitStructure.GPIO_Pin =  ADC_CNT2;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(ADC_CNT2_PORT, &GPIO_InitStructure);

    // PB0 - ADC8  DDRPHY_S 0.75v  系数1
    GPIO_InitStructure.GPIO_Pin =  ADC_CNT3;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(ADC_CNT3_PORT, &GPIO_InitStructure);

    // PB1 - ADC9  V_VSYS_Mointor   3.8v  系数0.3
    GPIO_InitStructure.GPIO_Pin =  ADC_CNT4;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(ADC_CNT4_PORT, &GPIO_InitStructure);

    // PC0 - ADC10  VRD_MOS_Mointor 3.3v  系数0.2
    GPIO_InitStructure.GPIO_Pin =  ADC_CNT5;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(ADC_CNT5_PORT, &GPIO_InitStructure);
}

/*
*********************************************************************************************************
*	函 数 名: ADCx_Mode_Config
*	功能说明: 
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
static void ADCx_Mode_Config(void)
{
    DMA_InitTypeDef DMA_InitStructure;
    ADC_InitTypeDef ADC_InitStructure;

    RCC_AHBPeriphClockCmd(ADC_DMA_CLK, ENABLE);// 打开DMA时钟
    ADC_APBxClock_FUN ( ADC_CLK, ENABLE );// 打开ADC时钟

    DMA_DeInit(ADC_DMA_CHANNEL);// 复位DMA控制器

    // 配置 DMA 初始化结构体
    DMA_InitStructure.DMA_PeripheralBaseAddr = ( u32 ) ( & ( ADC_x->DR ) );// 外设基址为：ADC 数据寄存器地址
    DMA_InitStructure.DMA_MemoryBaseAddr = (u32)ADC_ConvertedValue;// 存储器地址
    DMA_InitStructure.DMA_DIR = DMA_DIR_PeripheralSRC;// 数据源来自外设
    DMA_InitStructure.DMA_BufferSize = ADC_CONVERT_CHANNEL*MAX_ADC_COLL_CNTS;// 缓冲区大小，应该等于数据目的地的大小
    DMA_InitStructure.DMA_PeripheralInc = DMA_PeripheralInc_Disable;// 外设寄存器只有一个，地址不用递增
    DMA_InitStructure.DMA_MemoryInc = DMA_MemoryInc_Enable; // 存储器地址递增
    DMA_InitStructure.DMA_PeripheralDataSize = DMA_PeripheralDataSize_HalfWord;// 外设数据大小为半字，即两个字节
    DMA_InitStructure.DMA_MemoryDataSize = DMA_MemoryDataSize_HalfWord;// 内存数据大小也为半字，跟外设数据大小相同
    DMA_InitStructure.DMA_Mode = DMA_Mode_Circular;// 循环传输模式
    DMA_InitStructure.DMA_Priority = DMA_Priority_High;// DMA 传输通道优先级为高，当使用一个DMA通道时，优先级设置不影响
    DMA_InitStructure.DMA_M2M = DMA_M2M_Disable;// 禁止存储器到存储器模式，因为是从外设到存储器
    DMA_Init(ADC_DMA_CHANNEL, &DMA_InitStructure);// 初始化DMA

    // 使能 DMA 通道
    DMA_Cmd(ADC_DMA_CHANNEL , ENABLE);

    // ADC 模式配置
    ADC_InitStructure.ADC_Mode = ADC_Mode_Independent;// 只使用一个ADC，属于单模式
    ADC_InitStructure.ADC_ScanConvMode = ENABLE ; // 扫描模式
    ADC_InitStructure.ADC_ContinuousConvMode = ENABLE;// 连续转换模式
    ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;// 不用外部触发转换，软件开启即可
    ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;// 转换结果右对齐
    ADC_InitStructure.ADC_NbrOfChannel = ADC_CONVERT_CHANNEL;// 转换通道个数
    ADC_Init(ADC_x, &ADC_InitStructure);// 初始化ADC

    RCC_ADCCLKConfig(RCC_PCLK2_Div8);  // 配置ADC时钟Ｎ狿CLK2的8分频，即9MHz

    // 配置ADC 通道的转换顺序和采样时间
    ADC_RegularChannelConfig(ADC_x, ADC_CNT1_CHANNEL, 1, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC_x, ADC_CNT2_CHANNEL, 2, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC_x, ADC_CNT3_CHANNEL, 3, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC_x, ADC_CNT4_CHANNEL, 4, ADC_SampleTime_55Cycles5);
    ADC_RegularChannelConfig(ADC_x, ADC_CNT5_CHANNEL, 5, ADC_SampleTime_55Cycles5);

    ADC_DMACmd(ADC_x, ENABLE);// 使能ADC DMA 请求

    ADC_Cmd(ADC_x, ENABLE);// 开启ADC ，并开始转换

    ADC_ResetCalibration(ADC_x);// 初始化ADC 校准寄存器  
    while(ADC_GetResetCalibrationStatus(ADC_x));// 等待校准寄存器初始化完成

    ADC_StartCalibration(ADC_x);// ADC开始校准
    while(ADC_GetCalibrationStatus(ADC_x));// 等待校准完成

    ADC_SoftwareStartConvCmd(ADC_x, ENABLE);// 由于没有采用外部触发，所以使用软件触发ADC转换 
}

/*
*********************************************************************************************************
*	函 数 名: ADC_Filter
*	功能说明: ADC取平均
*	形    参: 无
*	返 回 值: 无
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
*	函 数 名: ADCx_Init
*	功能说明: ADC配置
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void ADCx_Init(void)
{
    ADCx_GPIO_Config();
    ADCx_Mode_Config();
}

/*
*********************************************************************************************************
*	函 数 名: ADC_Calculation
*	功能说明: ADC实际数值计算
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void ADC_Calculation(void)
{
    uint8_t i = 0;
    char flash_buf[128] = {0};
    
    ADC_Filter();
    for( i = 0; i < ADC_CONVERT_CHANNEL; i++ )
    {
            if(i==0){      //ADC6  +12V_HOT_Mointor 12v  系数0.1
                stADC_Data.adc_voltage_val[i]=stADC_Data.adc_filter_val[i]/ADC_PRE_12BIT*ADC_VREF/0.084;
                sprintf((char *)flash_buf,"12V_HOT_Mointor:%.2fV\r\n",stADC_Data.adc_voltage_val[i]);
            }
            else if(i==1){      //ADC7  DDRPHY_M 0.75v  系数1
                stADC_Data.adc_voltage_val[i]=stADC_Data.adc_filter_val[i]/ADC_PRE_12BIT*ADC_VREF/1.01;
                sprintf((char *)flash_buf,"0.75V_DDRPHY_M:%.2fV\r\n",stADC_Data.adc_voltage_val[i]);
            }
            else if(i==2){      //ADC8  DDRPHY_S 0.75v  系数1
                stADC_Data.adc_voltage_val[i]=stADC_Data.adc_filter_val[i]/ADC_PRE_12BIT*ADC_VREF/1.01;
                sprintf((char *)flash_buf,"0.75V_DDRPHY_S:%.2fV\r\n",stADC_Data.adc_voltage_val[i]);
            }
            else if(i==3){      //ADC9  V_VSYS_Mointor   3.8v  系数0.3
                stADC_Data.adc_voltage_val[i]=stADC_Data.adc_filter_val[i]/ADC_PRE_12BIT*ADC_VREF/0.31;
                sprintf((char *)flash_buf,"3.8V_V_VSYS_Mointor:%.2fV\r\n",stADC_Data.adc_voltage_val[i]);
            }
            else if(i==4){      //ADC10  VRD_MOS_Mointor 5v  系数0.2
                stADC_Data.adc_voltage_val[i]=stADC_Data.adc_filter_val[i]/ADC_PRE_12BIT*ADC_VREF/0.2;
                sprintf((char *)flash_buf,"5V_VRD_MOS_Mointor:%.2fV\r\n",stADC_Data.adc_voltage_val[i]);
            }

            strcat((char *)stPrintf_Buf.buf, flash_buf);
            memset(flash_buf, 0, 128);
    }
}

