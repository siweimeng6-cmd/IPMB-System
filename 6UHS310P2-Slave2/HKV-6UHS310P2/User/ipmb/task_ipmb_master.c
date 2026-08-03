#include "task_ipmb_master.h"
#include "FreeRTOS.h"
#include "task.h"
#include "bsp_i2c_master.h"
#include "bsp_i2c_slave.h"      /* g_i2c_slave_b, for slave-ready polling */
#include "ipmb_protocol.h"      /* IPMB_ADDR_DEFAULT_SLAVE, IPMB_Calc_Checksum ... */
#include "bsp_i2c.h"            /* IPMB_A_AS_MASTER */
#include <stdio.h>
#include <string.h>

/* =================== 全局 =================== */
IPMB_Master_Request_t g_ipmb_req = {0};
TaskHandle_t IPMB_Master_TaskHandle = NULL;

/* =================== 工具: 打印 hex =================== */
static void print_hex(const char* tag, const uint8_t* buf, uint8_t len)
{
    uint8_t i;
    printf("%s %uB:", tag, len);
    for (i = 0; i < len; i++) {
        printf(" %02X", buf[i]);
    }
    printf("\r\n");
}

/* =================== 响应解析 =================== */
static void decode_response(const uint8_t* req, const uint8_t* resp, uint8_t resp_len)
{
    uint8_t cmd = req[5];
    uint8_t cc;
    uint8_t j;   /* 通用循环变量 */

    if (cmd == IPMB_CMD_PEM) {
        /* PEM 轮询响应模拟"主动上报"原始帧形状,没有完成码字段,数据紧跟在
         * cmd 后面(resp[6]起),不能走下面通用的"resp[6]=cc"判断,否则会把
         * EvmRev(04h)误当成失败的完成码,直接return漏掉打印 */
        if (resp_len < 7 + 7) {
            printf("[IPMB-A Master] Platform Event: short resp\r\n");
            return;
        }
        {
            const uint8_t* d = &resp[6];
            if (d[2] == 0xFF) {
                printf("[IPMB-A Master]   Platform Event: no pending event\r\n");
            } else {
                printf("[IPMB-A Master]   Platform Event: sensorNum=0x%02X eventType=0x%02X "
                       "eventData=%02X %02X %02X\r\n", d[2], d[3], d[4], d[5], d[6]);
            }
        }
        return;
    }

    cc = resp[6];
    printf("[IPMB-A Master] CC=0x%02X\r\n", cc);
    if (cc != IPMB_CC_OK) {
        return;
    }

    switch (cmd) {
    case IPMB_CMD_GET_DEVICE_ID: {
        /* 数据 12 字节: devid rev fw1 fw2 ipmi extra manuf[3] product[2] 0 */
        if (resp_len < 7 + 12) {
            printf("[IPMB-A Master] Get Device ID: short resp\r\n");
            break;
        }
        const uint8_t* d = &resp[7];
        uint16_t product = (uint16_t)d[9] | ((uint16_t)d[10] << 8);
        uint32_t manuf   = (uint32_t)d[6] | ((uint32_t)d[7] << 8) | ((uint32_t)d[8] << 16);
        printf("[IPMB-A Master]   DeviceID=0x%02X Rev=0x%02X FW1=0x%02X FW2=0x%02X\r\n",
               d[0], d[1], d[2], d[3]);
        printf("[IPMB-A Master]   IPMI_Ver=0x%02X Support=0x%02X Manuf=0x%06X Product=0x%04X\r\n",
               d[4], d[5], manuf, product);
        break;
    }
    case IPMB_CMD_GET_SDR_INFO: {
        /* 数据: next_record_id[2] + 记录数据(最多16字节:sensorId+sensorType+sensorName[14]) */
        if (resp_len < 7 + 2) {
            printf("[IPMB-A Master] Get Device SDR: short resp\r\n");
            break;
        }
        const uint8_t* d = &resp[7];
        uint16_t next_id = (uint16_t)d[0] | ((uint16_t)d[1] << 8);
        printf("[IPMB-A Master]   Get SDR: NextRecordID=0x%04X DataLen=%u\r\n",
               next_id, (uint16_t)(resp_len - 9));
        if (resp_len > 9) {
            print_hex("[IPMB-A Master]   RecordData:", &d[2], (uint8_t)(resp_len - 9));
        }
        break;
    }
    case IPMB_CMD_GET_SDR: {
        /* 数据: next_rec[2] + record... */
        if (resp_len < 7 + 2) {
            printf("[IPMB-A Master] Get SDR: short resp\r\n");
            break;
        }
        const uint8_t* d = &resp[7];
        uint16_t next = (uint16_t)d[0] | ((uint16_t)d[1] << 8);
        printf("[IPMB-A Master]   NextRecID=0x%04X DataLen=%u\r\n",
               next, (uint16_t)(resp_len - 9));
        print_hex("[IPMB-A Master]   Data:", &d[2], (uint8_t)(resp_len - 9));
        break;
    }
    case IPMB_CMD_GET_SENSOR_READING: {
        /* 数据 3 字节: reading event[1]=80h status[1] */
        if (resp_len < 7 + 3) {
            printf("[IPMB-A Master] Get Sensor Reading: short resp\r\n");
            break;
        }
        const uint8_t* d = &resp[7];
        printf("[IPMB-A Master]   Reading=0x%02X Event=0x%02X Status=0x%02X\r\n",
               d[0], d[1], d[2]);
        break;
    }
    case IPMB_CMD_GET_BOARD_STATUS: {
        /* 数据 14 字节 (按 ipmb_slave.c 布局) */
        if (resp_len < 7 + 14) {
            printf("[IPMB-A Master] Get Board Status: short resp\r\n");
            break;
        }
        const uint8_t* d = &resp[7];
        uint16_t cpu_util = (uint16_t)d[2] | ((uint16_t)d[3] << 8);
        uint16_t mem_util = (uint16_t)d[4] | ((uint16_t)d[5] << 8);
        uint16_t bw1      = (uint16_t)d[6] | ((uint16_t)d[7] << 8);
        uint16_t bw2      = (uint16_t)d[8] | ((uint16_t)d[9] << 8);
        printf("[IPMB-A Master]   CPU_Exist=0x%02X SelfTest=0x%02X\r\n", d[0], d[1]);
        printf("[IPMB-A Master]   CPU_Util=%u%% Mem_Util=%u%%\r\n", cpu_util, mem_util);
        printf("[IPMB-A Master]   BW1=%u BW2=%u CPU_Temp=%dC OS_Ver=0x%02X FW_Ver=0x%02X\r\n",
               bw1, bw2, (int16_t)((int16_t)d[10] - 128), d[11], d[12]);
        break;
    }
    case IPMB_CMD_GET_SLOT: {
        if (resp_len < 7 + 1) {
            printf("[IPMB-A Master] Get Slot: short resp\r\n");
            break;
        }
        printf("[IPMB-A Master]   Slot=%u\r\n", resp[7]);
        break;
    }
    case IPMB_CMD_SET_FRU_ACTIVATION: {
        /* 数据 1 字节: vso */
        if (resp_len < 7 + 1) {
            printf("[IPMB-A Master] Set FRU Activation: short resp\r\n");
            break;
        }
        printf("[IPMB-A Master]   VSO=0x%02X\r\n", resp[7]);
        break;
    }
    case IPMB_CMD_GET_MODULE_INFO: {
        /* OEM Get Module Info 响应解析 */
        if (resp_len < 8) {
            printf("[IPMB-A Master] Get Module Info: short resp\r\n");
            break;
        }
        {
            uint8_t ic = resp[7];   /* item_count */
            uint8_t pos = 8;         /* 数据起始偏移 */
            printf("[IPMB-A Master] Get Module Info: item_count=%u\r\n", ic);
            for (j = 0; j < ic && (pos + 3) <= resp_len; j++) {
                uint8_t iid  = resp[pos];
                uint8_t ist  = resp[pos + 1];
                uint8_t ilen = resp[pos + 2];
                printf("[IPMB-A Master]   Item[%u]: ID=0x%02X Status=%u Len=%u\r\n",
                       j, iid, ist, ilen);
                if (ist == 0 && ilen > 0 && (pos + 3 + ilen) <= resp_len) {
                    print_hex("[IPMB-A Master]     Data:", &resp[pos + 3], ilen);
                }
                pos += 3 + ilen;
            }
        }
        break;
    }
    default:
        if (resp_len > 7) {
            print_hex("[IPMB-A Master]   Data:", &resp[7], (uint8_t)(resp_len - 7));
        }
        break;
    }
}

/* IPMB 请求对应的最小响应帧长度 (由协议规范推导),
 * 用于告诉 I2C master 期望收多少字节. 若解析出的实际帧长度更短,
 * 仍以 IPMB_Find_FrameLen 的结果为准. */
static uint8_t ipmb_master_expected_resp_len(uint8_t cmd)
{
    switch (cmd) {
    case IPMB_CMD_GET_DEVICE_ID:   return 8 + 11;   /* 头(7)+CC+data(11) = 19 */
    case IPMB_CMD_GET_SDR_INFO:    return 8 + 16;  /* next_record_id(2) + 记录最多16字节,按上限估算 */
    case IPMB_CMD_GET_SDR:         return 8 + 16;   /* 含 next_id(2) + record */
    case IPMB_CMD_GET_SENSOR_READING: return 8 + 3;
    case IPMB_CMD_GET_BOARD_STATUS: return 8 + 14;
    case IPMB_CMD_GET_SLOT:        return 8 + 1;
    case IPMB_CMD_SET_FRU_ACTIVATION: return 8 + 1;
    case IPMB_CMD_PEM:             return 7 + 7;     /* PEM轮询:模拟主动上报帧,无完成码,6头+7data+1尾校验=14 */
    case IPMB_CMD_GET_MODULE_INFO: return IPMB_MAX_FRAME_LEN;  /* 可变长度 ~116B */
    default:                       return IPMB_MAX_FRAME_LEN;
    }
}

/* =================== Master 任务 =================== */

void IPMB_Master_Task(void* parameter)
{
    uint8_t tx[IPMB_MAX_FRAME_LEN];
    uint8_t rx[IPMB_MAX_FRAME_LEN];
    uint8_t tx_len, rx_len, expected;
    uint8_t r;
    uint8_t slave_addr;
    const uint8_t self_addr = IPMB_ADDR_DEFAULT_SLAVE;   /* 主机自身 IPMB 地址 0x8E */

    (void)parameter;

    /* 任务启动 banner (I2C 初始化已由 bsp_i2c.c 的 IPMB_A_AS_MASTER 分支完成) */
    printf("[IPMB-A Master] Task entered, I2C1 master already initialized.\r\n");

    slave_addr = g_i2c_slave_b.slave_addr;  /* I2C2 从机实际配置地址 (init_ipmb_i2c 按槽位动态计算), 8-bit 格式, Send7bitAddress 直接写 DR 不做左移 */
    printf("[IPMB-A Master] Task started, host=0x%02X target=0x%02X @100KHz\r\n",
           self_addr, slave_addr);

static uint32_t master_loop_cnt = 0;
    while (1) {
        /* 没有请求则等 50ms 再查 */
        if (g_ipmb_req.pending == 0) {
            /* 每 2 秒 (40 个 50ms) 打一次心跳 */
            if ((master_loop_cnt % 40) == 0) {
                printf("[IPMB-A Master] waiting for UART4 request... (cnt=%lu)\r\n", master_loop_cnt);
            }
            master_loop_cnt++;
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        master_loop_cnt = 0;

        /* 取出请求 (临界区: 防止 UART ISR 改写) */
        taskENTER_CRITICAL();
        tx_len = g_ipmb_req.len;
        if (tx_len > IPMB_MAX_FRAME_LEN) tx_len = IPMB_MAX_FRAME_LEN;
        memcpy(tx, g_ipmb_req.buf, tx_len);
        g_ipmb_req.pending = 0;
        taskEXIT_CRITICAL();

        if (tx_len < IPMB_MIN_FRAME_LEN) {
            printf("[IPMB-A Master] RX bad frame len=%u, drop\r\n", tx_len);
            continue;
        }

        /* 按请求的 cmd 字段估算响应长度 */
        expected = ipmb_master_expected_resp_len(tx[5]);
        if (expected > IPMB_MAX_FRAME_LEN) expected = IPMB_MAX_FRAME_LEN;

        printf("[IPMB-A Master] Send req to slave 0x%02X (cmd=0x%02X, expect_resp=%uB)\r\n",
               slave_addr, tx[5], expected);
        print_hex("[IPMB-A Master] TX", tx, tx_len);

        /* 1) 主机发请求帧 (独立事务: START + W + data + STOP)
         *
         * 注意: 不能使用 I2C_Master_WriteRead 复合传输 (START+W+Sr+R+STOP),
         * 因为从机架构依赖 STOPF 中断通知 FreeRTOS 任务来准备响应数据.
         * 若中间无 STOP, 从机在 Sr 读阶段时 tx_buffer 仍为空 (0xFF).
         * 因此必须分两次独立事务: 先写 (触发 STOP → 从机任务处理),
         * 等从机准备好后再读.                                        */
        /* 诊断: 读 I2C1 硬件状态 */
        printf("[IPMB-A Master] I2C1 SR1=0x%04X SR2=0x%04X\r\n",
               (unsigned)I2C1->SR1, (unsigned)I2C1->SR2);
        r = I2C_Master_Transmit(I2C1, slave_addr, tx, tx_len, 50);
        if (r != I2C_M_OK) {
            printf("[IPMB-A Master] I2C TX err=%u (NACK/TIMEOUT)\r\n", r);
            /* 尝试恢复总线: NACK 或 START 失败都执行恢复 */
            if (r == I2C_M_ERR_START || r == I2C_M_ERR_ADDR_NACK) {
                printf("[IPMB-A Master] Attempting I2C bus recovery...\r\n");
                I2C_Master_BusRecover(I2C1);
                vTaskDelay(pdMS_TO_TICKS(100));
            }
            continue;
        }

        /* 2) 等从机处理 + 准备响应: 轮询从机 tx_len 非零, 超时 100ms
         *    代替固定延时, 避免 printf 慢速输出影响时序.               */
        {
            uint32_t poll_start = xTaskGetTickCount();
            const TickType_t poll_timeout = pdMS_TO_TICKS(100);
            while (g_i2c_slave_b.tx_len == 0) {
                if ((xTaskGetTickCount() - poll_start) > poll_timeout) {
                    printf("[IPMB-A Master] Slave response timeout (100ms)\r\n");
                    break;
                }
                vTaskDelay(pdMS_TO_TICKS(5));
            }
            if (g_i2c_slave_b.tx_len > 0) {
                printf("[IPMB-A Master] Slave ready, resp len=%u\r\n", g_i2c_slave_b.tx_len);
            }
        }

        /* 3) 如果从机 tx_len 仍为 0 (超时), 跳过读取 */
        if (g_i2c_slave_b.tx_len == 0) {
            printf("[IPMB-A Master] Skip RX, slave not ready\r\n");
            continue;
        }

        rx_len = expected;
        r = I2C_Master_Receive(I2C1, slave_addr, rx, rx_len, 50);
        if (r != I2C_M_OK) {
            printf("[IPMB-A Master] I2C RX err=%u (NACK/TIMEOUT)\r\n", r);
            continue;
        }

        /* 4) 校验 + 解析 — 尝试按 estimated len 验证,
         *    若失败, 在 rx[] 中搜索合法帧 (Find_FrameLen) */
        if (IPMB_Verify_Checksum(rx, rx_len) != 0) {
            uint8_t try_len = IPMB_Find_FrameLen(rx, rx_len);
            if (try_len == 0) {
                print_hex("[IPMB-A Master] RX (raw, CS fail)", rx, rx_len);
                printf("[IPMB-A Master] CS verify fail\r\n");
                continue;
            }
            rx_len = try_len;
        }

        print_hex("[IPMB-A Master] RX", rx, rx_len);
        decode_response(tx, rx, rx_len);

        /* 让 sensor_task 的"在线/离线"打印来得及更新 */
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
