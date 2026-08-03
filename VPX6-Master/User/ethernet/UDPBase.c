/*********************************************************************************************************
* 模块名称：UDPBase.c
* 摘    要：UDP驱动模块
* 当前版本：1.0.0
* 作    者：Leyutek(COPYRIGHT 2018 - 2021 Leyutek. All rights reserved.)
* 完成日期：2021年07月01日
* 内    容：
* 注    意：
**********************************************************************************************************
* 取代版本：
* 作    者：
* 完成日期：
* 修改内容：
* 修改文件：
*********************************************************************************************************/
/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "UDPBase.h"
#include "stdio.h"
#include "string.h"
#include "pQueue.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "semphr.h"
//#include "task_logger.h"

//LOGGER_MODULE_NAME("Ethernet");


 StructUDPDev s_structUDPDev; //UDP设备结构体
 QueueHandle_t  queue_ethernet_Handle;


/*********************************************************************************************************
* 函数名称：UDPRecvCallback
* 函数功能：UDP数据接收回调函数
* 输入参数：arg：用户参数
*          upcb：UDP的PCB
*          p：数据包首地址
*          addr：远程IP地址
*          port：远程端口号
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
static void UDPRecvCallback(void *arg, struct udp_pcb *upcb, struct pbuf *p, struct ip_addr *addr, uint16_t port)
{
#if DONT_USE_FREERTOS_QUEUE_RECEIVE_ETHERNET	//不使用freertos传送，使用标志位
  struct pbuf *q;
  //接收到不为空的数据时
  if(p != NULL)
  {
    //遍历完整个pbuf链表
    for(q = p; q != NULL; q = q->next)
    {
      //将数据保存到队列中
      EnQueue(&s_structUDPDev.recvCirQue, (DATA_TYPE*)q->payload, q->len);	//此队列为自定义队列，非rtos队列
			read_udp_length =q->len;
    }

    pbuf_free(p);//释放内存
  }
#else			//使用freertos的队列传送数据长度和地址
    struct pbuf *q = NULL;
	  uint8_t *m = NULL;  //, err_flag = 0
    uint16_t len = 0, total_len = 0;
	
	  q = p;
    if(q != NULL){
			  total_len = q->tot_len;
			  //LOGGER_PRINTF( "[UDP1]:recv total len = %d", total_len );
			  m = (uint8_t *)pvPortMalloc( total_len + 1 );
				if ( m != NULL ){
					 len = 0;
					 while( q != NULL ){
						  if (( len + q->len ) < ( total_len + 1 )){
								 memcpy( (uint8_t *)m + len, q->payload, q->len );
								 len += q->len;
						  }
							else{
								 printf( "udp1_recv_callback len %u != total %u err", len, total_len );//
								 break;
							}
						  q = q->next;
					 }
           
					 if( ethernet_send_queue( (uint32_t)m, len ) !=0 ){	//将数据长度和数据地址发送至freertos队列
						  vPortFree( m );
						  printf( "udp1_recv_callback xQueueSend fail" );
					 }
				}
				else{
					  printf( "udp1_recv_callback malloc fail" );
				}
				
        /* end of processing, free the pbuf */
        pbuf_free(p);
    } 
#endif
}

uint8_t ethernet_send_queue(uint32_t recv_data, uint16_t recv_data_len)
{
		stEVENT_t  ethernet_evt;
		ethernet_evt.data = recv_data;
		ethernet_evt.data_len = recv_data_len;
	
		if( queue_ethernet_Handle != NULL ){
			if( xQueueSend( queue_ethernet_Handle, &ethernet_evt, 0 ) == pdPASS ){
				return 0;
			}
	  }
		
    return -1;	
}



/*********************************************************************************************************
* 函数名称：CreateUDPConnect
* 函数功能：创建UDP连接
* 输入参数：ip0-ip3：服务器IP地址，例如192, 168, 1, 104，port：TCP服务器端口
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void CreateUDPConnect(uint8_t ServerIP0, uint8_t ServerIP1, uint8_t ServerIP2, uint8_t ServerIP3, uint16_t port)
{
  err_t err;
  struct ip_addr rmtipaddr;  //远端ip地址
                                                            //客户端端口号

  //标记连接断开
  s_structUDPDev.state = ES_UDP_NONE;

  //创建一个新的pcb
  s_structUDPDev.pcb = udp_new();
  if(s_structUDPDev.pcb)
  { 
    //生成IPV4地址
    IP4_ADDR(&rmtipaddr, ServerIP0, ServerIP1, ServerIP2, ServerIP3);

    //UDP客户端连接到指定IP地址和端口号的服务器
    err = udp_bind(s_structUDPDev.pcb, IP_ADDR_ANY, UDP_SERVER_PORT);
    if(err == ERR_OK)
    {
      //绑定本地IP地址与端口号
      err = udp_connect(s_structUDPDev.pcb, &rmtipaddr, port);

      //绑定完成
      if(err == ERR_OK)
      {
        //注册接收回调函数
        udp_recv(s_structUDPDev.pcb, UDPRecvCallback, NULL);

        //标记已连接
        s_structUDPDev.state = ES_UDP_CONNECTED;

        //初始化接收队列
        InitQueue(&s_structUDPDev.recvCirQue, s_structUDPDev.recvBuf, UDP_RX_BUFSIZE);

        //清空发送缓冲区
        s_structUDPDev.sendNum = 0;
      }
    }
  }

  //打印输出连接成功
  if(ES_UDP_CONNECTED == s_structUDPDev.state)
  {
    printf("****************UDP CONNECTED********************\r\n");
  }
}

/*********************************************************************************************************
* 函数名称：UDPConnectionClose
* 函数功能：关闭UDP连接
* 输入参数：void
* 输出参数：void
* 返 回 值：void
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
void UDPConnectionClose(void)
{
  udp_disconnect(s_structUDPDev.pcb); //断开UDP连接
  udp_remove(s_structUDPDev.pcb);     //移除UDP
  s_structUDPDev.state = ES_UDP_NONE; //标记连接断开
}

/*********************************************************************************************************
* 函数名称：ReadUDP
* 函数功能：读UDP接收数据缓冲区
* 输入参数：buf-读出数据储存地址，len-读取长度
* 输出参数：void
* 返 回 值：成功读取到的数据量
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
uint32_t ReadUDP(uint8_t* buf, uint32_t len)
{
  //校验是否已连接
  if(ES_UDP_CONNECTED != s_structUDPDev.state)
  {
    return 0;
  }

  return DeQueue(&s_structUDPDev.recvCirQue, buf, len);
}

/*********************************************************************************************************
* 函数名称：WriteUDP
* 函数功能：UDP发送数据
* 输入参数：buf-发送数据储存地址，len-发送长度
* 输出参数：void
* 返 回 值：成功发送的数据量
* 创建日期：2021年07月01日
* 注    意：
*********************************************************************************************************/
uint32_t WriteUDP(uint8_t* buf, uint32_t len)
{
  uint32_t sendCnt;
  struct pbuf *ptr;

  //校验是否已连接
  if(ES_UDP_CONNECTED != s_structUDPDev.state)
  {
    return 0;
  }

  //将发送数据拷贝到发送缓冲区
  sendCnt = 0;
  while((s_structUDPDev.sendNum < UDP_TX_BUFSIZE) && (sendCnt < len))
  {
    s_structUDPDev.sendBuf[s_structUDPDev.sendNum] = buf[sendCnt];
    s_structUDPDev.sendNum++;
    sendCnt++;
  }

  //发送数据
  ptr = pbuf_alloc(PBUF_TRANSPORT, s_structUDPDev.sendNum, PBUF_POOL); //申请内存
  if(ptr)
  {
    //拷贝数据到pbuf
    memcpy(ptr->payload, s_structUDPDev.sendBuf, s_structUDPDev.sendNum);

    //udp发送数据
    udp_send(s_structUDPDev.pcb, ptr);

    //释放内存
    pbuf_free(ptr);
  }
  s_structUDPDev.sendNum = 0;

  return sendCnt;
}


