#ifndef __LAN8720_H
#define __LAN8720_H

#include "bsp_ethernet.h"
/* FreeRTOSÍ·ÎÄ¼þ */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx.h"
#include "bsp_ethernet.h"
#include "stm32f4x7_eth.h"



void ETH_GPIO_Config(void);

#endif