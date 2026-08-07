#ifndef __USART_H
#define	__USART_H


#include "stm32f10x.h"
#include <stdio.h>

/** 
  * ���ں궨�壬��ͬ�Ĵ��ڹ��ص����ߺ�IO��һ������ֲʱ��Ҫ�޸��⼸����
	* 1-�޸�����ʱ�ӵĺ꣬uart1���ص�apb2���ߣ�����uart���ص�apb1����
	* 2-�޸�GPIO�ĺ�
  */
	

#define COMn                              2U  //uart7 not use

 //����4-UART4
#define  DEBUG_USARTx                   UART4
#define  DEBUG_USART_CLK                RCC_APB1Periph_UART4
#define  DEBUG_USART_APBxClkCmd         RCC_APB1PeriphClockCmd
#define  DEBUG_USART_BAUDRATE           115200

// USART GPIO ���ź궨��
#define  DEBUG_USART_GPIO_CLK           (RCC_APB2Periph_GPIOC)
#define  DEBUG_USART_GPIO_APBxClkCmd    RCC_APB2PeriphClockCmd
    
#define  DEBUG_USART_TX_GPIO_PORT       GPIOC   
#define  DEBUG_USART_TX_GPIO_PIN        GPIO_Pin_10
#define  DEBUG_USART_RX_GPIO_PORT       GPIOC
#define  DEBUG_USART_RX_GPIO_PIN        GPIO_Pin_11

#define  DEBUG_USART_IRQ                UART4_IRQn
#define  DEBUG_USART_IRQHandler         UART4_IRQHandler

// ����5-UART5
#define  COM_USART5                   	UART5
#define  COM_USART5_CLK                 RCC_APB1Periph_UART5
#define  COM_USART5_APBxClkCmd          RCC_APB1PeriphClockCmd
#define  COM_USART5_BAUDRATE            115200

// USART GPIO ���ź궨��
#define  COM_USART5_GPIO_CLK            (RCC_APB2Periph_GPIOC|RCC_APB2Periph_GPIOD)
#define  COM_USART5_GPIO_APBxClkCmd     RCC_APB2PeriphClockCmd
    
#define  COM_USART5_TX_GPIO_PORT        GPIOC   
#define  COM_USART5_TX_GPIO_PIN         GPIO_Pin_12
#define  COM_USART5_RX_GPIO_PORT        GPIOD
#define  COM_USART5_RX_GPIO_PIN         GPIO_Pin_2

#define  COM_USART5_IRQ                 UART5_IRQn
#define  COM_USART5_IRQHandler          UART5_IRQHandler

#define USART_RECV_SIZE                    256
/* UART5(功能串口)要收固件升级 Program 分片包: 300字节原始IPMB帧经 Basic Mode
 * 转义后最坏情况翻倍(0xA0起始+0xA5结束+每个特殊字节各多1字节转义), 单独给一个
 * 够大的缓冲区, 不改 UART4(调试口) 的 256 字节, 避免影响其现有用法 */
#define USART5_RECV_SIZE                   700

#pragma pack(1)
typedef struct
{
	unsigned char  recv_buf[USART_RECV_SIZE];
	unsigned short recv_data_len;
}stUSART_RECV_DATA_t,*pstUSART_RECV_DATA_t;

typedef struct
{
	unsigned char  recv_buf[USART5_RECV_SIZE];
	unsigned short recv_data_len;
}stUSART5_RECV_DATA_t,*pstUSART5_RECV_DATA_t;

extern stUSART_RECV_DATA_t    stUart4_recv_Data;
extern stUSART5_RECV_DATA_t   stUart5_recv_Data;


void xUsartSemaphoreGive(USART_TypeDef* usart_periph);
void USART_Config(USART_TypeDef * usartx ,uint32_t USARTx_Baudrate);
void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch);
void Usart_SendArray( USART_TypeDef * pUSARTx, uint8_t *array, uint16_t num);
void Usart_SendString( USART_TypeDef * pUSARTx, char *str);
void Usart_SendHalfWord( USART_TypeDef * pUSARTx, uint16_t ch);
void UART5_Task(void* parameter);

#endif /* __USART_H */
