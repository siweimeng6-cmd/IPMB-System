#ifndef __BSP_TIMER_H
#define	__BSP_TIMER_H

#include "bsp_init.h"
#include "stm32f10x.h"


#define            PWM_OUTPUT_TIM                   TIM1
#define            PWM_OUTPUT_TIM_APBxClock_FUN     RCC_APB2PeriphClockCmd
#define            PWM_OUTPUT_TIM_CLK               RCC_APB2Periph_TIM1
// TIM1 输出比较通道1
#define            PWM_OUTPUT_TIM_CH1_GPIO_CLK      RCC_APB2Periph_GPIOA
#define            PWM_OUTPUT_TIM_CH1_PORT          GPIOA
#define            PWM_OUTPUT_TIM_CH1_PIN           GPIO_Pin_8

// TIM1 输出比较通道2
#define            PWM_OUTPUT_TIM_CH2_GPIO_CLK      RCC_APB2Periph_GPIOA
#define            PWM_OUTPUT_TIM_CH2_PORT          GPIOA
#define            PWM_OUTPUT_TIM_CH2_PIN           GPIO_Pin_9


/************通用定时器TIM参数定义，只限TIM2、3、4、5************/
// 当使用不同的定时器的时候，对应的GPIO是不一样的，这点要注意
// 我们这里默认使用TIM5

#define            FAN_TACH_TIM                   TIM3
#define            FAN_TACH_TIM_APBxClock_FUN     RCC_APB1PeriphClockCmd
#define            FAN_TACH_TIM_CLK               RCC_APB1Periph_TIM3

// TIM 输入捕获通道GPIO相关宏定义
#define            FAN_TACH_TIM_CH1_GPIO_CLK      RCC_APB2Periph_GPIOA
#define            FAN_TACH_TIM_CH1_PORT          GPIOA
#define            FAN_TACH_TIM_CH1_PIN           GPIO_Pin_6

#define            FAN_TACH_TIM_CH2_GPIO_CLK      RCC_APB2Periph_GPIOA
#define            FAN_TACH_TIM_CH2_PORT          GPIOA
#define            FAN_TACH_TIM_CH2_PIN           GPIO_Pin_7

#define            FAN_TACH_TIM_CH3_GPIO_CLK      RCC_APB2Periph_GPIOB
#define            FAN_TACH_TIM_CH3_PORT          GPIOB
#define            FAN_TACH_TIM_CH3_PIN           GPIO_Pin_0

#define            FAN_TACH_TIM_CH4_GPIO_CLK      RCC_APB2Periph_GPIOB
#define            FAN_TACH_TIM_CH4_PORT          GPIOB
#define            FAN_TACH_TIM_CH4_PIN           GPIO_Pin_1

#define            FAN_TACH_TIM_IRQ               TIM3_IRQn
#define            FAN_TACH_TIM_INT_FUN           TIM3_IRQHandler

#define            FAN_TACH_TIM_GetCapture_FUN_CH1                 TIM_GetCapture1				// 获取捕获寄存器值函数宏定义
#define            FAN_TACH_TIM_OCPolarityConfig_FUN_CH1           TIM_OC1PolarityConfig // 捕获信号极性函数宏定义
#define            FAN_TACH_TIM_GetCapture_FUN_CH2                 TIM_GetCapture2				
#define            FAN_TACH_TIM_OCPolarityConfig_FUN_CH2           TIM_OC2PolarityConfig 
#define            FAN_TACH_TIM_GetCapture_FUN_CH3                 TIM_GetCapture3			
#define            FAN_TACH_TIM_OCPolarityConfig_FUN_CH3           TIM_OC3PolarityConfig 
#define            FAN_TACH_TIM_GetCapture_FUN_CH4                 TIM_GetCapture4				
#define            FAN_TACH_TIM_OCPolarityConfig_FUN_CH4           TIM_OC4PolarityConfig 


#define Input_TIM_Prescaler											72
#define Input_CNT																1000
#define Output_TIM_Prescaler										72
#define OUTPUT_CNT                              100
#define UP_CNT																	25


// 定时器输入捕获用户自定义变量结构体声明
typedef struct
{   
	uint8_t   Capture_FinishFlag;   // 捕获结束标志位
	uint16_t  Capture_CcrValue_UP;     // 捕获寄存器的值
	uint16_t  Capture_CcrValue_DOWN;     // 捕获寄存器的值
	uint16_t  Capture_Period_UP;       // 自动重装载寄存器更新标志 
	uint16_t  Capture_Period_DOWN;       // 自动重装载寄存器更新标志 
	uint8_t 	Get_State;
	uint16_t  Timer_Cnt;
	uint8_t 	Channel_en;
}TIM_ICUserValueTypeDef;


extern TIM_ICUserValueTypeDef TIM_ICUserValueStructure_CH1 ;
extern TIM_ICUserValueTypeDef TIM_ICUserValueStructure_CH2 ;
extern TIM_ICUserValueTypeDef TIM_ICUserValueStructure_CH3 ;
extern TIM_ICUserValueTypeDef TIM_ICUserValueStructure_CH4 ;

/**************************函数声明********************************/
void pwm_init(void);
void pwm_func(void);
void FAN_TACH_TIM_Mode_Config(uint16_t channel);

#endif 
