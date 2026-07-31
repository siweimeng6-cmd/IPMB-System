#ifndef __BSP_BPD20550_H
#define __BSP_BPD20550_H

#include "bsp_mo_i2c.h"

extern float g_bpd20550_current_A;
extern volatile uint8_t g_bpd20550_current_valid;
uint8_t bpd20550_start_operation(void);
int VRLinear11Format(uint16_t RAWData, float *pOut);
uint8_t bpd20550_ReadBytes(uint8_t *_pReadBuf, uint8_t command, uint16_t _usSize);
void Board_BPD20550_current(void);

#endif
