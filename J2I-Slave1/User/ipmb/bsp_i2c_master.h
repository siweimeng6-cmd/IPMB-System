#ifndef __BSP_I2C_MASTER_H
#define __BSP_I2C_MASTER_H

#include "stm32f10x.h"

/* ============================================================================
 * 硬件 I2C 主机驱动 (轮询模式)
 *
 * 用于 IPMB-A 在自测模式下充当局端, 主动向 IPMB-B 发起 IPMB 请求.
 * 与 bsp_i2c_slave.c 的从机驱动不同: 本驱动
 *   - 不使用 NVIC / 信号量, 全部用 SR1 标志位 + SysTick 超时
 *   - 调用方必须在 FreeRTOS 任务上下文, 阻塞时间 < 5ms/次
 *   - 共享同一对 SCL/SDA 引脚, 但 I2C1/I2C2 寄存器相互独立
 * ==========================================================================*/

/* 返回码 */
#define I2C_M_OK               0
#define I2C_M_ERR_START        1    /* START 阶段超时 */
#define I2C_M_ERR_ADDR_NACK    2    /* 地址阶段 NACK (从机不应答) */
#define I2C_M_ERR_TX           3    /* 数据发送阶段超时 */
#define I2C_M_ERR_RX           4    /* 数据接收阶段超时 */
#define I2C_M_ERR_TIMEOUT      5    /* 总超时 (起始前) */

/**
 * @brief  初始化指定 I2C 外设为主机模式 (轮询, 不开中断)
 * @param  I2Cx   I2C1 或 I2C2
 * @param  clkHz  SCL 频率 (Hz), 标准 100000 / 400000
 * @note   会先 I2C_DeInit + 软件复位, 清空原从机模式的 NVIC 状态.
 */
void I2C_Master_Init(I2C_TypeDef* I2Cx, uint32_t clkHz);

/**
 * @brief  主机发送 tx[len], 写完后 STOP
 * @return I2C_M_OK / I2C_M_ERR_xxx
 */
uint8_t I2C_Master_Transmit(I2C_TypeDef* I2Cx,
                            uint8_t addr7,
                            const uint8_t* tx, uint8_t len,
                            uint32_t timeout_ms);

/**
 * @brief  主机读 rx[len] 字节, 读完 STOP
 *         最后一字节前自动发 NACK
 * @return I2C_M_OK / I2C_M_ERR_xxx
 */
uint8_t I2C_Master_Receive(I2C_TypeDef* I2Cx,
                           uint8_t addr7,
                           uint8_t* rx, uint8_t len,
                           uint32_t timeout_ms);

/**
 * @brief  复合传输 (IPMB 主机典型流程):
 *         先 START+发地址+写 tx[] -> Sr+发地址+读 rx[] -> STOP.
 *         中间不释放总线, 适用于从机需要连续处理 请求/响应 的场景.
 * @return I2C_M_OK / I2C_M_ERR_xxx
 */
uint8_t I2C_Master_WriteRead(I2C_TypeDef* I2Cx,
                             uint8_t addr7,
                             const uint8_t* tx, uint8_t tx_len,
                             uint8_t* rx, uint8_t rx_len,
                             uint32_t timeout_ms);

/**
 * @brief  总线恢复: 手动产生 9 个 SCL 脉冲释放可能卡死的从机, 然后 STOP
 *         调用前 SCL/SDA 需为开漏输出
 */
void I2C_Master_BusRecover(I2C_TypeDef* I2Cx);

#endif /* __BSP_I2C_MASTER_H */


