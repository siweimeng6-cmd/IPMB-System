#ifndef __BSP_I2C_H
#define	__BSP_I2C_H

#include "bsp_init.h"

/* IPMB-A(I2C1)/IPMB-B(I2C2) 硬件初始化入口, 由 BSP_Init() 调用。
 * 实际的 I2C 从机 GPIO/外设/中断配置在 .\ipmb\bsp_i2c_slave.c 里,
 * 这里只负责算出两路共用的从机地址并调用 I2C_Slave_Init()。
 *
 * 从机地址按卡槽号动态计算: addr = 0x30 + (slot << 1),
 * slot 取 GA_VALUE(bsp_gpio.c 的 slave_addr_detect() 读 GA0..GA4+GAP 得到)
 * 的低 5 位按 CPEX 规则取反；高/open=0、低/GND=1。必须在
 * slave_addr_detect() 执行之后调用 (BSP_Init() 中已保证顺序)。
 */
void init_ipmb_i2c(void);

#endif
