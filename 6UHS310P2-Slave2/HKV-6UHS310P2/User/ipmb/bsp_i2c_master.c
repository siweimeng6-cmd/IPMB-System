#include "bsp_i2c_master.h"
#include "bsp_init.h"        /* SystemCoreClock (CMSIS) */
#include "FreeRTOS.h"
#include "task.h"
#include "misc.h"            /* NVIC_DisableIRQ */
#include <stddef.h>

/* ============================================================================
 * F103 标准库 I2C 主机轮询实现 (基于 STM32F10x_StdPeriph)
 *
 * 关键 EV 标志 (与 RM0008 §27.7 一致):
 *   EV5 : SB=1, MSL=1, BUSY=1                    (START 已发出)
 *   EV6 : ADDR=1, MSL=1, TXE=1, TRA=1, ACK=0    (地址已发出, 收到 ACK)
 *   EV8_1: TXE=1, shift 空, 没数据要发, 等待写 DR
 *   EV8   : TXE=1, shift 寄存器不空
 *   EV8_2 : BTF=1, TRA=1                          (字节已发出, 收到 ACK)
 *   EV6_RX: ADDR=1, MSL=1, RXNE=0                (地址已发出, 收到 ACK)
 *   EV7   : RXNE=1                                (收到 1 字节)
 *
 * 全部用 I2C_CheckEvent + SysTick 实现超时.
 * ==========================================================================*/

#define I2C_FLAG_TIMEOUT_MS   50U     /* 单次等待 EV 的超时 */

/* 记录 I2C_Master_Init 配置的时钟频率, 供 SWRST 恢复时重新配置外设 */
static uint32_t s_master_clk = 100000;
#define I2C_SR1_SB            0x0001
#define I2C_SR1_ADDR          0x0002
#define I2C_SR1_BTF           0x0004
#define I2C_SR1_RXNE          0x0040
#define I2C_SR1_TXE           0x0080
#define I2C_SR1_AF            0x0400
#define I2C_SR1_ARLO          0x0200
#define I2C_SR1_BERR          0x0100
#define I2C_SR1_STOPF         0x0010
#define I2C_SR1_OVR           0x0800

/* ---------------- 底层超时等待 ---------------- */

static uint8_t wait_for_event(I2C_TypeDef* I2Cx, uint32_t ev, uint32_t timeout_ms)
{
    uint32_t start = xTaskGetTickCount();
    while (I2C_CheckEvent(I2Cx, ev) == ERROR) {
        /* 关键错误: NACK (AF) 或仲裁丢失 (ARLO) 或总线错 (BERR) */
        if (I2Cx->SR1 & I2C_SR1_AF) {
            I2C_ClearFlag(I2Cx, I2C_FLAG_AF);
            return 2;     /* NACK */
        }
        if (I2Cx->SR1 & I2C_SR1_ARLO) {
            I2C_ClearFlag(I2Cx, I2C_FLAG_ARLO);
            return 5;
        }
        if (I2Cx->SR1 & I2C_SR1_BERR) {
            I2C_ClearFlag(I2Cx, I2C_FLAG_BERR);
            return 5;
        }
        if ((xTaskGetTickCount() - start) > timeout_ms) {
            return 1;     /* TIMEOUT */
        }
    }
    return 0;
}

static uint8_t wait_for_flag(I2C_TypeDef* I2Cx, uint32_t sr1_mask, uint32_t timeout_ms)
{
    uint32_t start = xTaskGetTickCount();
    while ((I2Cx->SR1 & sr1_mask) == 0) {
        if (I2Cx->SR1 & I2C_SR1_AF) {
            I2C_ClearFlag(I2Cx, I2C_FLAG_AF);
            return 2;
        }
        if ((xTaskGetTickCount() - start) > timeout_ms) {
            return 1;
        }
    }
    return 0;
}

/* 等待 SB=1 后才能写 DR (地址字节) */
static uint8_t wait_start(I2C_TypeDef* I2Cx, uint32_t timeout_ms)
{
    return wait_for_flag(I2Cx, I2C_SR1_SB, timeout_ms);
}

/* 等待 ADDR=1 后读 SR1 + SR2 清零 */
static uint8_t wait_addr(I2C_TypeDef* I2Cx, uint32_t timeout_ms)
{
    uint8_t r = wait_for_flag(I2Cx, I2C_SR1_ADDR, timeout_ms);
    if (r == 0) {
        (void)I2Cx->SR1;
        (void)I2Cx->SR2;     /* 读 SR2 清除 ADDR */
    }
    return r;
}

/* 等待 TXE=1 (data register 空) */
static uint8_t wait_txe(I2C_TypeDef* I2Cx, uint32_t timeout_ms)
{
    return wait_for_flag(I2Cx, I2C_SR1_TXE, timeout_ms);
}

/* 等待 RXNE=1 (data register 非空) */
static uint8_t wait_rxne(I2C_TypeDef* I2Cx, uint32_t timeout_ms)
{
    return wait_for_flag(I2Cx, I2C_SR1_RXNE, timeout_ms);
}

/* 等待 BTF=1 (byte transfer finished) */
static uint8_t wait_btf(I2C_TypeDef* I2Cx, uint32_t timeout_ms)
{
    return wait_for_flag(I2Cx, I2C_SR1_BTF, timeout_ms);
}

/* ---------------- SWRST 恢复辅助函数 ---------------- */

static void i2c_master_periph_config(I2C_TypeDef* I2Cx, uint32_t clkHz);

/* SWRST 后 I2C 所有寄存器清零 (CCR/TRISE/CR2/OAR1 等).
 * 必须重新初始化外设配置才能正常通信. */
static void i2c_master_swrst_recover(I2C_TypeDef* I2Cx)
{
    I2C_GenerateSTOP(I2Cx, ENABLE);
    I2C_SoftwareResetCmd(I2Cx, ENABLE);
    I2C_SoftwareResetCmd(I2Cx, DISABLE);
    i2c_master_periph_config(I2Cx, s_master_clk);
}

/* ---------------- 公共: GPIO + 外设配置 ---------------- */

static void i2c_master_gpio_config(I2C_TypeDef* I2Cx)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_AFIO, ENABLE);

    GPIO_InitStructure.GPIO_Mode  = GPIO_Mode_AF_OD;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;

    if (I2Cx == I2C1) {
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_7;     /* PB6=SCL, PB7=SDA */
        GPIO_Init(GPIOB, &GPIO_InitStructure);
    } else if (I2Cx == I2C2) {
        GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10 | GPIO_Pin_11;   /* PB10=SCL, PB11=SDA */
        GPIO_Init(GPIOB, &GPIO_InitStructure);
    }
}

static void i2c_master_periph_config(I2C_TypeDef* I2Cx, uint32_t clkHz)
{
    I2C_InitTypeDef I2C_InitStructure;

    if (I2Cx == I2C1) {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C1, ENABLE);
    } else if (I2Cx == I2C2) {
        RCC_APB1PeriphClockCmd(RCC_APB1Periph_I2C2, ENABLE);
    }

    I2C_InitStructure.I2C_Mode                = I2C_Mode_I2C;
    I2C_InitStructure.I2C_DutyCycle           = I2C_DutyCycle_2;
    I2C_InitStructure.I2C_OwnAddress1         = 0;                  /* 主机模式 OAR1 不使用 */
    I2C_InitStructure.I2C_Ack                 = I2C_Ack_Enable;
    I2C_InitStructure.I2C_AcknowledgedAddress = I2C_AcknowledgedAddress_7bit;
    I2C_InitStructure.I2C_ClockSpeed          = clkHz;

    I2C_Init(I2Cx, &I2C_InitStructure);
    I2C_Cmd(I2Cx, ENABLE);

    /* 清除使能后可能残留的 STOPF (F103 勘误: STOPF=1 阻止 START 生成) */
    if (I2Cx->SR1 & I2C_SR1_STOPF) {
        (void)I2Cx->SR1;
        I2C_Cmd(I2Cx, ENABLE);
    }

    /* 显式不开任何 I2C 中断 */
    I2C_ITConfig(I2Cx, I2C_IT_EVT | I2C_IT_BUF | I2C_IT_ERR, DISABLE);
}

void I2C_Master_Init(I2C_TypeDef* I2Cx, uint32_t clkHz)
{
    if (I2Cx == NULL) return;

    s_master_clk = clkHz;   /* 保存时钟, 供后续 SWRST 恢复 */

    /* 先彻底关闭本通道的 NVIC 中断 (防止从机模式残留中断干扰主机轮询) */
    if (I2Cx == I2C1) {
        NVIC_DisableIRQ(I2C1_EV_IRQn);
        NVIC_DisableIRQ(I2C1_ER_IRQn);
        NVIC_ClearPendingIRQ(I2C1_EV_IRQn);
        NVIC_ClearPendingIRQ(I2C1_ER_IRQn);
    } else if (I2Cx == I2C2) {
        NVIC_DisableIRQ(I2C2_EV_IRQn);
        NVIC_DisableIRQ(I2C2_ER_IRQn);
        NVIC_ClearPendingIRQ(I2C2_EV_IRQn);
        NVIC_ClearPendingIRQ(I2C2_ER_IRQn);
    }

    /* GPIO 配置 */
    i2c_master_gpio_config(I2Cx);

    /* 外设配置 */
    i2c_master_periph_config(I2Cx, clkHz);

    /* F103 勘误: 使能后 BUSY 可能错误保持为 1. 若检测到 BUSY 卡死,
     * 执行 SWRST 恢复并重新初始化外设配置. */
    if (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY)) {
        i2c_master_swrst_recover(I2Cx);
    }
}

/* ---------------- 单次发送 (START + 地址 + 数据 + STOP) ---------------- */

uint8_t I2C_Master_Transmit(I2C_TypeDef* I2Cx,
                            uint8_t addr7,
                            const uint8_t* tx, uint8_t len,
                            uint32_t timeout_ms)
{
    uint8_t i;
    uint8_t r;

    if (I2Cx == NULL || (tx == NULL && len > 0)) return I2C_M_ERR_TIMEOUT;

    /* F103 勘误: 若上电后首次 BUSY 卡死, 或上次传输残留 STOPF 阻止
     * 新 START 生成, 一律用 SWRST+重新配置 干净恢复. */
    {
        uint32_t busy_start = xTaskGetTickCount();
        while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY)) {
            if ((xTaskGetTickCount() - busy_start) > 5) {
                i2c_master_swrst_recover(I2Cx);
                break;
            }
        }
    }
    /* F103 勘误 2.14.1: 即使 BUSY=0, 残留 STOPF=1 也会阻止硬件产生 START.
     * 单纯读 SR1+写 CR1 清零不可靠 → SWRST 重新配置. */
    if (I2Cx->SR1 & I2C_SR1_STOPF) {
        i2c_master_swrst_recover(I2Cx);
    }

    /* START + 等待 SB=1 (I2C_EVENT_MASTER_MODE_SELECT) */
    I2C_GenerateSTART(I2Cx, ENABLE);
    {
        uint32_t tout = timeout_ms;
        uint32_t start_tick = xTaskGetTickCount();
        while (I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_MODE_SELECT) == ERROR) {
            /* 检测 NACK / 仲裁丢失 / 总线错 */
            if (I2Cx->SR1 & I2C_SR1_AF) { I2C_ClearFlag(I2Cx, I2C_FLAG_AF); I2C_GenerateSTOP(I2Cx, ENABLE); return I2C_M_ERR_ADDR_NACK; }
            if (I2Cx->SR1 & I2C_SR1_ARLO) { I2C_ClearFlag(I2Cx, I2C_FLAG_ARLO); I2C_GenerateSTOP(I2Cx, ENABLE); return I2C_M_ERR_START; }
            if (I2Cx->SR1 & I2C_SR1_BERR) { I2C_ClearFlag(I2Cx, I2C_FLAG_BERR); I2C_GenerateSTOP(I2Cx, ENABLE); return I2C_M_ERR_START; }
            if ((xTaskGetTickCount() - start_tick) > tout) {
                I2C_GenerateSTOP(I2Cx, ENABLE);
                return I2C_M_ERR_START;
            }
        }
    }

    /* 地址 (写) — 等待 EV6 (MASTER_TRANSMITTER_MODE_SELECTED) */
    I2C_Send7bitAddress(I2Cx, addr7, I2C_Direction_Transmitter);
    {
        uint32_t tout = timeout_ms;
        uint32_t start_tick = xTaskGetTickCount();
        while (I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED) == ERROR) {
            if (I2Cx->SR1 & I2C_SR1_AF) { I2C_ClearFlag(I2Cx, I2C_FLAG_AF); I2C_GenerateSTOP(I2Cx, ENABLE); return I2C_M_ERR_ADDR_NACK; }
            if ((xTaskGetTickCount() - start_tick) > tout) {
                I2C_GenerateSTOP(I2Cx, ENABLE);
                return I2C_M_ERR_TX;
            }
        }
        /* ADDR 清除: 读 SR1 再读 SR2 */
        (void)I2Cx->SR1;
        (void)I2Cx->SR2;
    }

    /* 数据 */
    for (i = 0; i < len; i++) {
        I2C_SendData(I2Cx, tx[i]);
        if (i < (len - 1)) {
            /* 非最后一字节: 等 EV8_1 (TXE=1, 移位寄存器空, 可以装下一字节) */
            uint32_t tout = timeout_ms;
            uint32_t start_tick = xTaskGetTickCount();
            while (I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTING) == ERROR) {
                if ((xTaskGetTickCount() - start_tick) > tout) {
                    I2C_GenerateSTOP(I2Cx, ENABLE);
                    return I2C_M_ERR_TX;
                }
            }
        } else {
            /* 最后一字节: 等 EV8_2 (BTF=1, 字节已发出并收到 ACK) */
            uint32_t tout = timeout_ms;
            uint32_t start_tick = xTaskGetTickCount();
            while (I2C_CheckEvent(I2Cx, I2C_EVENT_MASTER_BYTE_TRANSMITTED) == ERROR) {
                if ((xTaskGetTickCount() - start_tick) > tout) {
                    I2C_GenerateSTOP(I2Cx, ENABLE);
                    return I2C_M_ERR_TX;
                }
            }
        }
    }

    /* STOP */
    I2C_GenerateSTOP(I2Cx, ENABLE);
    /* 等待 STOPF 置位 (硬件检测到总线 STOP 后 SR1.STOPF=1),
     * 然后软件序列清零: 读 SR1 + 写 CR1. */
    {
        uint32_t start = xTaskGetTickCount();
        while ((I2Cx->SR1 & I2C_SR1_STOPF) == 0) {
            if ((xTaskGetTickCount() - start) > 10) break;
        }
        if (I2Cx->SR1 & I2C_SR1_STOPF) {
            (void)I2Cx->SR1;
            I2C_Cmd(I2Cx, ENABLE);
        }
    }
    return I2C_M_OK;
}

/* ---------------- 单次接收 (START + 地址 + 数据 + STOP) ---------------- */

uint8_t I2C_Master_Receive(I2C_TypeDef* I2Cx,
                           uint8_t addr7,
                           uint8_t* rx, uint8_t len,
                           uint32_t timeout_ms)
{
    uint8_t i;
    uint8_t r;

    if (I2Cx == NULL || rx == NULL || len == 0) return I2C_M_ERR_TIMEOUT;

    /* 等总线空闲; 若 BUSY 卡死, SWRST+重新配置 恢复 */
    {
        uint32_t start = xTaskGetTickCount();
        while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY)) {
            if ((xTaskGetTickCount() - start) > 5) {
                i2c_master_swrst_recover(I2Cx);
                break;
            }
        }
    }

    /* 清除残留 STOPF (F103 勘误 2.14.1: STOPF=1 阻止 START 生成) */
    if (I2Cx->SR1 & I2C_SR1_STOPF) {
        i2c_master_swrst_recover(I2Cx);
    }

    /* START */
    I2C_GenerateSTART(I2Cx, ENABLE);
    r = wait_start(I2Cx, timeout_ms);
    if (r != 0) {
        I2C_GenerateSTOP(I2Cx, ENABLE);
        return (r == 2) ? I2C_M_ERR_ADDR_NACK : I2C_M_ERR_START;
    }

    /* 在 ADDR 清零前: 若 len==1 需要 ACK=0 (NACK), len>1 保持 ACK=1 */
    if (len == 1) {
        I2C_AcknowledgeConfig(I2Cx, DISABLE);
    } else {
        I2C_AcknowledgeConfig(I2Cx, ENABLE);
    }

    /* 地址 (读) */
    I2C_Send7bitAddress(I2Cx, addr7, I2C_Direction_Receiver);
    r = wait_addr(I2Cx, timeout_ms);
    if (r != 0) {
        I2C_GenerateSTOP(I2Cx, ENABLE);
        I2C_AcknowledgeConfig(I2Cx, ENABLE);
        return (r == 2) ? I2C_M_ERR_ADDR_NACK : I2C_M_ERR_RX;
    }

    for (i = 0; i < len; i++) {
        if (i == (len - 1)) {
            /* 收最后 1 字节前: 关 ACK, 准备 STOP */
            I2C_AcknowledgeConfig(I2Cx, DISABLE);
        }
        r = wait_rxne(I2Cx, timeout_ms);
        if (r != 0) {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            I2C_AcknowledgeConfig(I2Cx, ENABLE);
            return I2C_M_ERR_RX;
        }
        rx[i] = I2C_ReceiveData(I2Cx);
    }

    /* STOP */
    I2C_GenerateSTOP(I2Cx, ENABLE);
    I2C_AcknowledgeConfig(I2Cx, ENABLE);    /* 恢复默认 ACK */
    return I2C_M_OK;
}

/* ---------------- 复合传输: 不释放总线的写 + 读 ----------------
 * 用于 IPMB 主机典型时序:
 *   START + addr+W + tx... + Sr + addr+R + rx... + STOP
 * 注意: F103 的 Sr 必须先写 START=1, 待 SB=1 后再发地址.
 * --------------------------------------------------------------- */

uint8_t I2C_Master_WriteRead(I2C_TypeDef* I2Cx,
                             uint8_t addr7,
                             const uint8_t* tx, uint8_t tx_len,
                             uint8_t* rx, uint8_t rx_len,
                             uint32_t timeout_ms)
{
    uint8_t i;
    uint8_t r;

    if (I2Cx == NULL) return I2C_M_ERR_TIMEOUT;
    if ((tx == NULL || tx_len == 0) && (rx == NULL || rx_len == 0)) {
        return I2C_M_ERR_TIMEOUT;
    }

    /* 写阶段: 包含 START + 地址 + 数据, 不发 STOP */
    if (tx != NULL && tx_len > 0) {
        /* 等总线空闲; 若 BUSY 卡死, SWRST+重新配置 恢复 */
        {
            uint32_t start = xTaskGetTickCount();
            while (I2C_GetFlagStatus(I2Cx, I2C_FLAG_BUSY)) {
                if ((xTaskGetTickCount() - start) > 5) {
                    i2c_master_swrst_recover(I2Cx);
                    break;
                }
            }
        }
        /* 清除残留 STOPF (F103 勘误 2.14.1) */
        if (I2Cx->SR1 & I2C_SR1_STOPF) {
            i2c_master_swrst_recover(I2Cx);
        }
        I2C_GenerateSTART(I2Cx, ENABLE);
        r = wait_start(I2Cx, timeout_ms);
        if (r != 0) {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return (r == 2) ? I2C_M_ERR_ADDR_NACK : I2C_M_ERR_START;
        }
        I2C_Send7bitAddress(I2Cx, addr7, I2C_Direction_Transmitter);
        r = wait_addr(I2Cx, timeout_ms);
        if (r != 0) {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return (r == 2) ? I2C_M_ERR_ADDR_NACK : I2C_M_ERR_TX;
        }
        for (i = 0; i < tx_len; i++) {
            r = wait_txe(I2Cx, timeout_ms);
            if (r != 0) {
                I2C_GenerateSTOP(I2Cx, ENABLE);
                return I2C_M_ERR_TX;
            }
            I2C_SendData(I2Cx, tx[i]);
        }
        r = wait_btf(I2Cx, timeout_ms);
        if (r != 0) {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return I2C_M_ERR_TX;
        }
    }

    /* 读阶段: Sr + 地址 + 数据 + STOP */
    if (rx != NULL && rx_len > 0) {
        /* Sr */
        I2C_GenerateSTART(I2Cx, ENABLE);
        r = wait_start(I2Cx, timeout_ms);
        if (r != 0) {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            return (r == 2) ? I2C_M_ERR_ADDR_NACK : I2C_M_ERR_START;
        }
        if (rx_len == 1) {
            I2C_AcknowledgeConfig(I2Cx, DISABLE);
        } else {
            I2C_AcknowledgeConfig(I2Cx, ENABLE);
        }
        I2C_Send7bitAddress(I2Cx, addr7, I2C_Direction_Receiver);
        r = wait_addr(I2Cx, timeout_ms);
        if (r != 0) {
            I2C_GenerateSTOP(I2Cx, ENABLE);
            I2C_AcknowledgeConfig(I2Cx, ENABLE);
            return (r == 2) ? I2C_M_ERR_ADDR_NACK : I2C_M_ERR_RX;
        }
        for (i = 0; i < rx_len; i++) {
            if (i == (rx_len - 1)) {
                I2C_AcknowledgeConfig(I2Cx, DISABLE);
            }
            r = wait_rxne(I2Cx, timeout_ms);
            if (r != 0) {
                I2C_GenerateSTOP(I2Cx, ENABLE);
                I2C_AcknowledgeConfig(I2Cx, ENABLE);
                return I2C_M_ERR_RX;
            }
            rx[i] = I2C_ReceiveData(I2Cx);
        }
        I2C_GenerateSTOP(I2Cx, ENABLE);
        I2C_AcknowledgeConfig(I2Cx, ENABLE);
    } else {
        /* 写完不读, 自己发 STOP */
        I2C_GenerateSTOP(I2Cx, ENABLE);
    }

    return I2C_M_OK;
}

/* ---------------- 总线恢复 ---------------- */
void I2C_Master_BusRecover(I2C_TypeDef* I2Cx)
{
    if (I2Cx == NULL) return;
    i2c_master_swrst_recover(I2Cx);
}
