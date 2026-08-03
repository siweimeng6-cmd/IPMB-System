#ifndef __BSP_GPIO_H
#define	__BSP_GPIO_H

#include "bsp_init.h"
/**********************************output***********************************************/

#define MCU_PWR_CTL_GPIO_PORT    	   	 GPIOA
#define MCU_PWR_CTL_GPIO_PIN			 		 GPIO_Pin_4

/* Source-IPMB compatible power-control names. */
#define PWR_CTL_GPIO_PORT MCU_PWR_CTL_GPIO_PORT
#define PWR_CTL_GPIO_PIN  MCU_PWR_CTL_GPIO_PIN

#define SOFT_POWER_OFF_GPIO_PORT    	 GPIOA
#define SOFT_POWER_OFF_GPIO_PIN			 	 GPIO_Pin_5

#define IPMBA_EN_GPIO_PORT    	 			 GPIOA
#define IPMBA_EN_GPIO_PIN			 	 			 GPIO_Pin_2

#define IPMBB_EN_GPIO_PORT    	 			 GPIOA
#define IPMBB_EN_GPIO_PIN			 	 			 GPIO_Pin_3

//GA
#define MCU_GA0_GPIO_PORT    	   			 GPIOA
#define MCU_GA0_GPIO_PIN			 			   GPIO_Pin_8

#define MCU_GA1_GPIO_PORT    	   			 GPIOA
#define MCU_GA1_GPIO_PIN			 			   GPIO_Pin_9

#define MCU_GA2_GPIO_PORT    	   			 GPIOA
#define MCU_GA2_GPIO_PIN			 			   GPIO_Pin_10

#define MCU_GA3_GPIO_PORT    	   			 GPIOA
#define MCU_GA3_GPIO_PIN			 			   GPIO_Pin_11

#define MCU_GA4_GPIO_PORT    	   			 GPIOC
#define MCU_GA4_GPIO_PIN			 			   GPIO_Pin_6

#define MCU_GAP_GPIO_PORT    	   			 GPIOC
#define MCU_GAP_GPIO_PIN			 			   GPIO_Pin_7

/* Source IPMB module names; keep them mapped to this project's CPEX pins. */
#define GA0_GPIO_PORT MCU_GA0_GPIO_PORT
#define GA0_GPIO_PIN  MCU_GA0_GPIO_PIN
#define GA1_GPIO_PORT MCU_GA1_GPIO_PORT
#define GA1_GPIO_PIN  MCU_GA1_GPIO_PIN
#define GA2_GPIO_PORT MCU_GA2_GPIO_PORT
#define GA2_GPIO_PIN  MCU_GA2_GPIO_PIN
#define GA3_GPIO_PORT MCU_GA3_GPIO_PORT
#define GA3_GPIO_PIN  MCU_GA3_GPIO_PIN
#define GA4_GPIO_PORT MCU_GA4_GPIO_PORT
#define GA4_GPIO_PIN  MCU_GA4_GPIO_PIN
#define GAP_GPIO_PORT MCU_GAP_GPIO_PORT
#define GAP_GPIO_PIN  MCU_GAP_GPIO_PIN

//
#define SELF_RST_GPIO_PORT    	   	 	 GPIOA
#define SELF_RST_GPIO_PIN			 		 		 GPIO_Pin_0

#define MCU_OUT_SYSRESET_GPIO_PORT     GPIOA
#define MCU_OUT_SYSRESET_GPIO_PIN			 GPIO_Pin_12

#define MCU_STATUS_LED_GPIO_PORT     	 GPIOA
#define MCU_STATUS_LED_GPIO_PIN			   GPIO_Pin_15

#define PAYLD_SYSPOWER_GPIO_PORT     	 GPIOB
#define PAYLD_SYSPOWER_GPIO_PIN			   GPIO_Pin_5

void payld_power_gpio_init(void);
void Power_On(void);
void Power_Off(void);
void MCU_Reset_Pulse(void);
void Power_Ctrl_Task(void* parameter);

extern TaskHandle_t Power_Ctrl_Task_Handle;
extern uint8_t mainboard_power_state;   // 0=off, 1=on
extern uint8_t power_state_confirmed;   // 0=尚未确认(MCU刚上电，PB5防抖/IPMB均未更新过，不可信) 1=已确认

/* IPMB Set FRU Activation(cmd 0x0C) 复位子命令(0x02/0x04)请求的异步复位脉冲标志:
 * 置位后由 Power_Ctrl_Task 消费并调用 MCU_Reset_Pulse(), 避免在 IPMB 从机任务里
 * 同步阻塞(MCU_Reset_Pulse 内部 vTaskDelay(100))导致主机读响应超时 */
extern volatile uint8_t ipmb_reset_flag;

/* 卡槽地址(GA0..GA4+GAP)读数, 由 slave_addr_detect() 填充,
 * IPMB 从机地址(0x30+(slot<<1))和 Get Slot 命令(0x15)都用它 */
extern int GA_VALUE;


//MCU RACK
#define MCU_RACK_ID4_GPIO_PORT     	 	 GPIOB
#define MCU_RACK_ID4_GPIO_PIN			     GPIO_Pin_3

#define MCU_RACK_ID5_GPIO_PORT     	 	 GPIOB
#define MCU_RACK_ID5_GPIO_PIN			     GPIO_Pin_4

#define MCU_RACK_ID0_GPIO_PORT     	 	 GPIOB
#define MCU_RACK_ID0_GPIO_PIN			     GPIO_Pin_12

#define MCU_RACK_ID1_GPIO_PORT     	 	 GPIOB
#define MCU_RACK_ID1_GPIO_PIN			     GPIO_Pin_13

#define MCU_RACK_ID2_GPIO_PORT     	 	 GPIOB
#define MCU_RACK_ID2_GPIO_PIN			     GPIO_Pin_14

#define MCU_RACK_ID3_GPIO_PORT     	 	 GPIOB
#define MCU_RACK_ID3_GPIO_PIN			     GPIO_Pin_15

//IPMB EN 
#define IPMB_A_EN_GPIO_PORT     	 	 	 GPIOA
#define IPMB_A_EN_GPIO_PIN			     	 GPIO_Pin_2

#define IPMB_B_EN_GPIO_PORT     	 	 	 GPIOA
#define IPMB_B_EN_GPIO_PIN			     	 GPIO_Pin_3

void slave_addr_detect(void);
void gpio_init(void);

#endif 


