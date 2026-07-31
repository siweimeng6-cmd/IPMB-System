#ifndef _TASK_ETHERNET_H_
#define _TASK_ETHERNET_H_

#include "bsp_ethernet.h"
#include "stm32f4x7_eth.h"
#include "bsp_init.h"
#include "netconf.h"
#include "UDPBase.h"

#define UDP_RX_QUEUE_SIZE 					128

extern TaskHandle_t task_enternet_Handle;
extern TaskHandle_t Ethernet_receive_Task_Handle;

void task_enternet_entry(void *p);
uint32_t Eth_Link_PHYITConfig(uint16_t PHYAddress);
void Ethernet_LinkCheckTask( void * pvParameters );
void Ethernet_receive_Task(void * pvParameters);

#endif
