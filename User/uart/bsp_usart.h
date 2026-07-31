#ifndef __BSP_USART_H
#define	__BSP_USART_H

#include "stm32f4xx.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


#include "task_usart.h"




#define COMn                              	3U  //uart7 not use
#define USART1_EN							1
#define USART2_EN							0
#define USART3_EN							0
#define USART4_EN							0
#define USART5_EN							1
#define USART6_EN							1

//���Ŷ���
/***************************************************************************************************/
#if USART1_EN
#define COM_USART1                         USART1

/* ��ͬ�Ĵ��ڹ��ص����߲�һ����ʱ��ʹ�ܺ���Ҳ��һ������ֲʱҪע��
* ����1��6��      	RCC_APB2PeriphClockCmd
* ����2/3/4/5��     RCC_APB1PeriphClockCmd
*/


#define USART1_CLK                         RCC_APB2Periph_USART1
#define USART1_BAUDRATE                    115200  //���ڲ�����

#define USART1_RX_GPIO_PORT                GPIOA
#define USART1_RX_GPIO_CLK                 RCC_AHB1Periph_GPIOA
#define USART1_RX_PIN                      GPIO_Pin_10
#define USART1_RX_AF                       GPIO_AF_USART1
#define USART1_RX_SOURCE                   GPIO_PinSource10

#define USART1_TX_GPIO_PORT                GPIOA
#define USART1_TX_GPIO_CLK                 RCC_AHB1Periph_GPIOA
#define USART1_TX_PIN                      GPIO_Pin_9
#define USART1_TX_AF                       GPIO_AF_USART1
#define USART1_TX_SOURCE                   GPIO_PinSource9

#define COM_USART1_IRQHandler              USART1_IRQHandler
#define COM_USART1_IRQ                 	   USART1_IRQn
#endif
/***************************************************************************************************/
#if USART2_EN
#define COM_USART2                         USART2

#define USART2_CLK                         RCC_APB1Periph_USART2
#define USART2_BAUDRATE                    115200  //���ڲ�����

#define USART2_RX_GPIO_PORT                GPIOD
#define USART2_RX_GPIO_CLK                 RCC_AHB1Periph_GPIOD
#define USART2_RX_PIN                      GPIO_Pin_6
#define USART2_RX_AF                       GPIO_AF_USART2
#define USART2_RX_SOURCE                   GPIO_PinSource6

#define USART2_TX_GPIO_PORT                GPIOD
#define USART2_TX_GPIO_CLK                 RCC_AHB1Periph_GPIOD
#define USART2_TX_PIN                      GPIO_Pin_5
#define USART2_TX_AF                       GPIO_AF_USART2
#define USART2_TX_SOURCE                   GPIO_PinSource5

#define COM_USART2_IRQHandler              USART2_IRQHandler
#define COM_USART2_IRQ                 	   USART2_IRQn
#endif
/***************************************************************************************************/
#if USART3_EN
#define COM_USART3                         USART3

#define USART3_CLK                         RCC_APB1Periph_USART3 
#define USART3_BAUDRATE                    115200  //���ڲ�����

#define USART3_RX_GPIO_PORT                GPIOD
#define USART3_RX_GPIO_CLK                 RCC_AHB1Periph_GPIOD
#define USART3_RX_PIN                      GPIO_Pin_9
#define USART3_RX_AF                       GPIO_AF_USART3
#define USART3_RX_SOURCE                   GPIO_PinSource9

#define USART3_TX_GPIO_PORT                GPIOD
#define USART3_TX_GPIO_CLK                 RCC_AHB1Periph_GPIOD
#define USART3_TX_PIN                      GPIO_Pin_8
#define USART3_TX_AF                       GPIO_AF_USART3
#define USART3_TX_SOURCE                   GPIO_PinSource8

#define COM_USART3_IRQHandler              USART3_IRQHandler
#define COM_USART3_IRQ                 	   USART3_IRQn
#endif
/***************************************************************************************************/
#if USART4_EN
#define COM_USART4                         UART4

#define USART4_CLK                         RCC_APB1Periph_UART4 
#define USART4_BAUDRATE                    115200  //���ڲ�����

#define USART4_RX_GPIO_PORT                GPIOC
#define USART4_RX_GPIO_CLK                 RCC_AHB1Periph_GPIOC
#define USART4_RX_PIN                      GPIO_Pin_11
#define USART4_RX_AF                       GPIO_AF_UART4
#define USART4_RX_SOURCE                   GPIO_PinSource11

#define USART4_TX_GPIO_PORT                GPIOC
#define USART4_TX_GPIO_CLK                 RCC_AHB1Periph_GPIOC
#define USART4_TX_PIN                      GPIO_Pin_10
#define USART4_TX_AF                       GPIO_AF_UART4
#define USART4_TX_SOURCE                   GPIO_PinSource10

#define COM_USART4_IRQHandler              UART4_IRQHandler
#define COM_USART4_IRQ                 	   UART4_IRQn
#endif
/***************************************************************************************************/
#if USART5_EN
#define COM_USART5                         UART5

#define USART5_CLK                         RCC_APB1Periph_UART5 
#define USART5_BAUDRATE                    115200  //���ڲ�����

#define USART5_RX_GPIO_PORT                GPIOD
#define USART5_RX_GPIO_CLK                 RCC_AHB1Periph_GPIOD
#define USART5_RX_PIN                      GPIO_Pin_2
#define USART5_RX_AF                       GPIO_AF_UART5
#define USART5_RX_SOURCE                   GPIO_PinSource2

#define USART5_TX_GPIO_PORT                GPIOC
#define USART5_TX_GPIO_CLK                 RCC_AHB1Periph_GPIOC
#define USART5_TX_PIN                      GPIO_Pin_12
#define USART5_TX_AF                       GPIO_AF_UART5
#define USART5_TX_SOURCE                   GPIO_PinSource12

#define COM_USART5_IRQHandler              UART5_IRQHandler
#define COM_USART5_IRQ                 	   UART5_IRQn
#endif

/***************************************************************************************************/
#if USART6_EN
#define COM_USART6                         USART6

#define USART6_CLK                         RCC_APB2Periph_USART6
#define USART6_BAUDRATE                    115200  //���ڲ�����

#define USART6_RX_GPIO_PORT                GPIOC
#define USART6_RX_GPIO_CLK                 RCC_AHB1Periph_GPIOC
#define USART6_RX_PIN                      GPIO_Pin_7
#define USART6_RX_AF                       GPIO_AF_USART6
#define USART6_RX_SOURCE                   GPIO_PinSource7

#define USART6_TX_GPIO_PORT                GPIOC
#define USART6_TX_GPIO_CLK                 RCC_AHB1Periph_GPIOC
#define USART6_TX_PIN                      GPIO_Pin_6
#define USART6_TX_AF                       GPIO_AF_USART6
#define USART6_TX_SOURCE                   GPIO_PinSource6

#define COM_USART6_IRQHandler              USART6_IRQHandler
#define COM_USART6_IRQ                 	   USART6_IRQn
#endif
/********************************************************************************************************/
#define USART_RECV_SIZE                    128

#pragma pack(1)
typedef struct
{
	unsigned char  recv_buf[USART_RECV_SIZE];
	unsigned short recv_data_len;

	unsigned char  data_valid_flag;
}stUSART_RECV_DATA_t,*pstUSART_RECV_DATA_t;


extern stUSART_RECV_DATA_t   stUsart1_recv_Data;
extern stUSART_RECV_DATA_t   stUsart2_recv_Data;
extern stUSART_RECV_DATA_t   stUsart3_recv_Data;
extern stUSART_RECV_DATA_t   stUsart4_recv_Data;
extern stUSART_RECV_DATA_t   stUsart5_recv_Data;
extern stUSART_RECV_DATA_t   stUsart6_recv_Data;

/* USART1 TX mutex: prevents printf/Usart_SendString interleaving from concurrent tasks */
extern xSemaphoreHandle xUsart1TxMutex;		


void USART_Config(USART_TypeDef * usartx ,uint32_t USARTx_Baudrate);
void uart_callback(USART_TypeDef* usart_periph);
void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch);
void Usart_SendString( USART_TypeDef * pUSARTx, char *str);
void xUsartSemaphoreGive(USART_TypeDef* usart_periph);

void Usart_SendHalfWord( USART_TypeDef * pUSARTx, uint16_t ch);

#endif /* __USART1_H */
