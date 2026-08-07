#include "bsp_usart.h"
#include "bsp_init.h"

static uint32_t USARTx_CLK[COMn]						=	{ DEBUG_USART_CLK,  					COM_USART5_CLK					};
static GPIO_TypeDef* USARTx_RX_PORT[COMn]   = { DEBUG_USART_RX_GPIO_PORT ,  COM_USART5_RX_GPIO_PORT };
static uint32_t USARTx_RX_GPIO_CLK[COMn] 		=	{	DEBUG_USART_GPIO_CLK , 	    COM_USART5_GPIO_CLK 		};
static uint32_t USARTx_RX_PIN[COMn] 	 			= { DEBUG_USART_RX_GPIO_PIN , 	COM_USART5_RX_GPIO_PIN  };
static GPIO_TypeDef* USARTx_TX_PORT[COMn]   = { DEBUG_USART_TX_GPIO_PORT ,  COM_USART5_TX_GPIO_PORT	};
static uint32_t USARTx_TX_PIN[COMn] 	 			= { DEBUG_USART_TX_GPIO_PIN , 	COM_USART5_TX_GPIO_PIN  };

stUSART_RECV_DATA_t    stUart4_recv_Data;
stUSART5_RECV_DATA_t   stUart5_recv_Data;



/*																						
*********************************************************************************************************
*	�� �� ��: USART_Config
*	����˵��: 
*	��    �Σ�USART_TypeDef * usartx ,uint32_t USARTx_Baudrate
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void USART_Config(USART_TypeDef * usartx ,uint32_t USARTx_Baudrate)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	USART_InitTypeDef USART_InitStructure;
	uint8_t COM_ID = 0;
	
	if( DEBUG_USARTx == usartx )
	{
       COM_ID = 0U;
	}
	else if( COM_USART5 == usartx )
	{
       COM_ID = 1U;
	}

	DEBUG_USART_GPIO_APBxClkCmd(USARTx_RX_GPIO_CLK[COM_ID], ENABLE);
	DEBUG_USART_APBxClkCmd(USARTx_CLK[COM_ID], ENABLE);

	// ��USART Tx��GPIO����Ϊ���츴��ģʽ
	GPIO_InitStructure.GPIO_Pin = USARTx_TX_PIN[COM_ID];
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(USARTx_TX_PORT[COM_ID], &GPIO_InitStructure);

  // ��USART Rx��GPIO����Ϊ��������ģʽ
	GPIO_InitStructure.GPIO_Pin = USARTx_RX_PIN[COM_ID];
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(USARTx_RX_PORT[COM_ID], &GPIO_InitStructure);
	
	// ���ô��ڵĹ�������
	USART_InitStructure.USART_BaudRate = DEBUG_USART_BAUDRATE;	// ���ò�����
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;	// ���� �������ֳ�
	USART_InitStructure.USART_StopBits = USART_StopBits_1;	// ����ֹͣλ
	USART_InitStructure.USART_Parity = USART_Parity_No ;	// ����У��λ
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	// ����Ӳ��������
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	// ���ù���ģʽ���շ�һ��
	USART_Init(usartx, &USART_InitStructure);		// ��ɴ��ڵĳ�ʼ������
	
		// ʹ�ܴ��ڽ����ж�
	USART_ITConfig(usartx, USART_IT_RXNE, ENABLE);	
	USART_ITConfig ( usartx, USART_IT_IDLE, ENABLE ); //ʹ�ܴ������߿����ж� 	
	
	USART_Cmd(usartx, ENABLE);	 	// ʹ�ܴ���   
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
*	�� �� ��: Usart_SendArray
*	����˵��: ����8λ������
*	��    �Σ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void Usart_SendArray( USART_TypeDef * pUSARTx, uint8_t *array, uint16_t num)
{
  uint16_t i;

	for(i=0; i<num; i++)
  {
	    /* ����һ���ֽ����ݵ�USART */
	    Usart_SendByte(pUSARTx,array[i]);	
  
  }
	/* �ȴ�������� */
	while(USART_GetFlagStatus(pUSARTx,USART_FLAG_TC)==RESET);
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
  do 
  {
      Usart_SendByte( pUSARTx, *(str + k) );
      k++;
  } while(*(str + k)!='\0');
  
  /* �ȴ�������� */
  while(USART_GetFlagStatus(pUSARTx,USART_FLAG_TC)==RESET)
  {}
}

/*																						
*********************************************************************************************************
*	�� �� ��: Usart_SendHalfWord
*	����˵��: 
*	��    �Σ�USART_TypeDef * pUSARTx, uint16_t ch
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void Usart_SendHalfWord( USART_TypeDef * pUSARTx, uint16_t ch)
{
	uint8_t temp_h, temp_l;
	
	/* ȡ���߰�λ */
	temp_h = (ch&0XFF00)>>8;
	/* ȡ���Ͱ�λ */
	temp_l = ch&0XFF;
	
	/* ���͸߰�λ */
	USART_SendData(pUSARTx,temp_h);	
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);
	
	/* ���͵Ͱ�λ */
	USART_SendData(pUSARTx,temp_l);	
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}

/*																						
*********************************************************************************************************
*	�� �� ��: fputc
*	����˵��: printf�ض���
*	��    �Σ�int ch, FILE *f
*	�� �� ֵ: ��
*********************************************************************************************************
*/
int fputc(int ch, FILE *f)
{
		USART_SendData(DEBUG_USARTx, (uint8_t) ch);		/* ����һ���ֽ����ݵ����� */
		while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_TXE) == RESET);				/* �ȴ�������� */	
		return (ch);
}


/*																						
*********************************************************************************************************
*	�� �� ��: fgetc
*	����˵��: �ض���c�⺯��scanf�����ڣ���д����ʹ��scanf��getchar�Ⱥ���
*	��    �Σ�int ch, FILE *f
*	�� �� ֵ: ��
*********************************************************************************************************
*/
int fgetc(FILE *f)
{
		/* �ȴ������������� */
		while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_RXNE) == RESET);
		return (int)USART_ReceiveData(DEBUG_USARTx);
}

/*																						
*********************************************************************************************************
*	�� �� ��: uart_callback
*	����˵��: uart1��uart5���ڻص�����
*	��    �Σ�USART_TypeDef * usartx ,uint32_t USARTx_Baudrate
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void uart_callback(USART_TypeDef* usart_periph)
{
		uint8_t clear;

		/* Rx: UART4/UART5 缓冲区大小不同(见 bsp_usart.h), 分开判断越界, 防止
		 * 单包超长时把 recv_data_len 后面的内存写坏。DR 必须每次都读, 否则
		 * RXNE 标志不清零会导致中断风暴 */
		if(USART_GetITStatus(usart_periph,USART_IT_RXNE)!=RESET)
		{
				uint8_t rx_byte = (uint8_t)USART_ReceiveData( usart_periph );

				if(usart_periph == DEBUG_USARTx)
				{
						if(stUart4_recv_Data.recv_data_len < USART_RECV_SIZE)
								stUart4_recv_Data.recv_buf[stUart4_recv_Data.recv_data_len++] = rx_byte;
				}
				else if(usart_periph == COM_USART5)
				{
						if(stUart5_recv_Data.recv_data_len < USART5_RECV_SIZE)
								stUart5_recv_Data.recv_buf[stUart5_recv_Data.recv_data_len++] = rx_byte;
				}
		}
		if(USART_GetITStatus(usart_periph,USART_IT_IDLE)!=RESET)
		{
				clear =usart_periph->DR;  										 //�ȶ�ȡ���ջ���������
				USART_ClearFlag(usart_periph, USART_IT_IDLE);  //��������жϱ�־λ
				xUsartSemaphoreGive(usart_periph);
		}
}

/*
*********************************************************************************************************
** name   : xUsartSemaphoreGive
** input  : ��ֵ����ֵ
** output : None
** discr  : uart_callback
*********************************************************************************************************
*/
void xUsartSemaphoreGive(USART_TypeDef* usart_periph)
{
		BaseType_t  xHigherPriorityTaskWoken;	
	
		if(usart_periph == DEBUG_USARTx)
			xSemaphoreGiveFromISR(xUart4Semaphore, &xHigherPriorityTaskWoken);
		else if(usart_periph == COM_USART5)
			xSemaphoreGiveFromISR(xUart5Semaphore, &xHigherPriorityTaskWoken);

		
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);	
}
/*																						
*********************************************************************************************************
*	�� �� ��: USART5_IRQHandler
*	����˵��: ����5�ж�
*	��    �Σ�int ch, FILE *f
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void UART5_IRQHandler(void) 
{
  uart_callback( UART5 );
}

/*																						
*********************************************************************************************************
*	�� �� ��: USART4_IRQHandler
*	����˵��: ����5�ж�
*	��    �Σ�int ch, FILE *f
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void UART4_IRQHandler(void) 
{
  uart_callback( UART4 );
}

