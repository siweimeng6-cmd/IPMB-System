#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "bsp_i2c_slave.h"
#include "bsp_i2c_master.h"
#include "bsp_i2c.h"
#include "ipmb_slave.h"
#include "ipmb_protocol.h"
#include "bsp_init.h"
#include <stdio.h>
#include <string.h>

/* 应答推送等待事件的超时: 与 IPMB_Master_Task/bsp_i2c_master.c 中
 * 其它主机侧调用保持一致 */
#define IPMB_TX_PUSH_TIMEOUT_MS   50

/* =================== IPMB-A/B 总线仲裁 =================== */

/* 上电默认优先 IPMB-A. 判定依据: g_i2c_slave_a.last_active_tick 是否在
 * IPMB_FAILOVER_TIMEOUT_MS 内更新过 (ISR 中无条件更新, 见 bsp_i2c_slave.c) */
volatile IPMB_Bus_t g_ipmb_active_bus = IPMB_BUS_A;
TaskHandle_t IPMB_Arbitration_TaskHandle = NULL;

/* 主机对 IPMB-A 的轮询间隔并不均匀(忙的时候几十ms一条, 空闲时可能隔好几秒),
 * 原来的 3000ms 太紧, 会在主机正常但轮询变慢时反复触发 A->B->A 翻转刷屏。
 * 放宽到 10000ms, 覆盖主机实际最慢的轮询间隔, 只有真正掉线才判超时。
 * 注: g_ipmb_active_bus 目前只用于日志, 不参与收发决策(请求一律在收到它的
 * 那条总线上直接回复, 见下方 IPMB_A_Task 里的说明), 所以放宽不影响功能。 */
#define IPMB_FAILOVER_TIMEOUT_MS   10000

void IPMB_Arbitration_Task(void* parameter)
{
    (void)parameter;

    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(200));

        {
            uint32_t now = xTaskGetTickCount();
            uint8_t a_recent = (uint8_t)((now - g_i2c_slave_a.last_active_tick) < pdMS_TO_TICKS(IPMB_FAILOVER_TIMEOUT_MS));

            if (g_ipmb_active_bus == IPMB_BUS_A && !a_recent) {
                /* A 超时未见合法事务, 切到 B; 清空 A 侧残留响应防止读到过期数据 */
                g_ipmb_active_bus = IPMB_BUS_B;
                I2C_Slave_SetTxData(&g_i2c_slave_a, NULL, 0);
                printf("[IPMB-ARB] A timeout, switch to B\r\n");
            } else if (g_ipmb_active_bus == IPMB_BUS_B && a_recent) {
                /* A 恢复, 自动切回 */
                g_ipmb_active_bus = IPMB_BUS_A;
                printf("[IPMB-ARB] A recovered, switch back to A\r\n");
            }
        }
    }
}

/* =================== IPMB-A (I2C1) 任务 =================== */

TaskHandle_t IPMB_A_TaskHandle = NULL;
TaskHandle_t IPMB_B_TaskHandle = NULL;

void IPMB_A_Task(void* parameter)
{
    uint8_t rx_buf[IPMB_MAX_FRAME_LEN];
    uint8_t tx_buf[IPMB_MAX_FRAME_LEN];
    uint8_t rx_len, tx_len;
    static uint8_t s_last_pem_seq_a = 0;   /* 上次看到的 g_pem_event_seq,变了就自主推一条 PEM */

    (void)parameter;
    printf("[IPMB-A] Task started, addr=0x%02X\r\n", g_i2c_slave_a.slave_addr);

    while (1) {
        /* 等待中断通知: 主机发完一帧 (STOP 检测到)。改用有限等待(而不是
         * portMAX_DELAY)定期醒来,超时时顺便检查有没有 PEM 事件要自主推送——
         * 这段检查和处理请求分支一样只在本任务里跑,天然独占 I2C1,不需要锁。 */
        if (xI2C1_Semaphore == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (xSemaphoreTake(xI2C1_Semaphore, pdMS_TO_TICKS(100)) != pdTRUE) {
            if (g_pem_event_seq != s_last_pem_seq_a) {
                s_last_pem_seq_a = g_pem_event_seq;
                IPMB_Slave_Handle_PlatformEvent(NULL, 0, tx_buf, &tx_len, g_i2c_slave_a.slave_addr);
                if (tx_len > 0) {
                    uint8_t tx_ret;

                    I2C_Master_Init(I2C1, g_ipmb_i2c_speed);
                    tx_ret = I2C_Master_Transmit(I2C1, IPMB_ADDR_HOST, tx_buf, tx_len,
                                                  IPMB_TX_PUSH_TIMEOUT_MS);
                    if (tx_ret != I2C_M_OK) {
                        printf("[IPMB-A] PEM push err=%u\r\n", tx_ret);
                    } else {
                        printf("[IPMB-A] PEM push (autonomous) %u bytes, seq=%u\r\n",
                               tx_len, s_last_pem_seq_a);
                    }
                    I2C_Slave_Resume(I2C1);
                    if (g_ipmb_i2c_speed_pending != 0) {   /* 这次推送已经发完,提交速率变更 */
                        g_ipmb_i2c_speed = g_ipmb_i2c_speed_pending;
                        g_ipmb_i2c_speed_pending = 0;
                        printf("[FRU] I2C speed committed: %luHz\r\n", (unsigned long)g_ipmb_i2c_speed);
                    }
                }
            }
            continue;
        }

        /* 取走接收到的数据 */
        rx_len = I2C_Slave_GetRxData(&g_i2c_slave_a, rx_buf, sizeof(rx_buf));
        if (rx_len == 0) continue;

        /* 校验和 */
        if (IPMB_Verify_Checksum(rx_buf, rx_len) != 0) {
            printf("[IPMB-A] CS error, drop %u bytes\r\n", rx_len);
            continue;
        }

        printf("[IPMB-A] RX cmd=0x%02X rqSeq=%u len=%u\r\n",
               rx_buf[5], IPMB_Extract_RqSeq(rx_buf), rx_len);

        /* 从机对寻址到自己、校验通过的请求, 一律在收到它的这条总线上直接回复,
         * 不再由 g_ipmb_active_bus(活跃总线仲裁)决定答不答。
         * 原先的 "非活跃总线就丢弃" 会在仲裁任务(200ms 轮询 + 3s 迟滞)尚未切到
         * 本总线的窗口里把刚收到的请求扔掉(只打印 RX、无 TX resp), 与主机侧
         * 独立的失败切换相位追逐, 导致 A/B 直连时 A 路持续失败。见修复方案。 */

        /* 协议分发 + 生成响应 */
        tx_len = IPMB_Slave_ProcessRequest(rx_buf, rx_len, tx_buf,
                                            sizeof(tx_buf),
                                            g_i2c_slave_a.slave_addr);
        if (tx_len > 0) {
            uint8_t tx_ret;
            /* 回写目标地址:响应帧必须真正 push 到主机监听的物理 I2C 地址 0x20,
             * 而不是原样把 rqSA(rx_buf[3])当成 I2C 目标地址去 write。
             * 主机侧除了真实地址 0x20 外,还会用若干"标记值"(比如 0x00=地址发现探测,
             * 0x01=网页轮询, 0x05=网页控制台异步控制通道等)冒充 rqSA 发请求,
             * 目的是让响应帧内容里回显的 buf[0] 带上这个标记、方便主机自己按 buf[0]
             * 把响应分流到不同的专属处理队列——但这些标记值都不是总线上真实存在的
             * I2C 从机地址,如果直接拿来 write 会因为无人应答而收不到任何响应。
             * 所以这里的规则改为:只有 rqSA 确实等于真实主机地址 0x20 时才原样使用,
             * 其余任何值(0x00/0x01/0x05/...)一律强制改写成真实主机地址 0x20——
             * 响应帧内容(含 buf[0]的原始标记值)仍由 ProcessRequest 按 rqSA 原样构好,
             * 主机据此做分流的逻辑不受影响,只是把"投递地址"和"帧内容里的标记值"
             * 彻底分开处理,不再要求两者相等。 */
            uint8_t tx_target = (rx_buf[3] == IPMB_ADDR_HOST) ? rx_buf[3] : IPMB_ADDR_HOST;

            I2C_Slave_SetTxData(&g_i2c_slave_a, tx_buf, tx_len);

            /* 主动切主机把应答帧 write 回请求方, 而不是被动等对方来读:
             * 与 IPMB-B 对称, 见 task_ipmb.c 头部说明 */
            I2C_Master_Init(I2C1, g_ipmb_i2c_speed);
            tx_ret = I2C_Master_Transmit(I2C1, tx_target, tx_buf, tx_len,
                                          IPMB_TX_PUSH_TIMEOUT_MS);
            if (tx_ret != I2C_M_OK) {
                printf("[IPMB-A] TX push err=%u\r\n", tx_ret);
            }
            I2C_Slave_Resume(I2C1);
            if (g_ipmb_i2c_speed_pending != 0) {   /* 这次响应已经发完,提交速率变更(见ipmb_fru.c case 0x10/0x11) */
                g_ipmb_i2c_speed = g_ipmb_i2c_speed_pending;
                g_ipmb_i2c_speed_pending = 0;
                printf("[FRU] I2C speed committed: %luHz\r\n", (unsigned long)g_ipmb_i2c_speed);
            }

            printf("[IPMB-A] TX resp %u bytes\r\n", tx_len);
        }
    }
}

/* =================== IPMB-B (I2C2) 任务 =================== */

void IPMB_B_Task(void* parameter)
{
    uint8_t rx_buf[IPMB_MAX_FRAME_LEN];
    uint8_t tx_buf[IPMB_MAX_FRAME_LEN];
    uint8_t rx_len, tx_len;
#if !IPMB_A_AS_MASTER
    static uint8_t s_last_pem_seq_b = 0;   /* 上次看到的 g_pem_event_seq,见 IPMB_A_Task 同款注释 */
#endif

    (void)parameter;
    printf("[IPMB-B] Task started, addr=0x%02X\r\n", g_i2c_slave_b.slave_addr);

    while (1) {
#if !IPMB_A_AS_MASTER
        if (xI2C2_Semaphore == NULL) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (xSemaphoreTake(xI2C2_Semaphore, pdMS_TO_TICKS(100)) != pdTRUE) {
            if (g_pem_event_seq != s_last_pem_seq_b) {
                s_last_pem_seq_b = g_pem_event_seq;
                IPMB_Slave_Handle_PlatformEvent(NULL, 0, tx_buf, &tx_len, g_i2c_slave_b.slave_addr);
                if (tx_len > 0) {
                    uint8_t tx_ret;

                    I2C_Master_Init(I2C2, g_ipmb_i2c_speed);
                    tx_ret = I2C_Master_Transmit(I2C2, IPMB_ADDR_HOST, tx_buf, tx_len,
                                                  IPMB_TX_PUSH_TIMEOUT_MS);
                    if (tx_ret != I2C_M_OK) {
                        printf("[IPMB-B] PEM push err=%u\r\n", tx_ret);
                    } else {
                        printf("[IPMB-B] PEM push (autonomous) %u bytes, seq=%u\r\n",
                               tx_len, s_last_pem_seq_b);
                    }
                    I2C_Slave_Resume(I2C2);
                    if (g_ipmb_i2c_speed_pending != 0) {
                        g_ipmb_i2c_speed = g_ipmb_i2c_speed_pending;
                        g_ipmb_i2c_speed_pending = 0;
                        printf("[FRU] I2C speed committed: %luHz\r\n", (unsigned long)g_ipmb_i2c_speed);
                    }
                }
            }
            continue;
        }
#else
        /* 自测构建(IPMB_A_AS_MASTER=1):IPMB_Master_Task 仍要用旧的
         * "轮询 tx_len + 主机读回"路径验证 IPMB-B,这里不抢主模式、
         * 不做自主推送,保持原有 portMAX_DELAY 阻塞等待。 */
        if (xI2C2_Semaphore == NULL ||
            xSemaphoreTake(xI2C2_Semaphore, portMAX_DELAY) != pdTRUE) {
            continue;
        }
#endif

        rx_len = I2C_Slave_GetRxData(&g_i2c_slave_b, rx_buf, sizeof(rx_buf));
        if (rx_len == 0) continue;

        if (IPMB_Verify_Checksum(rx_buf, rx_len) != 0) {
            printf("[IPMB-B] CS error, drop %u bytes\r\n", rx_len);
            continue;
        }

        printf("[IPMB-B] RX cmd=0x%02X rqSeq=%u len=%u\r\n",
               rx_buf[5], IPMB_Extract_RqSeq(rx_buf), rx_len);

        /* 与 IPMB_A_Task 对称: 收到寻址到自己、校验通过的请求即在本总线直接回复,
         * 不再由活跃总线仲裁 gate, 消除请求被丢弃的窗口。 */

        tx_len = IPMB_Slave_ProcessRequest(rx_buf, rx_len, tx_buf,
                                            sizeof(tx_buf),
                                            g_i2c_slave_b.slave_addr);
        if (tx_len > 0) {
            I2C_Slave_SetTxData(&g_i2c_slave_b, tx_buf, tx_len);

#if !IPMB_A_AS_MASTER
            /* 主动切主机把应答帧 write 回请求方, 而不是被动等对方来读。
             * 仅在非自测模式下这样做: IPMB_A_AS_MASTER=1 时 IPMB_Master_Task
             * 仍要用旧的"轮询 tx_len + 主机读回"路径来验证 IPMB-B,
             * 这里抢着切主机会打断它。
             * 回写目标同 IPMB_A_Task: 只有 rqSA==真实主机地址0x20 才原样使用,
             * 其余任何标记值(0x00/0x01/0x05/...)一律改写到 0x20,详见上面
             * IPMB_A_Task 里的完整说明。 */
            {
                uint8_t tx_ret;
                uint8_t tx_target = (rx_buf[3] == IPMB_ADDR_HOST) ? rx_buf[3] : IPMB_ADDR_HOST;

                I2C_Master_Init(I2C2, g_ipmb_i2c_speed);
                tx_ret = I2C_Master_Transmit(I2C2, tx_target, tx_buf, tx_len,
                                              IPMB_TX_PUSH_TIMEOUT_MS);
                if (tx_ret != I2C_M_OK) {
                    printf("[IPMB-B] TX push err=%u\r\n", tx_ret);
                }
                I2C_Slave_Resume(I2C2);
                if (g_ipmb_i2c_speed_pending != 0) {
                    g_ipmb_i2c_speed = g_ipmb_i2c_speed_pending;
                    g_ipmb_i2c_speed_pending = 0;
                    printf("[FRU] I2C speed committed: %luHz\r\n", (unsigned long)g_ipmb_i2c_speed);
                }
            }
#endif

            printf("[IPMB-B] TX resp %u bytes\r\n", tx_len);
        }
    }
}


