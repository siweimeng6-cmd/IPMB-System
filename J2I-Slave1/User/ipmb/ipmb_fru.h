#ifndef __IPMB_FRU_H
#define __IPMB_FRU_H

#include "stm32f10x.h"
#include "ipmb_protocol.h"

/* 系统状态机 (PEM 当前/之前 state, 协议 §M0..M7 状态码) */
typedef enum {
    IPMB_STATE_M0 = 0,   /* 正常 */
    IPMB_STATE_M1 = 1,   /* 暂时性故障 */
    IPMB_STATE_M4 = 4,   /* 严重故障 (CPU 不在位) */
    IPMB_STATE_M6 = 6,   /* 关键故障 */
    IPMB_STATE_M7 = 7    /* 不可恢复 */
} IPMB_SystemState_t;

/* 状态变化原因码 (PEM EventData2[7:4]) */
typedef enum {
    IPMB_CAUSE_NORMAL        = 0x0,
    IPMB_CAUSE_THRESHOLD_HI  = 0x1,
    IPMB_CAUSE_THRESHOLD_LO  = 0x2,
    IPMB_CAUSE_POWER_ON      = 0x3,
    IPMB_CAUSE_RESET         = 0x4,
    IPMB_CAUSE_USER_REQUEST  = 0x5
} IPMB_StateChangeCause_t;

/* FRU / 风扇 / IPMC 全局状态 */
typedef struct {
    uint8_t  fru_id;                  /* FRU Device ID, 固定 0 */
    uint8_t  power_state;             /* 0=断电 1=上电 */
    uint8_t  soft_state;              /* 0=软关机 1=运行 */
    IPMB_SystemState_t sys_state;     /* M0..M7 */
    IPMB_SystemState_t prev_sys_state;
    IPMB_StateChangeCause_t cause;
    uint8_t  threshold_exceeded;      /* 0=正常 1=超限 */
    uint8_t  fan_default_level;       /* 默认 3..5 */
    uint16_t fan_max_rpm;             /* 最大转速, num = max_rpm/90 */
    uint8_t  ipmc_version;            /* BCD, x.y 大版本低4位 小版本高4位 */
} IPMB_FRU_State_t;

extern IPMB_FRU_State_t g_fru_state;

/* PEM(Platform Event Message)自主推送触发计数:每次 IPMB_FRU_HandleControl
 * 处理阈值超限(0x05)/阈值正常(0x06)就自增一次。task_ipmb.c 的 IPMB_A_Task/
 * IPMB_B_Task 各自维护一个"上次看到的值",发现变化就主动推一条 PEM 帧给主机,
 * 不用等主机轮询。手动测试命令(阈值超限/正常)和以后真实的阈值检测器
 * (ipmb_sensor.c)都走这同一条 HandleControl 通路,天然共用这个触发点。 */
extern volatile uint8_t g_pem_event_seq;

/* IPMB 总线速率(control_req 0x10/0x11 可切换)。g_ipmb_i2c_speed 是当前生效值,
 * task_ipmb.c 的 I2C_Master_Init()/bsp_i2c_slave.c 的 I2C_Slave_Mode_Config()
 * 都读它。g_ipmb_i2c_speed_pending 是"预约值",非0表示有一次变更还没提交——
 * 命令处理时只写pending,不直接改当前值,等这次响应真正发完(task_ipmb.c 里
 * I2C_Slave_Resume()之后)才提交成正式值,避免这次响应本身用错速率发出去。 */
extern volatile uint32_t g_ipmb_i2c_speed;
extern volatile uint32_t g_ipmb_i2c_speed_pending;

/* 初始化全局 FRU 状态 */
void IPMB_FRU_Init(void);

/* Set FRU Activation 控制子命令处理
 * 返回 0=成功, 0xFF=不支持的控制请求
 */
uint8_t IPMB_FRU_HandleControl(uint8_t control_req, const uint8_t* data, uint8_t data_len);

#endif


