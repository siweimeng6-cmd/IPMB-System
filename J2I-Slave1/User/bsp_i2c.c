#include "bsp_i2c.h"
#include "bsp_i2c_slave.h"
#include "bsp_i2c_master.h"
#include ".\ipmb\ipmb_slave.h"
#include <stdio.h>

/* 兼容层入口: 初始化 IPMB-A / IPMB-B 两路 I2C
 *
 * IPMB_A_AS_MASTER = 0: IPMB-A(I2C1) 与 IPMB-B(I2C2) 都做 I2C 从机
 * IPMB_A_AS_MASTER = 1: IPMB-A(I2C1) 改做 I2C 主机 (用于自测),
 *                     仅 IPMB-B(I2C2) 保留从机.
 *                     此时需外部把 I2C1 与 I2C2 的 SDA/SCL 短接.
 *
 * 从机地址按背板槽位号动态计算: addr = 0x30 + (Slot_ID << 1),
 * Slot_ID 由 read_slot_from_gpio() 读取 GA0..GA4 得到 (需在
 * bsp_gpio_init() 之后调用, BSP_Init() 中已保证此顺序).
 */
void init_ipmb_i2c(void)
{
    uint8_t slot = read_slot_from_gpio();
    uint8_t addr = (uint8_t)(0x30 + (slot << 1));

#if IPMB_A_AS_MASTER
    printf("[IPMB] Mode=MASTER: I2C1=master @100KHz, I2C2=slave(0x%02X, slot=%u)\r\n",
           (unsigned)addr, (unsigned)slot);
    I2C_Master_Init(I2C1, 100000);   /* I2C1 主机 @100KHz */
    I2C_Slave_Init(I2C2, addr);      /* I2C2 仍是从机 */
#else
    printf("[IPMB] Mode=SLAVE: I2C1=slave(0x%02X), I2C2=slave(0x%02X), slot=%u\r\n",
           (unsigned)addr, (unsigned)addr, (unsigned)slot);
    I2C_Slave_Init(I2C1, addr);
    I2C_Slave_Init(I2C2, addr);
#endif
}


