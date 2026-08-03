#ifndef __BSP_ADC_H
#define	__BSP_ADC_H

#include "stm32f4xx.h"

#define P12V_HOT_MONITOR_GPIO_PORT    				GPIOF
#define P12V_HOT_MONITOR_GPIO_PIN     				GPIO_Pin_4
#define P12V_HOT_MONITOR_GPIO_CLK     				RCC_AHB1Periph_GPIOF
#define P12V_HOT_MONITOR_CHANNEL      				ADC_Channel_14

#define VCC3V_GPIO_PORT    							GPIOF
#define VCC3V_GPIO_PIN     							GPIO_Pin_5
#define VCC3V_GPIO_CLK     							RCC_AHB1Periph_GPIOF
#define VCC3V_CHANNEL      							ADC_Channel_15

#define VCC_DDR_GPIO_PORT    						GPIOF
#define VCC_DDR_GPIO_PIN     						GPIO_Pin_6
#define VCC_DDR_GPIO_CLK	   						RCC_AHB1Periph_GPIOF
//#define VCC_DDR_CHANNEL    		  				ADC_Channel_3
#define VCC_DDR_CHANNEL      		  				ADC_Channel_4

#define VCC_CORE_GPIO_PORT    						GPIOF
#define VCC_CORE_GPIO_PIN     						GPIO_Pin_7
#define VCC_CORE_GPIO_CLK     						RCC_AHB1Periph_GPIOF
//#define VCC_CORE_CHANNEL      		  			ADC_Channel_4
#define VCC_CORE_CHANNEL      		  				ADC_Channel_5

#define VCC_X100_GPIO_PORT    						GPIOF
#define VCC_X100_GPIO_PIN     						GPIO_Pin_8
#define VCC_X100_GPIO_CLK     						RCC_AHB1Periph_GPIOF
//#define VCC_X100_CHANNEL      		  			ADC_Channel_5
#define VCC_X100_CHANNEL      		  				ADC_Channel_6

// ADC 序号宏定义
#define COM_ADC_CLK          						RCC_APB2Periph_ADC3
// ADC DR寄存器宏定义，ADC转换后的数字值则存放在这里
#define COM_ADC_DR_ADDR    							((uint32_t)ADC3+0x4c)

#define ADC_CONVERT_CHANNEL      					5	//转换通道数
#define MAX_ADC_COLL_CNTS							1//转换轮数
#define ADC_VREF                     				3.3f//3.3V
#define ADC_PRE_12BIT                				4096.0f//2^12

// ADC DMA 通道宏定义，这里我们使用DMA传输
#define ADC_DMA_CLK      							RCC_AHB1Periph_DMA2
#define ADC_DMA_CHANNEL  							DMA_Channel_2
#define ADC_DMA_STREAM   							DMA2_Stream0
/**********************************************************/

typedef struct
{
	volatile uint32_t adc_filter_val[ADC_CONVERT_CHANNEL];	
	volatile float adc_current_val;//1路电流
//volatile float adc_voltage_val[ADC_CONVERT_CHANNEL-1];	//3路电压
	volatile float adc_voltage_val[ADC_CONVERT_CHANNEL];//3路电压
}stADC_DATA_t, *pstADC_DATA_t;

extern stADC_DATA_t  stADC_Data;

/***********************函数声明***************************/
void Bsp_ADC_Init(void);
void ADC_Filter( void );
void ADC_Calculation( stADC_DATA_t *pstADC_Val );

#endif /* __BSP_ADC_H */
