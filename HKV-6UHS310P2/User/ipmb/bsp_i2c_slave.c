#include "bsp_i2c_slave.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

/* 全局从机变量 */
I2C_Slave_t g_i2c_slave_a = {0};
I2C_Slave_t g_i2c_slave_b = {0};

/* 信号量: 用于 ISR 唤醒 IPMB 任务 */
xSemaphoreHandle xI2C1_Semaphore = NULL;
xSemaphoreHandle xI2C2_Semaphore = NULL;

/**
 * @brief  I2C 从机 GPIO 配置
 * @param  I2Cx I2C 外设指针 (I2C1 或 I2C2)
 * @note   F103 的 I2C 引脚默认复用即 I2C 功能, 不需 GPIO_PinAFConfig
 */
static void I2C_Slave_GPIO_Config(I2C_TypeDef* I2Cx)
{
    GPIO_InitTypeDef GPIO_InitStructure;

    /* PB 时钟 (I2C1 / I2C2 都在 PB) + AFIO 复用功能时钟 */
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    if (I2Cx == I2C1) {
        /* I2C1: PB6=SCL, PB7=SDA */
        GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_6 | GPIO_Pin_7;
        GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
    } else if (I2Cx == I2C2) {
        /* I2C2: PB10=SCL, PB11=SDA */
        GPIO_InitStructure.GPIO_Pin   = GPIO_Pin_10 | GPIO_Pin_11;
        GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
        GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(GPIOB, &GPIO_InitStructure);
    }
}

/**
 * @brief  I2C 从机模式配置
 * @param  I2Cx       I2C 外设指针
 * @param  slave_addr 7-bit 从机地址
 */
static void I2C_Slave_Mode_Config(I2C_TypeDef* I2Cx, uint8_t slave_addr)
{
    I2C_InitTypeDef I2C_InitStructure;

    if (I2Cx == I2C1) {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    } else if (I2Cx == I2C2) {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    }

    I2C_InitStructure.I2C_Mode                = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle           = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1         = (uint16_t)(slave_addr);  /* IPMB 地址已是 8-bit 格式 (7-bit << 1) */
    I2C_InitStructure.I2C_Ack                 = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed          = g_ipmb_i2c_speed;  /* 默认 100KHz,可被 control_req 0x10/0x11 切换,见 ipmb_fru.h */

    I2C_Init(I2Cx, &I2C_InitStructure);
    I2C_Cmd(I2Cx, ENABLE);

    /* 使能事件中断 (含 ADDR / RXNE / TXE / STOPF) 与缓冲区中断 (TXE/RXNE) */
    I2C_ITConfig(I2Cx, I2C_IT_EVT | I2C_IT_BUF, ENABLE);
}

/**
 * @brief  NVIC 配置
 */
static void I2C_Slave_NVIC_Config(I2C_TypeDef* I2Cx)
{
    NVIC_InitTypeDef NVIC_InitStructure;

    if (I2Cx == I2C1) {
        NVIC_InitStructure.NVIC_IRQChannel                   = I2C1_EV_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
        NVIC_Init(&NVIC_InitStructure);

        NVIC_InitStructure.NVIC_IRQChannel                   = I2C1_ER_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
        NVIC_Init(&NVIC_InitStructure);
    } else if (I2Cx == I2C2) {
        NVIC_InitStructure.NVIC_IRQChannel                   = I2C2_EV_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 0;
        NVIC_InitStructure.NVIC_IRQChannelCmd                = ENABLE;
        NVIC_Init(&NVIC_InitStructure);

        NVIC_InitStructure.NVIC_IRQChannel                   = I2C2_ER_IRQn;
        NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
        NVIC_InitStructure.NVIC_IRQChannelSubPriority        = 1;
        NVIC_Init(&NVIC_InitStructure);
    }
}

/**
 * @brief  I2C 从机初始化
 */
void I2C_Slave_Init(I2C_TypeDef* I2Cx, uint8_t slave_addr)
{
    I2C_Slave_t* slave = (I2Cx == I2C1) ? &g_i2c_slave_a : &g_i2c_slave_b;
    memset(slave, 0, sizeof(I2C_Slave_t));
    slave->slave_addr = slave_addr;
    slave->state      = I2C_SLAVE_STATE_IDLE;

    I2C_Slave_GPIO_Config(I2Cx);
    I2C_Slave_Mode_Config(I2Cx, slave_addr);
    I2C_Slave_NVIC_Config(I2Cx);

    /* 创建二进制信号量 (初始为空) */
    if (I2Cx == I2C1) {
        xI2C1_Semaphore = xSemaphoreCreateBinary();
    } else if (I2Cx == I2C2) {
        xI2C2_Semaphore = xSemaphoreCreateBinary();
    }
}

/**
 * @brief  从临时主机模式恢复从机监听
 * @note   与 I2C_Slave_Init 不同: 不 memset 整个结构体 (保留
 *         last_active_tick 供 IPMB_Arbitration_Task / 在线诊断使用),
 *         也不重新创建信号量 (避免句柄泄漏), 只重配 GPIO/外设/NVIC
 *         并清空收发状态, 供 IPMB_A_Task/IPMB_B_Task 在用
 *         I2C_Master_Transmit() 主动推送应答帧后调用.
 */
void I2C_Slave_Resume(I2C_TypeDef* I2Cx)
{
    I2C_Slave_t* slave = (I2Cx == I2C1) ? &g_i2c_slave_a : &g_i2c_slave_b;

    slave->rx_len   = 0;
    slave->tx_len   = 0;
    slave->tx_index = 0;
    slave->state    = I2C_SLAVE_STATE_IDLE;
    slave->event_flag = 0;

    I2C_Slave_GPIO_Config(I2Cx);
    I2C_Slave_Mode_Config(I2Cx, slave->slave_addr);
    I2C_Slave_NVIC_Config(I2Cx);
}

/**
 * @brief  I2C 从机事件中断处理
 * @note   F103 的 I2C 事件通过 SR1/SR2 寄存器 + I2C_GetLastEvent 读取
 */
void I2C_Slave_EV_IRQHandler(I2C_TypeDef* I2Cx)
{
    I2C_Slave_t* slave = (I2Cx == I2C1) ? &g_i2c_slave_a : &g_i2c_slave_b;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint32_t event;
    uint8_t tr;

    event = I2C_GetLastEvent(I2Cx);

    /* 1. 地址匹配 (主机寻址本机) */
    if ((event & I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED) == I2C_EVENT_SLAVE_RECEIVER_ADDRESS_MATCHED ||
        (event & I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED) == I2C_EVENT_SLAVE_TRANSMITTER_ADDRESS_MATCHED) {
        /* 记录最后一次被主控访问的时间 (在线检测) */
        slave->last_active_tick = xTaskGetTickCountFromISR();
        /* TX=1 表示主机要读 (从机发), TX=0 表示主机要写 (从机收) */
        tr = (I2Cx->SR2 & 0x04) ? 1 : 0;
        if (tr) {
            /* 主机读: 从机转入发送 */
            slave->state    = I2C_SLAVE_STATE_TX_DATA;
            slave->tx_index = 0;
            /* 预先装入第 1 字节 (如果有) */
            if (slave->tx_len > 0) {
                I2C_SendData(I2Cx, slave->tx_buffer[slave->tx_index++]);
            }
        } else {
            /* 主机写: 从机转入接收, 清空缓冲区 */
            slave->state  = I2C_SLAVE_STATE_RX_DATA;
            slave->rx_len = 0;
        }
        I2C_ClearFlag(I2Cx, I2C_FLAG_ADDR);
    }
    /* 2. RXNE: 收到 1 字节 (从机收) */
    else if ((event & I2C_EVENT_SLAVE_BYTE_RECEIVED) == I2C_EVENT_SLAVE_BYTE_RECEIVED) {
        if (slave->state == I2C_SLAVE_STATE_RX_DATA) {
            if (slave->rx_len < I2C_SLAVE_BUFF_SIZE) {
                slave->rx_buffer[slave->rx_len++] = I2C_ReceiveData(I2Cx);
            } else {
                /* 缓冲区满, 丢弃 */
                (void)I2C_ReceiveData(I2Cx);
            }
        } else {
            (void)I2C_ReceiveData(I2Cx);
        }
    }
    /* 3. TXE: 发送缓冲空 (从机发) */
    else if ((event & I2C_EVENT_SLAVE_BYTE_TRANSMITTED) == I2C_EVENT_SLAVE_BYTE_TRANSMITTED ||
             (event & I2C_EVENT_SLAVE_BYTE_TRANSMITTING) == I2C_EVENT_SLAVE_BYTE_TRANSMITTING) {
        if (slave->state == I2C_SLAVE_STATE_TX_DATA) {
            if (slave->tx_index < slave->tx_len) {
                I2C_SendData(I2Cx, slave->tx_buffer[slave->tx_index++]);
            } else {
                /* 数据已发完, 保持 0xFF 等待主机 STOP */
                I2C_SendData(I2Cx, 0xFF);
            }
        }
    }
    /* 4. STOPF: 主机发 STOP, 帧接收完成 */
    if ((event & I2C_EVENT_SLAVE_STOP_DETECTED) == I2C_EVENT_SLAVE_STOP_DETECTED) {
        I2C_Cmd(I2Cx, ENABLE);  /* 复位从机 */
        if (slave->state == I2C_SLAVE_STATE_RX_DATA) {
            slave->event_flag |= I2C_SLAVE_EVENT_RX;
            /* 通知任务: 一帧接收完成 */
            if (I2Cx == I2C1) {
                if (xI2C1_Semaphore != NULL) {
                    xSemaphoreGiveFromISR(xI2C1_Semaphore, &xHigherPriorityTaskWoken);
                }
            } else if (I2Cx == I2C2) {
                if (xI2C2_Semaphore != NULL) {
                    xSemaphoreGiveFromISR(xI2C2_Semaphore, &xHigherPriorityTaskWoken);
                }
            }
        }
        slave->state = I2C_SLAVE_STATE_IDLE;
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/**
 * @brief  I2C 从机错误中断处理
 */
void I2C_Slave_ER_IRQHandler(I2C_TypeDef* I2Cx)
{
    uint8_t idx = (I2Cx == I2C1) ? 1 : 2;
    if (I2C_GetITStatus(I2Cx, I2C_IT_BERR)) {
        printf("[I2C%d] ER: BERR\r\n", idx);
        I2C_ClearITPendingBit(I2Cx, I2C_IT_BERR);
    }
    if (I2C_GetITStatus(I2Cx, I2C_IT_ARLO)) {
        printf("[I2C%d] ER: ARLO\r\n", idx);
        I2C_ClearITPendingBit(I2Cx, I2C_IT_ARLO);
    }
    if (I2C_GetITStatus(I2Cx, I2C_IT_AF)) {
        printf("[I2C%d] ER: AF\r\n", idx);
        I2C_ClearITPendingBit(I2Cx, I2C_IT_AF);
    }
    if (I2C_GetITStatus(I2Cx, I2C_IT_OVR)) {
        printf("[I2C%d] ER: OVR\r\n", idx);
        I2C_ClearITPendingBit(I2Cx, I2C_IT_OVR);
    }
}

/**
 * @brief  从从机接收缓冲区取走数据
 */
uint8_t I2C_Slave_GetRxData(I2C_Slave_t* slave, uint8_t* buffer, uint8_t len)
{
    uint8_t copy_len = (len < slave->rx_len) ? len : slave->rx_len;
    if (copy_len > 0) {
        memcpy(buffer, slave->rx_buffer, copy_len);
        slave->rx_len = 0;
    }
    return copy_len;
}

/**
 * @brief  设置从机待发送的数据
 */
void I2C_Slave_SetTxData(I2C_Slave_t* slave, const uint8_t* buffer, uint8_t len)
{
    if (len > I2C_SLAVE_BUFF_SIZE) {
        len = I2C_SLAVE_BUFF_SIZE;
    }
    if (buffer != NULL && len > 0) {
        memcpy(slave->tx_buffer, buffer, len);
    }
    slave->tx_len   = len;
    slave->tx_index = 0;
}

uint8_t I2C_Slave_GetEvent(I2C_Slave_t* slave)
{
    return slave->event_flag;
}

void I2C_Slave_ClearEvent(I2C_Slave_t* slave, uint8_t event)
{
    slave->event_flag &= ~event;
}
