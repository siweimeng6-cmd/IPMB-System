#include "ipmb_fru.h"
#include "bsp_init.h"
#include "bsp_usart.h"  /* powerrst_flag: 复位脉冲由 GPIO 任务异步产生, 见 bsp_gpio.c */
#include <stdio.h>
#include <string.h>

IPMB_FRU_State_t g_fru_state;
volatile uint8_t g_pem_event_seq = 0;
volatile uint32_t g_ipmb_i2c_speed = 100000;
volatile uint32_t g_ipmb_i2c_speed_pending = 0;   /* 0=无待生效变更 */

void IPMB_FRU_Init(void)
{
    g_fru_state.fru_id             = 0;
    g_fru_state.power_state        = 1;       /* 默认上电 */
    g_fru_state.soft_state         = 1;       /* 默认运行 */
    g_fru_state.sys_state          = IPMB_STATE_M0;
    g_fru_state.prev_sys_state     = IPMB_STATE_M0;
    g_fru_state.cause              = IPMB_CAUSE_NORMAL;
    g_fru_state.threshold_exceeded = 0;
    g_fru_state.fan_default_level  = 3;
    g_fru_state.fan_max_rpm        = 0;
    g_fru_state.ipmc_version       = 0x12;    /* 2.1 */
}

uint8_t IPMB_FRU_HandleControl(uint8_t control_req, const uint8_t* data, uint8_t data_len)
{
    (void)data_len;

    switch (control_req) {
    case 0x00:  /* 断电 */
        g_fru_state.prev_sys_state = g_fru_state.sys_state;
        g_fru_state.sys_state  = IPMB_STATE_M0;
        g_fru_state.cause      = IPMB_CAUSE_USER_REQUEST;
        g_fru_state.power_state = 0;
        /* 控制 GPIO: PWR_CTL 高 = 下电 */
        GPIO_SetBits(PWR_CTL_GPIO_PORT, PWR_CTL_GPIO_PIN);
        printf("[FRU] Power OFF (PA4=HIGH)\r\n");
        return 0;

    case 0x01:  /* 上电 */
        g_fru_state.prev_sys_state = g_fru_state.sys_state;
        g_fru_state.sys_state  = IPMB_STATE_M0;
        g_fru_state.cause      = IPMB_CAUSE_POWER_ON;
        g_fru_state.power_state = 1;
        /* 控制 GPIO: PWR_CTL 低 = 上电 */
        GPIO_ResetBits(PWR_CTL_GPIO_PORT, PWR_CTL_GPIO_PIN);
        printf("[FRU] Power ON (PA4=LOW)\r\n");
        return 0;

    case 0x02:  /* 复位 */
        g_fru_state.prev_sys_state = g_fru_state.sys_state;
        g_fru_state.sys_state  = IPMB_STATE_M0;
        g_fru_state.cause      = IPMB_CAUSE_RESET;
        /* 不在这里堵塞式地拉高+延时+拉低(会拖慢IPMB响应,导致F407超时):
         * 置位 powerrst_flag,交给 GPIO_Task(bsp_gpio.c 里已有的
         * "拉高->延时1s->拉低"复位脉冲逻辑)异步产生,这里立即返回 */
        powerrst_flag = 1;
        printf("[FRU] Reset requested (PA12 pulse queued to GPIO_Task)\r\n");
        return 0;

    case 0x03:  /* 软关机 — 和断电(0x00)同一套动作,额外记录 soft_state */
        g_fru_state.prev_sys_state = g_fru_state.sys_state;
        g_fru_state.sys_state  = IPMB_STATE_M0;
        g_fru_state.cause      = IPMB_CAUSE_USER_REQUEST;
        g_fru_state.power_state = 0;
        g_fru_state.soft_state = 0;
        /* 控制 GPIO: PWR_CTL 高 = 下电 */
        GPIO_SetBits(PWR_CTL_GPIO_PORT, PWR_CTL_GPIO_PIN);
        printf("[FRU] Soft shutdown -> Power OFF (PA4=HIGH), soft_state=%u\r\n", g_fru_state.soft_state);
        return 0;

    case 0x04:  /* 软复位 */
        g_fru_state.prev_sys_state = g_fru_state.sys_state;
        g_fru_state.cause      = IPMB_CAUSE_RESET;
        g_fru_state.soft_state ^= 1;
        /* 同 case 0x02: 交给 GPIO_Task 异步产生复位脉冲,不在此阻塞 */
        powerrst_flag = 1;
        printf("[FRU] Soft reset requested (PA12 pulse queued to GPIO_Task), soft_state=%u\r\n", g_fru_state.soft_state);
        return 0;

    case 0x05: {  /* 阈值超限 —— 直接更新 g_fru_state,Platform Event Message(cmd=0x02)
                   * 每次被轮询时会实时读这几个字段汇报,不需要额外入队 */
        uint8_t event_high = (data && data_len > 0) ? 1 : 0;
        g_fru_state.threshold_exceeded = 1;
        g_fru_state.prev_sys_state = g_fru_state.sys_state;
        g_fru_state.sys_state = IPMB_STATE_M1;
        g_fru_state.cause = event_high ? IPMB_CAUSE_THRESHOLD_HI : IPMB_CAUSE_THRESHOLD_LO;
        printf("[FRU] Threshold exceeded, cause=%u\r\n", g_fru_state.cause);
        g_pem_event_seq++;   /* 触发 IPMB_A_Task/B_Task 主动推一条 PEM 帧给主机 */
        return 0;
    }

    case 0x06:  /* 阈值正常 */
        g_fru_state.threshold_exceeded = 0;
        g_fru_state.prev_sys_state = g_fru_state.sys_state;
        g_fru_state.sys_state = IPMB_STATE_M0;
        g_fru_state.cause = IPMB_CAUSE_NORMAL;
        printf("[FRU] Threshold normal\r\n");
        g_pem_event_seq++;   /* 同上 */
        return 0;

    case 0x07:  /* 风扇默认档位 */
        if (data_len >= 1) {
            g_fru_state.fan_default_level = data[0];
        }
        printf("[FRU] Fan default level=%u\r\n", g_fru_state.fan_default_level);
        return 0;

    case 0x08:  /* 风扇最大转速 (低字节在前) */
        if (data_len >= 2) {
            g_fru_state.fan_max_rpm = ((uint16_t)data[1] << 8) | data[0];
        }
        printf("[FRU] Fan max rpm=%u\r\n", g_fru_state.fan_max_rpm);
        return 0;

    case 0x09:  /* IPMC 版本 */
        if (data_len >= 1) {
            g_fru_state.ipmc_version = data[0];
        }
        printf("[FRU] IPMC version=0x%02X\r\n", g_fru_state.ipmc_version);
        return 0;

    case 0x10:  /* 设置 I2C 通讯速率 100Kbps (协议控制码 10h)
                 * 只写pending,不直接改g_ipmb_i2c_speed——这次响应本身还要按当前速率
                 * 发出去,真正提交在 task_ipmb.c 里这次收发彻底结束之后 */
        g_ipmb_i2c_speed_pending = 100000;
        printf("[FRU] Set I2C rate 100K requested (will apply after this response)\r\n");
        return 0;

    case 0x11:  /* 设置 I2C 通讯速率 400Kbps (协议控制码 11h) */
        g_ipmb_i2c_speed_pending = 400000;
        printf("[FRU] Set I2C rate 400K requested (will apply after this response)\r\n");
        return 0;

    default:
        return 0xFF;
    }
}
