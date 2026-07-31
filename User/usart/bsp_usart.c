#include "bsp_usart.h"
#include "bsp_init.h"

static uint32_t USARTx_CLK[COMn]						=	{ DEBUG_USART_CLK,  					COM_USART5_CLK					};
static GPIO_TypeDef* USARTx_RX_PORT[COMn]   = { DEBUG_USART_RX_GPIO_PORT ,  COM_USART5_RX_GPIO_PORT };
static uint32_t USARTx_RX_GPIO_CLK[COMn] 		=	{	DEBUG_USART_GPIO_CLK , 	    COM_USART5_GPIO_CLK 		};
static uint32_t USARTx_RX_PIN[COMn] 	 			= { DEBUG_USART_RX_GPIO_PIN , 	COM_USART5_RX_GPIO_PIN  };
static GPIO_TypeDef* USARTx_TX_PORT[COMn]   = { DEBUG_USART_TX_GPIO_PORT ,  COM_USART5_TX_GPIO_PORT	};
static uint32_t USARTx_TX_PIN[COMn] 	 			= { DEBUG_USART_TX_GPIO_PIN , 	COM_USART5_TX_GPIO_PIN  };

stUSART_RECV_DATA_t   stUart4_recv_Data;
stUSART_RECV_DATA_t   stUart5_recv_Data;



/*																						
*********************************************************************************************************
*	函 数 名: USART_Config
*	功能说明: 
*	形    参：USART_TypeDef * usartx ,uint32_t USARTx_Baudrate
*	返 回 值: 无
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

	// 将USART Tx的GPIO配置为推挽复用模式
	GPIO_InitStructure.GPIO_Pin = USARTx_TX_PIN[COM_ID];
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(USARTx_TX_PORT[COM_ID], &GPIO_InitStructure);

  // 将USART Rx的GPIO配置为浮空输入模式
	GPIO_InitStructure.GPIO_Pin = USARTx_RX_PIN[COM_ID];
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(USARTx_RX_PORT[COM_ID], &GPIO_InitStructure);
	
	// 配置串口的工作参数
	USART_InitStructure.USART_BaudRate = DEBUG_USART_BAUDRATE;	// 配置波特率
	USART_InitStructure.USART_WordLength = USART_WordLength_8b;	// 配置 针数据字长
	USART_InitStructure.USART_StopBits = USART_StopBits_1;	// 配置停止位
	USART_InitStructure.USART_Parity = USART_Parity_No ;	// 配置校验位
	USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;	// 配置硬件流控制
	USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;	// 配置工作模式，收发一起
	USART_Init(usartx, &USART_InitStructure);		// 完成串口的初始化配置
	
		// 使能串口接收中断
	USART_ITConfig(usartx, USART_IT_RXNE, ENABLE);	
	USART_ITConfig ( usartx, USART_IT_IDLE, ENABLE ); //使能串口总线空闲中断 	
	
	USART_Cmd(usartx, ENABLE);	 	// 使能串口   
}

/*																						
*********************************************************************************************************
*	函 数 名: Usart_SendByte
*	功能说明: 发送一个字节
*	形    参：USART_TypeDef * pUSARTx, uint8_t ch
*	返 回 值: 无
*********************************************************************************************************
*/
void Usart_SendByte( USART_TypeDef * pUSARTx, uint8_t ch)
{
	/* 发送一个字节数据到USART */
	USART_SendData(pUSARTx,ch);
		
	/* 等待发送数据寄存器为空 */
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}

/*																						
*********************************************************************************************************
*	函 数 名: Usart_SendArray
*	功能说明: 发送8位的数组
*	形    参：
*	返 回 值: 无
*********************************************************************************************************
*/
void Usart_SendArray( USART_TypeDef * pUSARTx, uint8_t *array, uint16_t num)
{
  uint8_t i;
	
	for(i=0; i<num; i++)
  {
	    /* 发送一个字节数据到USART */
	    Usart_SendByte(pUSARTx,array[i]);	
  
  }
	/* 等待发送完成 */
	while(USART_GetFlagStatus(pUSARTx,USART_FLAG_TC)==RESET);
}


/*																						
*********************************************************************************************************
*	函 数 名: Usart_SendString
*	功能说明: 发送字符串
*	形    参：无
*	返 回 值: 无
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
  
  /* 等待发送完成 */
  while(USART_GetFlagStatus(pUSARTx,USART_FLAG_TC)==RESET)
  {}
}

/*																						
*********************************************************************************************************
*	函 数 名: Usart_SendHalfWord
*	功能说明: 
*	形    参：USART_TypeDef * pUSARTx, uint16_t ch
*	返 回 值: 无
*********************************************************************************************************
*/
void Usart_SendHalfWord( USART_TypeDef * pUSARTx, uint16_t ch)
{
	uint8_t temp_h, temp_l;
	
	/* 取出高八位 */
	temp_h = (ch&0XFF00)>>8;
	/* 取出低八位 */
	temp_l = ch&0XFF;
	
	/* 发送高八位 */
	USART_SendData(pUSARTx,temp_h);	
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);
	
	/* 发送低八位 */
	USART_SendData(pUSARTx,temp_l);	
	while (USART_GetFlagStatus(pUSARTx, USART_FLAG_TXE) == RESET);	
}

/*																						
*********************************************************************************************************
*	函 数 名: fputc
*	功能说明: printf重定义
*	形    参：int ch, FILE *f
*	返 回 值: 无
*********************************************************************************************************
*/
int fputc(int ch, FILE *f)
{
		USART_SendData(DEBUG_USARTx, (uint8_t) ch);		/* 发送一个字节数据到串口 */
		while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_TXE) == RESET);				/* 等待发送完毕 */	
		return (ch);
}


/*																						
*********************************************************************************************************
*	函 数 名: fgetc
*	功能说明: 重定向c库函数scanf到串口，重写向后可使用scanf、getchar等函数
*	形    参：int ch, FILE *f
*	返 回 值: 无
*********************************************************************************************************
*/
int fgetc(FILE *f)
{
		/* 等待串口输入数据 */
		while (USART_GetFlagStatus(DEBUG_USARTx, USART_FLAG_RXNE) == RESET);
		return (int)USART_ReceiveData(DEBUG_USARTx);
}

/*																						
*********************************************************************************************************
*	函 数 名: uart_callback
*	功能说明: uart1，uart5串口回调函数
*	形    参：USART_TypeDef * usartx ,uint32_t USARTx_Baudrate
*	返 回 值: 无
*********************************************************************************************************
*/
void uart_callback(USART_TypeDef* usart_periph)
{
		stUSART_RECV_DATA_t *pstUart_Recv;
		uint8_t clear;

		if(usart_periph == DEBUG_USARTx)
			pstUart_Recv	=	&stUart4_recv_Data;
		else if(usart_periph == COM_USART5)
			pstUart_Recv	=	&stUart5_recv_Data;
		
		/* Rx */
		if(USART_GetITStatus(usart_periph,USART_IT_RXNE)!=RESET)
		{
				pstUart_Recv->recv_buf[pstUart_Recv->recv_data_len++] = USART_ReceiveData( usart_periph );
		}
		if(USART_GetITStatus(usart_periph,USART_IT_IDLE)!=RESET)
		{
				clear =usart_periph->DR;  										 //先读取接收缓存中数据
				USART_ClearFlag(usart_periph, USART_IT_IDLE);  //清除空闲中断标志位
				xUsartSemaphoreGive(usart_periph);
		}
}

/*
*********************************************************************************************************
** name   : xUsartSemaphoreGive
** input  : 二值量赋值
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
*	函 数 名: USART5_IRQHandler
*	功能说明: 串口5中断
*	形    参：int ch, FILE *f
*	返 回 值: 无
*********************************************************************************************************
*/
void UART5_IRQHandler(void) 
{
  uart_callback( UART5 );
}

/*																						
*********************************************************************************************************
*	函 数 名: USART4_IRQHandler
*	功能说明: 串口5中断
*	形    参：int ch, FILE *f
*	返 回 值: 无
*********************************************************************************************************
*/
void UART4_IRQHandler(void) 
{
  uart_callback( UART4 );
}

