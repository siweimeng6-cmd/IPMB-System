#ifndef __BSP_PWM_H
#define	__BSP_PWM_H

#include "stm32f4xx.h"
#include "bsp_init.h"

/*******************************PWM���**************************************/             
#define OCPWM1_GPIO_PORT       						GPIOE                      
#define OCPWM1_GPIO_CLK        						RCC_AHB1Periph_GPIOE
#define OCPWM1_CHANNEL_PIN             				GPIO_Pin_6 
#define OCPWM1_CHANNEL_PINSOURCE					GPIO_PinSource6
#define OCPWM1_AF									GPIO_AF_TIM9
#define OCPWM1_TIM           		    			TIM9
#define OCPWM1_TIM_CLK       		    			RCC_APB2Periph_TIM9


#define OCPWM2_GPIO_PORT       						GPIOE                      
#define OCPWM2_GPIO_CLK        						RCC_AHB1Periph_GPIOE
#define OCPWM2_CHANNEL_PIN             				GPIO_Pin_5 
#define OCPWM2_CHANNEL_PINSOURCE					GPIO_PinSource5
#define OCPWM2_AF									GPIO_AF_TIM9
#define OCPWM2_TIM           		    			TIM9
#define OCPWM2_TIM_CLK       		    			RCC_APB2Periph_TIM9


#define OCPWM3_GPIO_PORT       						GPIOC                      
#define OCPWM3_GPIO_CLK        						RCC_AHB1Periph_GPIOC
#define OCPWM3_CHANNEL_PIN             				GPIO_Pin_8 
#define OCPWM3_CHANNEL_PINSOURCE					GPIO_PinSource8
#define OCPWM3_AF									GPIO_AF_TIM8
#define OCPWM3_TIM           		    			TIM8
#define OCPWM3_TIM_CLK       		    			RCC_APB2Periph_TIM8


#define OCPWM4_GPIO_PORT       						GPIOI                      
#define OCPWM4_GPIO_CLK        						RCC_AHB1Periph_GPIOI
#define OCPWM4_CHANNEL_PIN             				GPIO_Pin_6
#define OCPWM4_CHANNEL_PINSOURCE					GPIO_PinSource6
#define OCPWM4_AF									GPIO_AF_TIM8
#define OCPWM4_TIM           		    			TIM8
#define OCPWM4_TIM_CLK       		    			RCC_APB2Periph_TIM8


typedef enum {
    OCPWM1_CHANNEL = 0,   
    OCPWM2_CHANNEL,       
    OCPWM3_CHANNEL,       
    OCPWM4_CHANNEL,        
} ocpwm_channel_t;


/*******************************PWM����**************************************/ 
#define INPUT_PWM1_GPIO_PORT       					GPIOB                      
#define INPUT_PWM1_GPIO_CLK        					RCC_AHB1Periph_GPIOB
#define	INPUT_PWM1_CHANNEL_PIN             			GPIO_Pin_9
#define INPUT_PWM1_CHANNEL_PINSOURCE				GPIO_PinSource9
#define INPUT_PWM1_AF								GPIO_AF_TIM4
#define INPUT_PWM1_TIM           		    		TIM4
#define INPUT_PWM1_TIM_CLK       		    		RCC_APB1Periph_TIM4
#define INPUT_PWM1_TIM_IRQn					    	TIM4_IRQn
#define INPUT_PWM1_TIM_IRQHandler       			TIM4_IRQHandler


#define INPUT_PWM2_GPIO_PORT       					GPIOI                      
#define INPUT_PWM2_GPIO_CLK        					RCC_AHB1Periph_GPIOI
#define	INPUT_PWM2_CHANNEL_PIN             			GPIO_Pin_2
#define INPUT_PWM2_CHANNEL_PINSOURCE				GPIO_PinSource2
#define INPUT_PWM2_AF								GPIO_AF_TIM8
#define INPUT_PWM2_TIM           		    		TIM8
#define INPUT_PWM2_TIM_CLK       		    		RCC_APB2Periph_TIM8
#define INPUT_PWM2_TIM_IRQn					    	TIM8_IRQn
#define INPUT_PWM2_TIM_IRQHandler       			TIM8_IRQHandler

#define INPUT_PWM3_GPIO_PORT       					GPIOH                      
#define INPUT_PWM3_GPIO_CLK        					RCC_AHB1Periph_GPIOH
#define	INPUT_PWM3_CHANNEL_PIN             			GPIO_Pin_11
#define INPUT_PWM3_CHANNEL_PINSOURCE				GPIO_PinSource11
#define INPUT_PWM3_AF								GPIO_AF_TIM5
#define INPUT_PWM3_TIM           		    		TIM5
#define INPUT_PWM3_TIM_CLK       		    		RCC_APB1Periph_TIM5
#define INPUT_PWM3_TIM_IRQn					    	TIM5_IRQn
#define INPUT_PWM3_TIM_IRQHandler       			TIM5_IRQHandler


#define INPUT_PWM4_GPIO_PORT       					GPIOH                      
#define INPUT_PWM4_GPIO_CLK        					RCC_AHB1Periph_GPIOH
#define	INPUT_PWM4_CHANNEL_PIN             			GPIO_Pin_10
#define INPUT_PWM4_CHANNEL_PINSOURCE				GPIO_PinSource10
#define INPUT_PWM4_AF								GPIO_AF_TIM5
#define INPUT_PWM4_TIM           		    		TIM5
#define INPUT_PWM4_TIM_CLK       		    		RCC_APB1Periph_TIM5
#define INPUT_PWM4_TIM_IRQn					    	TIM5_IRQn
#define INPUT_PWM4_TIM_IRQHandler       			TIM5_IRQHandler


typedef enum {
    INPUT1_CHANNEL = 0,   
    INPUT2_CHANNEL,       
    INPUT3_CHANNEL,       
    INPUT4_CHANNEL,        
} input_channel_t;
/****************************************************************************/ 

#define APB1_TIM_Prescaler						84
#define APB2_TIM_Prescaler						42
#define APB1_CLK								84000000
#define APB2_CLK								42000000
#define OUTPUT_CNT                              200
#define INPUT_CNT                              	1000
#define UP_CNT									100


// ��ʱ�����벶���û��Զ�������ṹ������
typedef struct
{   
	uint8_t   Capture_FinishFlag;              // ���������־λ
	uint16_t  Capture_CcrValue_UP;             // ����Ĵ�����ֵ
	uint16_t  Capture_CcrValue_DOWN;           // ����Ĵ�����ֵ
	uint16_t  Capture_Period_UP;               // �Զ���װ�ؼĴ������±�־ 
	uint16_t  Capture_Period_DOWN;             // �Զ���װ�ؼĴ������±�־ 
	uint8_t   Get_State;
	uint16_t  Timer_Cnt;
	uint8_t   Channel_en;
	uint16_t  timer_period;         // 记录该通道定时器的实际period值
	uint8_t   timer_tick_us;        // 每微秒tick数: APB1定时器=1, APB2定时器=4
}TIM_ICUserValueTypeDef;



extern TIM_ICUserValueTypeDef TIM_ICUserValueStructure_TACH1 ;
extern TIM_ICUserValueTypeDef TIM_ICUserValueStructure_TACH2 ;
extern TIM_ICUserValueTypeDef TIM_ICUserValueStructure_TACH3 ;
extern TIM_ICUserValueTypeDef TIM_ICUserValueStructure_TACH4 ;

void PWM_Configuration(void);
void pwm_input_Config(uint16_t channel);
void time_callback(void);
void pwm_input_callback(void);
void pwm_gpio_disable(uint16_t channel) ;
void pwm_func(void);

#endif /* __GENERAL_TIM_H */
