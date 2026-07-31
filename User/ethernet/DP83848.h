#ifndef __DP83848_H
#define __DP83848_H

#include "bsp_ethernet.h"
#include "bsp_init.h"

/* MII and RMII mode selection */
#define RMII_MODE  // user have to provide the 50 MHz clock by soldering a 50 MHz oscillator
//#define MII_MODE


void ETH_GPIO_Config(void);

#endif