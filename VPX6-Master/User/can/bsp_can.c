
#include "bsp_init.h"
#include "./can/bsp_can.h"

CanTxMsg TxMessage;			     //发送缓冲区
CanRxMsg Rx1Message;				 //接收缓冲区
CanRxMsg Rx2Message;				 //接收缓冲区


 /*																						
*********************************************************************************************************
*	函 数 名: CAN_GPIO_Config
*	功能说明: CAN的GPIO 配置
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
static void CAN_GPIO_Config(void)
{
 	GPIO_InitTypeDef GPIO_InitStructure;
   	
  /* Enable GPIO clock */
  RCC_AHB1PeriphClockCmd(CAN2_TX_GPIO_CLK|CAN2_RX_GPIO_CLK, ENABLE);
  GPIO_PinAFConfig(CAN2_TX_GPIO_PORT, CAN2_TX_SOURCE, CAN2_AF_PORT);
  GPIO_PinAFConfig(CAN2_RX_GPIO_PORT, CAN2_RX_SOURCE, CAN2_AF_PORT); 

	  /* Configure CAN TX pins */
  GPIO_InitStructure.GPIO_Pin = CAN2_TX_PIN;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
  GPIO_InitStructure.GPIO_PuPd  = GPIO_PuPd_UP;
  GPIO_Init(CAN2_TX_GPIO_PORT, &GPIO_InitStructure);
	
	/* Configure CAN RX  pins */
  GPIO_InitStructure.GPIO_Pin = CAN2_RX_PIN ;
	  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
  GPIO_Init(CAN2_RX_GPIO_PORT, &GPIO_InitStructure);
	
#if CAN1_ENABLE
	RCC_AHB1PeriphClockCmd(CAN1_TX_GPIO_CLK|CAN1_RX_GPIO_CLK, ENABLE);	
	GPIO_PinAFConfig(CAN1_TX_GPIO_PORT, CAN1_TX_SOURCE, CAN1_AF_PORT);
  GPIO_PinAFConfig(CAN1_RX_GPIO_PORT, CAN1_RX_SOURCE, CAN1_AF_PORT); 
	GPIO_InitStructure.GPIO_Pin = CAN1_RX_PIN ;
  GPIO_Init(CAN1_RX_GPIO_PORT, &GPIO_InitStructure);
	GPIO_InitStructure.GPIO_Pin = CAN1_TX_PIN ;
  GPIO_Init(CAN1_TX_GPIO_PORT, &GPIO_InitStructure);
#endif


}


/*																						
*********************************************************************************************************
*	函 数 名: CAN_Mode_Config
*	功能说明: CAN的模式配置
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
static void CAN_Mode_Config(void)
{
	CAN_InitTypeDef        CAN_InitStructure;
	/************************CAN通信参数设置**********************************/
	/* Enable CAN clock */
 RCC_APB1PeriphClockCmd(CAN_CLK, ENABLE);
#if CAN1_ENABLE
	CAN_DeInit(CAN1);
#endif
#if CAN2_ENABLE	
	CAN_DeInit(CAN2);
#endif
	CAN_StructInit(&CAN_InitStructure);

	/*CAN单元初始化*/
	CAN_InitStructure.CAN_TTCM=DISABLE;			   //MCR-TTCM  关闭时间触发通信模式使能
	CAN_InitStructure.CAN_ABOM=ENABLE;			   //MCR-ABOM  自动离线管理 
	CAN_InitStructure.CAN_AWUM=ENABLE;			   //MCR-AWUM  使用自动唤醒模式
	CAN_InitStructure.CAN_NART=DISABLE;			   //MCR-NART  禁止报文自动重传	  DISABLE-自动重传
	CAN_InitStructure.CAN_RFLM=DISABLE;			   //MCR-RFLM  接收FIFO 锁定模式  DISABLE-溢出时新报文会覆盖原有报文  
	CAN_InitStructure.CAN_TXFP=DISABLE;			   //MCR-TXFP  发送FIFO优先级 DISABLE-优先级取决于报文标示符 
	CAN_InitStructure.CAN_Mode = CAN_Mode_Normal;  //正常工作模式
	CAN_InitStructure.CAN_SJW=CAN_SJW_2tq;		   //BTR-SJW 重新同步跳跃宽度 2个时间单元
	 
	/* ss=1 bs1=4 bs2=2 位时间宽度为(1+4+2) 波特率即为时钟周期tq*(1+4+2)  */
	CAN_InitStructure.CAN_BS1=CAN_BS1_4tq;		   //BTR-TS1 时间段1 占用了4个时间单元
	CAN_InitStructure.CAN_BS2=CAN_BS2_2tq;		   //BTR-TS1 时间段2 占用了2个时间单元	
	
	/* CAN Baudrate = 1 MBps (1MBps已为stm32的CAN最高速率) (CAN 时钟频率为 APB 1 = 42 MHz) */
	CAN_InitStructure.CAN_Prescaler =6;		   ////BTR-BRP 波特率分频器  定义了时间单元的时间长度 42/(1+4+2)/6=1 Mbps
#if CAN2_ENABLE	
	CAN_Init(CAN2, &CAN_InitStructure);
#endif
#if CAN1_ENABLE
	CAN_Init(CAN1, &CAN_InitStructure);
#endif
}


/*																						
*********************************************************************************************************
*	函 数 名: CAN_Filter_Config
*	功能说明: CAN的过滤器配置
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
static void CAN_Filter_Config(void)
{

	CAN_FilterInitTypeDef  CAN_FilterInitStructure;

	/*CAN筛选器初始化*/
	CAN_FilterInitStructure.CAN_FilterNumber=0;											///CAN1对应0~13,CAN2对应14~28
	CAN_FilterInitStructure.CAN_FilterMode=CAN_FilterMode_IdMask;		//工作在掩码模式
	CAN_FilterInitStructure.CAN_FilterScale=CAN_FilterScale_32bit;	//筛选器位宽为单个32位。
	/* 使能筛选器，按照标志的内容进行比对筛选，扩展ID不是如下的就抛弃掉，是的话，会存入FIFO0。 */

	CAN_FilterInitStructure.CAN_FilterIdHigh= (((CAN_RECEIVE_EXTID<<3)|CAN_ID_EXT|CAN_RTR_DATA)&0xFFFF0000)>>16;		//要筛选的ID高位 
	CAN_FilterInitStructure.CAN_FilterIdLow= ((CAN_RECEIVE_EXTID<<3)|CAN_ID_EXT|CAN_RTR_DATA)&0xFFFF; //要筛选的ID低位 
	CAN_FilterInitStructure.CAN_FilterMaskIdHigh= 0x0000;																							//筛选器高16位每位必须匹配
	CAN_FilterInitStructure.CAN_FilterMaskIdLow= 0x0000;																							//筛选器低16位每位必须匹配，设置0x0000即关闭
	CAN_FilterInitStructure.CAN_FilterFIFOAssignment=CAN_Filter_FIFO0 ;																//筛选器被关联到FIFO0
	CAN_FilterInitStructure.CAN_FilterActivation=ENABLE;																							//使能筛选器
	CAN_FilterInit(&CAN_FilterInitStructure);
	/*CAN通信中断使能*/
	CAN_ITConfig(CAN1, CAN_IT_FMP0, ENABLE);

	CAN_FilterInitStructure.CAN_FilterNumber=20;						//CAN1对应0~13,CAN2对应14~28
	CAN_FilterInit(&CAN_FilterInitStructure);
	CAN_ITConfig(CAN2, CAN_IT_FMP0, ENABLE);
}



/*																						
*********************************************************************************************************
*	函 数 名: CAN_Config
*	功能说明: 完整配置CAN的功能
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void CAN_Config(void)
{
  CAN_GPIO_Config();
  CAN_Mode_Config();
  CAN_Filter_Config();   
}

/*																						
*********************************************************************************************************
*	函 数 名: Init_RxMes
*	功能说明: 初始化 Rx Message数据结构体
*	形    参：RxMessage: 指向要初始化的数据结构体
*	返 回 值: 无
*********************************************************************************************************
*/
void Init_RxMes(CanRxMsg *RxMessage)
{
  uint8_t ubCounter = 0;

	/*把接收结构体清零*/
  RxMessage->StdId = 0x00;
  RxMessage->ExtId = 0x00;
  RxMessage->IDE = CAN_ID_STD;
  RxMessage->DLC = 0;
  RxMessage->FMI = 0;
  for (ubCounter = 0; ubCounter < 8; ubCounter++)
  {
    RxMessage->Data[ubCounter] = 0x00;
  }
}



/*																						
*********************************************************************************************************
*	函 数 名: CAN_SetMsg
*	功能说明: CAN通信报文内容设置,设置一个数据内容为0-7的数据包
*	形    参：发送报文结构体
*	返 回 值: 无
*********************************************************************************************************
*/
void CAN_SetMsg(CanTxMsg *TxMessage,uint32_t ext_id, uint8_t *sendbuff)
{	  
	uint8_t ubCounter = 0;

  //TxMessage.StdId=0x00;						 
  TxMessage->ExtId=ext_id;					 //使用的扩展ID
  TxMessage->IDE=CAN_ID_EXT;					 //扩展模式
  TxMessage->RTR=CAN_RTR_DATA;				 //发送的是数据
  TxMessage->DLC=8;							 //数据长度为8字节
	
	/*设置要发送的数据0-7*/
	for (ubCounter = 0; ubCounter < 8; ubCounter++)
  {
    TxMessage->Data[ubCounter] = *(sendbuff+ubCounter);
  }
}
/*																						
*********************************************************************************************************
*	函 数 名: CAN2_RX0_IRQHandler
*	功能说明: 
*	形    参：
*	返 回 值: 无
*********************************************************************************************************
*/
void CAN2_RX0_IRQHandler(void)
{
	BaseType_t  xHigherPriorityTaskWoken;	

	/*从邮箱中读出报文*/
	CAN_Receive(CAN2, CAN_FIFO0, &Rx2Message);
	xSemaphoreGiveFromISR(xCan2Semaphore, &xHigherPriorityTaskWoken);
	portYIELD_FROM_ISR(xHigherPriorityTaskWoken);		       //接收成功  


}

/*																						
*********************************************************************************************************
*	函 数 名: CAN1_RX0_IRQHandler
*	功能说明: 
*	形    参：
*	返 回 值: 无
*********************************************************************************************************
*/
void CAN1_RX0_IRQHandler(void)
{
		BaseType_t  xHigherPriorityTaskWoken;	
	
		CAN_Receive(CAN1,CAN_FIFO0, &Rx1Message);
		xSemaphoreGiveFromISR(xCan1Semaphore, &xHigherPriorityTaskWoken);
		portYIELD_FROM_ISR(xHigherPriorityTaskWoken);		       //接收成功  
}


/**************************END OF FILE************************************/












