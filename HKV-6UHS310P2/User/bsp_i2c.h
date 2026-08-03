#ifndef __BSP_I2C_H
#define	__BSP_I2C_H

#include "bsp_init.h"

/* 兼容层: 旧 API 已重定向到 ipmb/bsp_i2c_slave.c
 * 此处仅保留 IPMB-A/B 初始化入口, 由 BSP_Init 调用.
 */

/* IPMB-A/B 从机地址不再固定, 改由 init_ipmb_i2c() 内部根据
 * read_slot_from_gpio() 动态计算: 0x30 + (Slot_ID << 1) */

/* 角色切换宏
 *   0: IPMB-A 保持 I2C 从机 (默认, 业务板对外)
 *   1: IPMB-A 切为 I2C 主机, 用于自测 IPMB-B
 *      - 需要外部把 I2C1 (PB6/PB7) 与 I2C2 (PB10/PB11) 的
 *        SDA-SDA / SCL-SCL 短接成同一物理 I2C 总线.
 *      - 用户通过 UART4 下发 hex 裸帧, IPMB-A 主机转发到 IPMB-B
 *        从机, 收到响应后通过 UART4 同时打印 hex + 解析字段.
 */
#define IPMB_A_AS_MASTER   0

void init_ipmb_i2c(void);

#endif
