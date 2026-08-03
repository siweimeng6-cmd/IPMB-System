#ifndef __BSP_I2C_SLAVE_H
#define __BSP_I2C_SLAVE_H

#include "stm32f10x.h"
#include "bsp_init.h"
#include "FreeRTOS.h"
#include "semphr.h"

/* I2C 从机缓冲区大小定义 */
#define I2C_SLAVE_BUFF_SIZE    128    /* 需容纳 OEM 模块信息响应 (~116 字节) */
/* I2C 从机地址: 7-bit 形式 */
#define I2C_SLAVE_ADDR         0x8E
/* I2C 从机事件标志 */
#define I2C_SLAVE_EVENT_RX     0x01    /* 接收完成 (已收到 STOP) */
#define I2C_SLAVE_EVENT_TX     0x02    /* 主机已发读地址, 进入发送 */

/* I2C 从机状态枚举 */
typedef enum {
    I2C_SLAVE_STATE_IDLE = 0,       /* 空闲 */
    I2C_SLAVE_STATE_RX_DATA,        /* 接收数据中 */
    I2C_SLAVE_STATE_TX_DATA         /* 发送数据中 */
} I2C_Slave_State_t;

/* I2C 从机结构体 */
typedef struct {
    uint8_t  rx_buffer[I2C_SLAVE_BUFF_SIZE];    /* 接收缓冲区 */
    uint8_t  rx_len;                            /* 接收数据长度 */
    uint8_t  tx_buffer[I2C_SLAVE_BUFF_SIZE];    /* 发送缓冲区 */
    uint8_t  tx_len;                            /* 发送数据长度 */
    uint8_t  tx_index;                          /* 发送数据索引 */
    I2C_Slave_State_t state;                   /* 从机状态 */
    uint8_t  slave_addr;                        /* 从机 7-bit 地址 */
    uint8_t  event_flag;                        /* 事件标志 */
    volatile uint32_t last_active_tick;         /* 最后一次被主控访问的 Tick */
} I2C_Slave_t;

/* 全局从机变量 (供 ISR 引用) */
extern I2C_Slave_t g_i2c_slave_a;   /* I2C1 - IPMB-A */
extern I2C_Slave_t g_i2c_slave_b;   /* I2C2 - IPMB-B */

/* 信号量: 用于 ISR 唤醒 IPMB 任务 */
extern xSemaphoreHandle xI2C1_Semaphore;
extern xSemaphoreHandle xI2C2_Semaphore;

void I2C_Slave_Init(I2C_TypeDef* I2Cx, uint8_t slave_addr);
/* 从临时主机模式恢复从机监听: 与 I2C_Slave_Init 不同, 不清零
 * last_active_tick、不重新创建信号量, 只重配 GPIO/外设/NVIC. */
void I2C_Slave_Resume(I2C_TypeDef* I2Cx);
void I2C_Slave_EV_IRQHandler(I2C_TypeDef* I2Cx);
void I2C_Slave_ER_IRQHandler(I2C_TypeDef* I2Cx);
uint8_t I2C_Slave_GetRxData(I2C_Slave_t* slave, uint8_t* buffer, uint8_t len);
void I2C_Slave_SetTxData(I2C_Slave_t* slave, const uint8_t* buffer, uint8_t len);
uint8_t I2C_Slave_GetEvent(I2C_Slave_t* slave);
void I2C_Slave_ClearEvent(I2C_Slave_t* slave, uint8_t event);

#endif
