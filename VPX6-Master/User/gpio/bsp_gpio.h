#ifndef __BSP_GPIO_H
#define	__BSP_GPIO_H

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include <stdbool.h>
#include <stdio.h>

/* GPIO  */
typedef enum
{
	GPIO_CMD_NONE = 0,
	GPIO_CMD_POWER_ON_BMC_CPLD_PWR_BTN,     /* 300ms低脉冲 -> power on */
	GPIO_CMD_POWER_OFF_BMC_CPLD_PWR_BTN,    /* 5s低脉冲   -> power off/关机 */
	GPIO_CMD_RESET_BMC_OUT_SYS_RST,
	GPIO_CMD_PWRBTN_SHORT_PRESS,           /* R15 PB15 低脉冲 <2s  -> 开机(延时20ms后拉高R14) */
	GPIO_CMD_PWRBTN_LONG_PRESS,            /* R15 PB15 低脉冲 >=2s -> 强制关机(延时20ms后拉低R14) */
	GPIO_CMD_SOFT_PWRDN_FROM_CPLD          /* N11 PE13 下降沿      -> 软关机(延时20ms后拉低R14) */
}GPIO_Cmd_t;

extern xQueueHandle xGpioCmdQueue;
extern TaskHandle_t GPIO_Task_Handle;

/************�˿����Ŷ����λ�忨��ַ*************************************************/
#define  GPIO_GA_CLK             		RCC_AHB1Periph_GPIOD
#define  GPIO_GA0_PORT  						GPIOD 
#define  GPIO_GA0_PIN   						GPIO_Pin_11

#define  GPIO_GA1_PORT  						GPIOD
#define  GPIO_GA1_PIN  							GPIO_Pin_12

#define  GPIO_GA2_PORT  						GPIOD
#define  GPIO_GA2_PIN   						GPIO_Pin_13

#define  GPIO_GA3_PORT  						GPIOD
#define  GPIO_GA3_PIN   						GPIO_Pin_14

#define  GPIO_GA4_PORT  						GPIOD
#define  GPIO_GA4_PIN   						GPIO_Pin_15

#define  GPIO_GAP_PORT 							GPIOD
#define  GPIO_GAP_PIN  							GPIO_Pin_10


#define  RACK_ID0_PORT  						GPIOD
#define  RACK_ID0_PIN   						GPIO_Pin_5
#define  RACK_ID0_CLK   						RCC_AHB1Periph_GPIOD

#define  RACK_ID1_PORT  						GPIOE
#define  RACK_ID1_PIN   						GPIO_Pin_0
#define  RACK_ID1_CLK   						RCC_AHB1Periph_GPIOE

#define  RACK_ID2_PORT  						GPIOE
#define  RACK_ID2_PIN   						GPIO_Pin_1
#define  RACK_ID2_CLK   						RCC_AHB1Periph_GPIOE

#define  RACK_ID3_PORT  						GPIOB
#define  RACK_ID3_PIN   						GPIO_Pin_12
#define  RACK_ID3_CLK   						RCC_AHB1Periph_GPIOB

#define  RACK_ID4_PORT  						GPIOB
#define  RACK_ID4_PIN   						GPIO_Pin_13
#define  RACK_ID4_CLK   						RCC_AHB1Periph_GPIOB

#define  RACK_ID5_PORT  						GPIOI
#define  RACK_ID5_PIN   						GPIO_Pin_3
#define  RACK_ID5_CLK   						RCC_AHB1Periph_GPIOI

/*************************************************************************************/
#define  BMC_OUT_SYS_RST_PORT  				GPIOE
#define  BMC_OUT_SYS_RST_PIN   				GPIO_Pin_2
#define  BMC_OUT_SYS_RST_CLK   				RCC_AHB1Periph_GPIOE

#define  VPX_PWR_BTN_PORT  						GPIOE
#define  VPX_PWR_BTN_PIN   						GPIO_PIN_3

#define  BMC_CPLD_PWR_BTN_PORT  			GPIOA
#define  BMC_CPLD_PWR_BTN_PIN   			GPIO_Pin_12
#define  BMC_CPLD_PWR_BTN_CLK   			RCC_AHB1Periph_GPIOA

#define  CPLD_BMC_SYSPWR_GD_PORT  		GPIOE
#define  CPLD_BMC_SYSPWR_GD_PIN   		GPIO_PIN_15

#define  MASKABLE_RESET_PORT  				GPIOD
#define  MASKABLE_RESET_PIN   				GPIO_PIN_9

#define  BMC_RST_OUT_PORT  						GPIOA
#define  BMC_RST_OUT_PIN   						GPIO_PIN_15

/* PE14 : LED_BMC_ALERT# — 状态指示灯，低电平有效 */
#define  LED_BMC_ALERT_PORT  					GPIOE
#define  LED_BMC_ALERT_PIN   					GPIO_Pin_14
#define  LED_BMC_ALERT_CLK   					RCC_AHB1Periph_GPIOE

/* PH3  : BMC_IPMBA_EN  — IPMB-A 总线隔离器 NCA9511_DMSR 使能,默认拉高(使能) */
#define  BMC_IPMBA_EN_PORT   					GPIOH
#define  BMC_IPMBA_EN_PIN    					GPIO_Pin_3
#define  BMC_IPMBA_EN_CLK    					RCC_AHB1Periph_GPIOH

/* PH6  : BMC_IPMBB_EN  — IPMB-B 总线隔离器 NCA9511_DMSR 使能,默认拉高(使能) */
#define  BMC_IPMBB_EN_PORT   					GPIOH
#define  BMC_IPMBB_EN_PIN    					GPIO_Pin_6
#define  BMC_IPMBB_EN_CLK    					RCC_AHB1Periph_GPIOH

/************R14 BMC_GPIO2 / R15 BMC_GPIO3 / N11 BMC_CPLD_GPIO2 电源按钮相关引脚**************/
/* R14 BMC_GPIO2 (PB14) — 输出,默认高电平,通知电源板供电/断电 */
#define  BMC_GPIO2_PORT           GPIOB
#define  BMC_GPIO2_PIN            GPIO_Pin_14
#define  BMC_GPIO2_CLK            RCC_AHB1Periph_GPIOB

/* R15 BMC_GPIO3 (PB15) — 输入,默认上拉,低脉冲<2s开机,>=2s强制关机 */
#define  BMC_GPIO3_PORT           GPIOB
#define  BMC_GPIO3_PIN            GPIO_Pin_15
#define  BMC_GPIO3_CLK            RCC_AHB1Periph_GPIOB
#define  BMC_GPIO3_EXTI_PORTSOURCE  EXTI_PortSourceGPIOB
#define  BMC_GPIO3_EXTI_PINSOURCE   EXTI_PinSource15
#define  BMC_GPIO3_EXTI_LINE        EXTI_Line15

/* N11 BMC_CPLD_GPIO2 (PE13) — 输入,默认高电平,下降沿触发软关机 */
#define  BMC_CPLD_GPIO2_PORT      GPIOE
#define  BMC_CPLD_GPIO2_PIN       GPIO_Pin_13
#define  BMC_CPLD_GPIO2_CLK       RCC_AHB1Periph_GPIOE
#define  BMC_CPLD_GPIO2_EXTI_PORTSOURCE  EXTI_PortSourceGPIOE
#define  BMC_CPLD_GPIO2_EXTI_PINSOURCE   EXTI_PinSource13
#define  BMC_CPLD_GPIO2_EXTI_LINE        EXTI_Line13

/* PB15 和 PE13 都在 EXTI_Line10~15,共用 EXTI15_10_IRQn */

/* 电源按钮逻辑阈值(可按需调整) */
#define  BMC_GPIO3_SHORT_THRESHOLD_MS   2000   /* < 该值视为短按(开机) */
#define  BMC_GPIO_OUTPUT_DELAY_MS       20     /* 事件确认后输出 R14 的延时 */
#define  BMC_GPIO_DEBOUNCE_MS           20     /* 输入软件去抖时间 */
#define  BMC_GPIO3_LONG_FORCE_MS        2200   /* 超过该值仍在按:强制报一次长按,避免一直按住不释放导致卡死 */

void slave_addr_detect(void);
void print_slot_addr(void);
void GPIO_OUTPUT_Config(void);
void BMC_CPLD_PWR_BTN_Control(void);            /* 5s低脉冲 -> 关机(保持兼容) */
void BMC_CPLD_PWR_BTN_PowerOn(void);            /* 300ms低脉冲 -> 开机 */
void BMC_CPLD_PWR_BTN_PowerOff(void);           /* 5s低脉冲 -> 关机 */
void BMC_OUT_SYS_RST_Reset(void);
void GPIO_Task(void* parameter);
void GPIO_INPUT_Config(void);
void print_rack_addr(void);
void IPMB_Bus_Enable_Init(void);

/* 初始化 R14/R15/N11 + EXTI + TIM6 1ms 节拍,在 BSP_Init() 中调用 */
void GPIO_Power_Config(void);

/* 中断转发入口(由 stm32f4xx_it.c 调用) */
void PowerButton_EXTI_IRQHandler(void);
void PowerButton_TIM6_IRQHandler(void);
#endif