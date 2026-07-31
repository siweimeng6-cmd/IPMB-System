#ifndef __BSP_BPD20550_H
#define	__BSP_BPD20550_H

#include "stm32f4xx.h"
#include <stdint.h>

uint8_t bpd20550_start_operation(void);
int     VRLinear11Format(uint16_t RAWData, float *pOut);
uint8_t bpd20550_ReadBytes(uint8_t *_pReadBuf, uint8_t command, uint16_t _usSize);
void    Board_BPD20550_current(char *outbuf);

#endif
