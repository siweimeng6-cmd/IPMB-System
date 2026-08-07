#ifndef __BSP_BL_TICK_H
#define __BSP_BL_TICK_H

#include "stm32f10x.h"

void BL_Tick_Init(void);
uint32_t BL_GetTick(void);
void BL_Delay(uint32_t ms);

#endif
