#ifndef __USART_H
#define	__USART_H


#include "stm32f10x.h"
#include <stdio.h>


#define  DEBUG_USARTx                   UART4
#define  DEBUG_USART_CLK                RCC_APB1Periph_UART4
#define  DEBUG_USART_APBxClkCmd         RCC_APB1PeriphClockCmd
#define  DEBUG_USART_BAUDRATE           115200

#define  DEBUG_USART_GPIO_CLK           (RCC_APB2Periph_GPIOC)
#define  DEBUG_USART_GPIO_APBxClkCmd    RCC_APB2PeriphClockCmd
    
#define  DEBUG_USART_TX_GPIO_PORT       GPIOC   
#define  DEBUG_USART_TX_GPIO_PIN        GPIO_Pin_10
#define  DEBUG_USART_RX_GPIO_PORT       GPIOC
#define  DEBUG_USART_RX_GPIO_PIN        GPIO_Pin_11

#define  DEBUG_USART_IRQ                UART4_IRQn
#define  DEBUG_USART_IRQHandler         UART4_IRQHandler

// �ⲿ��������
extern uint8_t softoff_flag;
extern uint8_t poweron_flag;
extern uint8_t poweroff_flag;
extern uint8_t powerrst_flag;


void USART_Config(void);
void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch);
void Usart_SendString( USART_TypeDef * pUSARTx, char *str);
void Usart_SendHalfWord( USART_TypeDef * pUSARTx, uint16_t ch);
void USART5_SendData(uint8_t *data, uint16_t len);

/* 诊断计数器 (UART4 ISR 更新, 无 printf 依赖) */
extern volatile uint32_t g_uart4_rx_byte_cnt;
extern volatile uint32_t g_uart4_isr_cnt;
extern volatile uint32_t g_uart4_line_cnt;

#endif /* __USART_H */