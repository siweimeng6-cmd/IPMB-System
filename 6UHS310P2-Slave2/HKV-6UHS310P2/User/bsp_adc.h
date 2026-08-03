#ifndef __ADC_H
#define	__ADC_H


#include "stm32f10x.h"

// ע�⣺����ADC�ɼ���IO����û�и��ã�����ɼ���ѹ����Ӱ��
/********************ADC1����ͨ�������ţ�����**************************/
#define    ADC_APBxClock_FUN             RCC_APB2PeriphClockCmd
#define    ADC_CLK                       RCC_APB2Periph_ADC1

#define    ADC_GPIO_APBxClock_FUN        RCC_APB2PeriphClockCmd


// ת��ͨ������
#define 	 ADC_CONVERT_CHANNEL        		5				//ת��ͨ����
#define 	 MAX_ADC_COLL_CNTS						20								//ת������
#define 	 ADC_VREF                       3.3f      					//3.3V
#define 	 ADC_PRE_12BIT                  4096.0f   					//2^12


// ADC1 - PA6 (P12V��ϵ��0.1)
#define    ADC_CNT1                      GPIO_Pin_6
#define    ADC_CNT1_PORT                 GPIOA
#define    ADC_CNT1_CHANNEL              ADC_Channel_6

// ADC2 - PA7 (P5V��ϵ��0.2)
#define    ADC_CNT2                      GPIO_Pin_7
#define    ADC_CNT2_PORT                 GPIOA
#define    ADC_CNT2_CHANNEL              ADC_Channel_7

// ADC3 - PB0 (P3V3��ϵ��0.3)
#define    ADC_CNT3                      GPIO_Pin_0
#define    ADC_CNT3_PORT                 GPIOB
#define    ADC_CNT3_CHANNEL              ADC_Channel_8

// ADC4 - PB1 (DDR5_VDD_P1V��ϵ��1)
#define    ADC_CNT4                      GPIO_Pin_1
#define    ADC_CNT4_PORT                 GPIOB
#define    ADC_CNT4_CHANNEL              ADC_Channel_9

// ADC5 - PC0 (CPU0_VDD_CORE   ϵ��1)
#define    ADC_CNT5                     	 GPIO_Pin_0
#define    ADC_CNT5_PORT                 GPIOC
#define    ADC_CNT5_CHANNEL              ADC_Channel_10





// ADC1 ��Ӧ DMA1ͨ��1��ADC3��ӦDMA2ͨ��5��ADC2û��DMA����
#define    ADC_x                         ADC1
#define    ADC_DMA_CHANNEL               DMA1_Channel1
#define    ADC_DMA_CLK                   RCC_AHBPeriph_DMA1

typedef struct
{
  volatile uint32_t adc_filter_val[ADC_CONVERT_CHANNEL];								
  volatile float adc_voltage_val[ADC_CONVERT_CHANNEL];				
}stADC_DATA_t, *pstADC_DATA_t;

extern stADC_DATA_t stADC_Data;


/************************** API 声明 ********************************/
void ADCx_Init(void);
void adc_convert(void);
void ADC_Calculation(void);

#endif /* __ADC_H */
