#ifndef __BSP_INIT_H
#define	__BSP_INIT_H

/*
*************************************************************************
*                             ??????????
*************************************************************************
*/ 
/* FreeRTOS???? */
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "semphr.h"
#include "stm32f4xx.h"

#include <stdio.h>
#include "stdbool.h"
#include "string.h"

/* ?????????bsp???? */
#include "bsp_usart.h"
#include "task_usart.h"
#include "bsp_pwm.h"
#include "task_i2c.h"
#include "task_slot_cache.h"
#include "task_slot_poll.h"
#include "task_ctrl_dispatch.h"
#include "ipmb_request_builder.h"
#include "bsp_i2c.h"
#include "bsp_adc.h"
#include "bsp_gpio.h"
#include "netconf.h"
#include "bsp_ethernet.h"
#include "UDPBase.h"
#include "task_ethernet.h"
#include "bsp_mo_i2c.h"
#include "stm32f4x7_eth.h"
#include "bsp_mo_i2c.h"
#include "task_pwm.h"
#include "UDPBase.h"
#include "./can/bsp_can.h"

/* 网页控制台改造(2026-07-27)阶段0:task_enternet_entry/Ethernet_receive_Task 是与 IPMB/httpd
 * 无关的 UDP 收发自测代码(见 task_ethernet.c),软禁用(不删除、可逆)以回收约4KB堆,
 * 给新增的槽位缓存/控制任务腾空间。改成1即可恢复原有UDP自测功能。 */
#define ENABLE_UDP_TEST_TRAFFIC   0

/****************************START???????????????**********************************************/
/* MAC ADDRESS*/
#define MAC_ADDR0   0x04
#define MAC_ADDR1   0x78
#define MAC_ADDR2   0x63
#define MAC_ADDR3   0xA0
#define MAC_ADDR4   0x0B
#define MAC_ADDR5   0xF7
 
/*Static IP ADDRESS*/
#define IP_ADDR0   192
#define IP_ADDR1   168
#define IP_ADDR2   100
#define IP_ADDR3   20
   
/*NETMASK*/
#define NETMASK_ADDR0   255
#define NETMASK_ADDR1   255
#define NETMASK_ADDR2   255



#define NETMASK_ADDR3   0

/*Gateway Address*/
#define GW_ADDR0   192
#define GW_ADDR1   168
#define GW_ADDR2   100
#define GW_ADDR3   1  

#define DEST_IP_ADDR0               192
#define DEST_IP_ADDR1               168
#define DEST_IP_ADDR2                 100
#define DEST_IP_ADDR3               3

#define DEST_PORT                  5000
#define UDP_SERVER_PORT            5000   /* define the UDP local connection port */
#define UDP_CLIENT_PORT            6000   /* define the UDP remote connection port */


/* Uncomment the define below to clock the PHY from external 25MHz crystal (only for MII mode) */
#ifdef 	MII_MODE
 #define PHY_CLOCK_MCO
#endif

/*---------------------------------------------------------------------------------------------------*/
#pragma pack(1)
typedef struct
{
	uint16_t data_len;
	uint32_t data;
}stEVENT_t;

#define PRINTF_BUF_SIZE     		 1024
typedef struct
{
	unsigned short size;
	unsigned char  buf[PRINTF_BUF_SIZE];
}stPRINTF_BUF_t, *pstPRINTF_BUF_t;

extern stPRINTF_BUF_t stPrintf_Buf;

extern __IO uint32_t  EthStatus;
extern xSemaphoreHandle s_xSemaphore;
extern xSemaphoreHandle ETH_link_xSemaphore;
/***********************************END?????*******************************************************/

void delay_1ms(uint32_t count);
void SemaphoreTakeCreate(void);
void task_create(void);
void BSP_Init(void);

#endif 
