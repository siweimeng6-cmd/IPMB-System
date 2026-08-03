/*********************************************************************************************************
* 模块名称：UDPBase.h
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
#ifndef _UDP_BASE_H_
#define _UDP_BASE_H_

/*********************************************************************************************************
*                                              包含头文件
*********************************************************************************************************/
#include "stdint.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"
#include "lwip/tcp.h"
#include "pQueue.h"

#include "FreeRTOS.h"
#include "queue.h"
#include "bsp_init.h"
/*********************************************************************************************************
*                                              宏定义
*********************************************************************************************************/
#define UDP_RX_BUFSIZE 2048 //定义udp最大接收数据长度
#define UDP_TX_BUFSIZE 512  //定义udp最大发送数据长度
#define TASK_ETHERNET_RX_QUEUE_SIZE      (50) //160  
#define DONT_USE_FREERTOS_QUEUE_RECEIVE_ETHERNET        0


/*********************************************************************************************************
*                                              枚举结构体定义
*********************************************************************************************************/
//UDP连接状态
typedef enum
{
  ES_UDP_NONE = 0,  //没有连接
  ES_UDP_CONNECTED, //连接上了
}EnumUDPStates;

//客户端设备结构体
typedef struct
{
  EnumUDPStates    state;                   //当前连接状
  struct udp_pcb*  pcb;                     //指向当前的pcb
  StructCirQue     recvCirQue;              //接收循环队列
  unsigned char    recvBuf[UDP_RX_BUFSIZE]; //接收循环队列的缓冲区
  uint32_t              sendNum;                 //发送缓冲区中的数据量
  unsigned char    sendBuf[UDP_TX_BUFSIZE]; //发送缓冲区
}StructUDPDev;

/*********************************************************************************************************
*                                              API函数声明
*********************************************************************************************************/
//extern QueueHandle_t  queue_ethernet;
extern StructUDPDev s_structUDPDev; //UDP设备结构体
extern uint16_t read_udp_length;
extern QueueHandle_t  queue_ethernet_Handle;

uint8_t ethernet_send_queue(uint32_t recv_data, uint16_t recv_data_len);
void CreateUDPConnect(uint8_t ServerIP0, uint8_t ServerIP1, uint8_t ServerIP2, uint8_t ServerIP3, uint16_t port); //创建UDP连接
void UDPConnectionClose(void);   //关闭UDP连接
uint32_t  ReadUDP(uint8_t* buf, uint32_t len);  //读UDP接收数据缓冲区
uint32_t  WriteUDP(uint8_t* buf, uint32_t len); //UDP发送数据

#endif
