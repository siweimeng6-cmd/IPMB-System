#ifndef __USART_H
#define	__USART_H


#include "stm32f10x.h"
#include <stdio.h>

/** 
  * 串口宏定义，不同的串口挂载的总线和IO不一样，移植时需要修改这几个宏
	* 1-修改总线时钟的宏，uart1挂载到apb2总线，其他uart挂载到apb1总线
	* 2-修改GPIO的宏
  */
	

#define COMn                              2U  //uart7 not use

 //串口4-UART4
#define  DEBUG_USARTx                   UART4
#define  DEBUG_USART_CLK                RCC_APB1Periph_UART4
#define  DEBUG_USART_APBxClkCmd         RCC_APB1PeriphClockCmd
#define  DEBUG_USART_BAUDRATE           115200

// USART GPIO 引脚宏定义
#define  DEBUG_USART_GPIO_CLK           (RCC_APB2Periph_GPIOC)
#define  DEBUG_USART_GPIO_APBxClkCmd    RCC_APB2PeriphClockCmd
    
#define  DEBUG_USART_TX_GPIO_PORT       GPIOC   
#define  DEBUG_USART_TX_GPIO_PIN        GPIO_Pin_10
#define  DEBUG_USART_RX_GPIO_PORT       GPIOC
#define  DEBUG_USART_RX_GPIO_PIN        GPIO_Pin_11

#define  DEBUG_USART_IRQ                UART4_IRQn
#define  DEBUG_USART_IRQHandler         UART4_IRQHandler

// 串口5-UART5
#define  COM_USART5                   	UART5
#define  COM_USART5_CLK                 RCC_APB1Periph_UART5
#define  COM_USART5_APBxClkCmd          RCC_APB1PeriphClockCmd
#define  COM_USART5_BAUDRATE            115200

// USART GPIO 引脚宏定义
#define  COM_USART5_GPIO_CLK            (RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOD)
#define  COM_USART5_GPIO_APBxClkCmd     RCC_APB2PeriphClockCmd
    
#define  COM_USART5_TX_GPIO_PORT        GPIOC   
#define  COM_USART5_TX_GPIO_PIN         GPIO_Pin_12
#define  COM_USART5_RX_GPIO_PORT        GPIOD
#define  COM_USART5_RX_GPIO_PIN         GPIO_Pin_2

#define  COM_USART5_IRQ                 UART5_IRQn
#define  COM_USART5_IRQHandler          UART5_IRQHandler

#define USART_RECV_SIZE                    256

#pragma pack(1)
typedef struct
{
	unsigned char  recv_buf[USART_RECV_SIZE];
	unsigned short recv_data_len;
}stUSART_RECV_DATA_t,*pstUSART_RECV_DATA_t;

extern stUSART_RECV_DATA_t   stUart4_recv_Data;
extern stUSART_RECV_DATA_t   stUart5_recv_Data;


void xUsartSemaphoreGive(USART_TypeDef* usart_periph);
void USART_Config(USART_TypeDef * usartx ,uint32_t USARTx_Baudrate);
void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch);
void Usart_SendString( USART_TypeDef * pUSARTx, char *str);
void Usart_SendHalfWord( USART_TypeDef * pUSARTx, uint16_t ch);
void UART5_Task(void* parameter);

#endif /* __USART_H */
