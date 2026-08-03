#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bsp_usart.h"


static uint32_t USARTx_CLK[COMn] = { USART1_CLK , USART5_CLK, USART6_CLK};
static GPIO_TypeDef* USARTx_RX_PORT[COMn] = {USART1_RX_GPIO_PORT , USART5_RX_GPIO_PORT , USART6_RX_GPIO_PORT};
static uint32_t USARTx_RX_GPIO_CLK[COMn] = {USART1_RX_GPIO_CLK , USART5_RX_GPIO_CLK , USART6_RX_GPIO_CLK};
static uint32_t USARTx_RX_PIN[COMn] = {USART1_RX_PIN , USART5_RX_PIN , USART6_RX_PIN};
static uint32_t USARTx_RX_AF[COMn] = {USART1_RX_AF , USART5_RX_AF	, USART6_RX_AF};
static uint8_t USARTx_RX_SOURCE[COMn] =	{USART1_RX_SOURCE , USART5_RX_SOURCE, USART6_RX_SOURCE};
static GPIO_TypeDef* USARTx_TX_PORT[COMn] = {USART1_TX_GPIO_PORT , USART5_TX_GPIO_PORT , USART6_TX_GPIO_PORT};
static uint32_t USARTx_TX_GPIO_CLK[COMn] = {USART1_TX_GPIO_CLK , USART5_TX_GPIO_CLK , USART6_TX_GPIO_CLK};
static uint32_t USARTx_TX_PIN[COMn] = {USART1_TX_PIN , USART5_TX_PIN , USART6_TX_PIN};
static uint32_t USARTx_TX_AF[COMn] = {USART1_TX_AF , USART5_TX_AF	, USART6_TX_AF};
static uint8_t USARTx_TX_SOURCE[COMn] =	{ USART1_TX_SOURCE , USART5_TX_SOURCE, USART6_TX_SOURCE};

stUSART_RECV_DATA_t   stUsart1_recv_Data;
stUSART_RECV_DATA_t   stUsart5_recv_Data;
stUSART_RECV_DATA_t   stUsart6_recv_Data;																								


xSemaphoreHandle xUsart1TxMutex = NULL;

/*
*********************************************************************************************************
*	�� �� ��: USART_Config
*	����˵��: ���ڳ�ʼ��
*	��    �Σ�usartx�˿ڣ�USARTx_Baudrate������
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void USART_Config(USART_TypeDef * usartx ,uint32_t USARTx_Baudrate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	uint8_t COM_ID = 0;
	  
    if( COM_USART1 == usartx )
       COM_ID = 0U;		
		else if( COM_USART5 == usartx )
       COM_ID = 1U;
		else if( COM_USART6 == usartx )
       COM_ID = 2U;
		
	RCC_AHB1PeriphClockCmd(USARTx_RX_GPIO_CLK[COM_ID] | USARTx_TX_GPIO_CLK[COM_ID] , ENABLE);//GPIOʱ��ʹ��
	if((COM_USART1 == usartx)||( COM_USART6 == usartx ))
	{
		RCC_APB2PeriphClockCmd(USARTx_CLK[COM_ID], ENABLE);	//usart1,usart6����ʱ��ʹ��
	}
	else
	{
		RCC_APB1PeriphClockCmd(USARTx_CLK[COM_ID], ENABLE);//usart2,usart3,uart4,uart5����ʱ��ʹ��
	}
	
	/* GPIO��ʼ�� */
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_UP;  
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Pin = USARTx_TX_PIN[COM_ID]  ;  
	GPIO_Init(USARTx_TX_PORT[COM_ID], &GPIO_InitStructure);//����Tx����Ϊ���ù��� 

	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_Pin = USARTx_RX_PIN[COM_ID];
	GPIO_Init(USARTx_RX_PORT[COM_ID] , &GPIO_InitStructure);// ����Rx����Ϊ���ù��� 

	GPIO_PinAFConfig(USARTx_RX_PORT[COM_ID] , USARTx_RX_SOURCE[COM_ID] , USARTx_RX_AF[COM_ID]);// ���� PXx �� USARTx_Tx
	GPIO_PinAFConfig(USARTx_TX_PORT[COM_ID] , USARTx_TX_SOURCE[COM_ID] , USARTx_TX_AF[COM_ID]);// ���� PXx �� USARTx__Rx


	USART_InitStructure.USART_BaudRate = USARTx_Baudrate;//����������
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;// �ֳ�(����λ+У��λ)
	USART_InitStructure.USART_StopBits = USART_StopBits_1;// ֹͣλ��1��ֹͣλ
	USART_InitStructure.USART_Parity = USART_Parity_No;// У��λѡ�񣺲�ʹ��У�� 
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;//Ӳ�������ƣ���ʹ��Ӳ���� 
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;//USARTģʽ���ƣ�ͬʱʹ�ܽ��պͷ��� 
	USART_Init(usartx, &USART_InitStructure);//���USART��ʼ������ 

	USART_ITConfig(usartx, USART_IT_RXNE, ENABLE);// ʹ�ܴ��ڽ����ж� 
	USART_ITConfig(usartx, USART_IT_IDLE, ENABLE);// ʹ�ܴ��ڿ����ж� 
	USART_Cmd(usartx, ENABLE);// ʹ�ܴ��� 
}

/*																						
*********************************************************************************************************
*	�� �� ��: uart_callback
*	����˵��: �����жϻص�����
*	��    �Σ�usart_periph���ں�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void uart_callback(USART_TypeDef* usart_periph)
{
	stUSART_RECV_DATA_t *pstUart_Recv;
	uint8_t clear;
	if(usart_periph == COM_USART1)
		pstUart_Recv	=	&stUsart1_recv_Data;
	else if(usart_periph == COM_USART5)
		pstUart_Recv	=	&stUsart5_recv_Data;
	else if(usart_periph == COM_USART6)
		pstUart_Recv	=	&stUsart6_recv_Data;
	
	/* Rx */
	if(USART_GetITStatus(usart_periph,USART_IT_RXNE)!=RESET)
	{
		pstUart_Recv->recv_buf[pstUart_Recv->recv_data_len++] = USART_ReceiveData(  usart_periph );
	}
	if(USART_GetITStatus(usart_periph,USART_IT_IDLE)!=RESET)
	{
		clear =usart_periph->DR;//�ȶ�ȡ���ջ���������
		USART_ClearFlag(usart_periph, USART_IT_IDLE);  //��������жϱ�־λ
		//pstUart_Recv->recv_data_len = 0;//��������
		xUsartSemaphoreGive(usart_periph);
	}
	
#if 0	
	/* Tx */
	if((RESET != usart_flag_get(usart_periph, USART_FLAG_TBE))&&
		 (RESET != usart_interrupt_flag_get(usart_periph, USART_INT_FLAG_TBE))){
			/* Write one byte to the transmit data register */
			usart_data_transmit(usart_periph, tx_buffer[tx_counter++]);

			if(tx_counter >= nbr_data_to_send){
					/* disable the UART7 transmit interrupt */
					usart_interrupt_disable(usart_periph, USART_INT_TBE);
			}
	}
#endif
	
//	if(RESET != usart_interrupt_flag_get(usart_periph, USART_INT_FLAG_RBNE_ORERR)){
//			usart_data_receive(usart_periph);
//	}
//	if(RESET != usart_interrupt_flag_get(usart_periph, USART_INT_FLAG_ERR_ORERR)){
//			usart_data_receive(usart_periph);
//	}
//	if(RESET != usart_interrupt_flag_get(usart_periph, USART_INT_FLAG_ERR_NERR)){
//			usart_data_receive(usart_periph);
//	}
//	if(RESET != usart_interrupt_flag_get(usart_periph, USART_INT_FLAG_ERR_FERR)){
//			usart_data_receive(usart_periph);
//	}	
}

/*																						
*********************************************************************************************************
*	�� �� ��: xUsartSemaphoreGive
*	����˵��: ���ڽ����жϣ�����Ӧ�ӿڸ���ֵ��
*	��    �Σ�usart_periph
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void xUsartSemaphoreGive(USART_TypeDef* usart_periph)
{
	BaseType_t  xHigherPriorityTaskWoken;	

	if(usart_periph == COM_USART1)
		xSemaphoreGiveFromISR(xUsart1Semaphore, &xHigherPriorityTaskWoken);
	else if(usart_periph == COM_USART5)
		xSemaphoreGiveFromISR(xUsart5Semaphore, &xHigherPriorityTaskWoken);
	else if(usart_periph == COM_USART6)
		xSemaphoreGiveFromISR(xUsart6Semaphore, &xHigherPriorityTaskWoken);
	
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);	
}


/*																						
*********************************************************************************************************
*	�� �� ��: fputc
*	����˵��: �ض���c�⺯��printf�����ڣ��ض�����ʹ��printf����
*	��    �Σ�int ch, FILE *f
*	�� �� ֵ: ��
*********************************************************************************************************
*/
int fputc(int ch, FILE *f)
{
		USART_SendData(COM_USART1, (uint8_t) ch);
		while (USART_GetFlagStatus(COM_USART1, USART_FLAG_TXE) == RESET);
		return (ch);
}
/*																						
*********************************************************************************************************
*	�� �� ��: Usart_SendByte
*	����˵��: ����һ���ֽ�
*	��    �Σ�USART_TypeDef * pUSARTx, uint8_t ch
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch)
{
	/* ����һ���ֽ����ݵ�USART */
	USART_SendData(pUSARTx,ch);
		
	/* �ȴ��������ݼĴ���Ϊ�� */
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}
/*																						
*********************************************************************************************************
*	�� �� ��: Usart_SendString
*	����˵��: �����ַ���
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void Usart_SendString( USART_TypeDef * pUSARTx, char *str)
{
	unsigned int k=0;
	int need_unlock = 0;

	if (pUSARTx == COM_USART1 && xUsart1TxMutex) {
		if (xSemaphoreTake(xUsart1TxMutex, (TickType_t)200) != pdTRUE)
			return;
		need_unlock = 1;
	}

	do
	{
			Usart_SendByte( pUSARTx, *(str + k) );
			k++;
	} while(*(str + k)!='\0');

	/* Wait for TX complete */
	while(USART_GetFlagStatus(pUSARTx,USART_FLAG_TC)==RESET)
	{}
	if (need_unlock)
		xSemaphoreGive(xUsart1TxMutex);
}
/*																						
*********************************************************************************************************
*	�� �� ��: USART1_IRQHandler
*	����˵��: USART1�ж�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void USART1_IRQHandler(void) 
{
  uart_callback( USART1 );
}

/*																						
*********************************************************************************************************
*	�� �� ��: USART2_IRQHandler
*	����˵��: USART2�ж�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void USART2_IRQHandler(void) 
{
  uart_callback( USART2 );
}

/*																						
*********************************************************************************************************
*	�� �� ��: USART3_IRQHandler
*	����˵��: USART3�ж�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void USART3_IRQHandler(void) 
{
  uart_callback( USART3 );
}

/*																						
*********************************************************************************************************
*	�� �� ��: USART4_IRQHandler
*	����˵��: USART4�ж�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void UART4_IRQHandler(void) 
{
  uart_callback( UART4 );
}

/*																						
*********************************************************************************************************
*	�� �� ��: USART5_IRQHandler
*	����˵��: USART5�ж�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void UART5_IRQHandler(void) 
{
  uart_callback( UART5 );
}

/*																						
*********************************************************************************************************
*	�� �� ��: USART6_IRQHandler
*	����˵��: USART6�ж�
*	��    �Σ���
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void USART6_IRQHandler(void) 
{
  uart_callback( USART6 );
}



