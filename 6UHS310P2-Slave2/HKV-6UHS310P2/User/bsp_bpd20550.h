#ifndef __BSP_BPD20550_H
#define	__BSP_BPD20550_H

#include "bsp_mo_i2c.h"   /* stm32f10x.h + bit-bang i2c_* funcs + BPD20550_* macros (PB8/PB9) */
#include <stdint.h>

/* 最近一次成功读取的 12V 输入电流(A),供 IPMB 传感器查询直接读取缓存值,
 * 避免在响应请求时现场发起一次(较慢的)位带 I2C 事务。由
 * Board_BPD20550_current() 周期性调用时更新。读失败时保持上一次的值不变。 */
extern float g_bpd20550_current_A;

uint8_t bpd20550_start_operation(void);
int     VRLinear11Format(uint16_t RAWData, float *pOut);
uint8_t bpd20550_ReadBytes(uint8_t *_pReadBuf, uint8_t command, uint16_t _usSize);
void    Board_BPD20550_current(char *outbuf);

#endif
