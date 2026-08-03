#include "bsp_i2c.h"
#include ".\ipmb\bsp_i2c_slave.h"
#include ".\ipmb\ipmb_slave.h"
#include ".\gpio\bsp_gpio.h"
#include <stdio.h>

/* IPMB-A(I2C1)/IPMB-B(I2C2) 初始化: 两路都做 I2C 从机, 应答同一个由卡槽号
 * 派生的地址(真正的冗余双总线语义, 由 task_ipmb.c 里的 IPMB_Arbitration_Task
 * 决定当前哪一路为活动总线)。
 */
void init_ipmb_i2c(void)
{
    /* CPEX: open/high=0, GND/low=1; GA_VALUE holds raw pin levels. */
    uint8_t slot = (uint8_t)((~GA_VALUE) & 0x1F);
    uint8_t addr = (uint8_t)(0x30 + (slot << 1));

    printf("[IPMB] I2C1(A)=slave(0x%02X), I2C2(B)=slave(0x%02X), slot=%u\r\n",
           (unsigned)addr, (unsigned)addr, (unsigned)slot);
    I2C_Slave_Init(I2C1, addr);
    I2C_Slave_Init(I2C2, addr);
}
