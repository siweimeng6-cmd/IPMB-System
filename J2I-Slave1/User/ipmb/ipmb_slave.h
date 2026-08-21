#ifndef __IPMB_SLAVE_H
#define __IPMB_SLAVE_H

#include "stm32f10x.h"
#include "FreeRTOS.h"
#include "task.h"
#include "ipmb_protocol.h"
#include "ipmb_sensor.h"
#include "ipmb_fru.h"

/* 设备 ID 字段 */
typedef struct {
    uint8_t  device_id;             /* 模块类型 ID (0x03) */
    uint8_t  device_revision;       /* [7]=1 提供SDR, [0:3]=硬件版本 */
    uint8_t  firmware_rev1;         /* [0:3]大版本 [4:6]小版本 [7]=1 */
    uint8_t  firmware_rev2;         /* IPMC 软件版本 BCD */
    uint8_t  ipmi_version;          /* 0x51 = 1.5 */
    uint8_t  additional_support;    /* 0x23=IPMC 0x93=CHMC */
    uint8_t  manufacturer_id[3];    /* 低字节在前 */
    uint16_t product_id;            /* 低字节在前 */
    uint8_t  slot;                  /* 业务板槽位号 (Get Slot 返回) */
} IPMB_Slave_DeviceID_t;

extern IPMB_Slave_DeviceID_t g_slave_device_id;

/* 初始化从机 (在 BSP_Init 调用) */
void IPMB_Slave_Init(void);

/* 初始化传感器表与 FRU 状态 (在 BSP_Init 调用) */
void IPMB_Slave_SensorInit(void);
void IPMB_Slave_FRUInit(void);

/* 解析请求并生成响应 (由 task_ipmb 调用)
 * @param rx_buf  收到的 IPMB 帧
 * @param rx_len  收到的帧长度 (含 2 字节校验)
 * @param tx_buf  响应帧输出缓冲
 * @param tx_max  缓冲大小
 * @param own_addr 本机 IPMB 地址 (用于响应 rsSA)
 * @return 响应帧长度, 0 表示未生成响应
 */
uint8_t IPMB_Slave_ProcessRequest(const uint8_t* rx_buf, uint8_t rx_len,
                                   uint8_t* tx_buf, uint8_t tx_max,
                                   uint8_t own_addr);

/* Read slot number (GA0..GA4 GPIO), same value as IPMB Get Slot */
uint8_t read_slot_from_gpio(void);

/* =================== Handler 函数 (供外部调试引用) =================== */
void IPMB_Slave_Handle_GetDeviceID(const uint8_t* req, uint8_t req_len,
                                     uint8_t* resp, uint8_t* resp_len,
                                     uint8_t own_addr);
void IPMB_Slave_Handle_GetDeviceSDR(const uint8_t* req, uint8_t req_len,
                                      uint8_t* resp, uint8_t* resp_len,
                                      uint8_t own_addr);
void IPMB_Slave_Handle_GetSDR(const uint8_t* req, uint8_t req_len,
                                uint8_t* resp, uint8_t* resp_len,
                                uint8_t own_addr);
void IPMB_Slave_Handle_GetSensorReading(const uint8_t* req, uint8_t req_len,
                                          uint8_t* resp, uint8_t* resp_len,
                                          uint8_t own_addr);
void IPMB_Slave_Handle_GetBoardStatus(const uint8_t* req, uint8_t req_len,
                                        uint8_t* resp, uint8_t* resp_len,
                                        uint8_t own_addr);
void IPMB_Slave_Handle_GetSlot(const uint8_t* req, uint8_t req_len,
                                uint8_t* resp, uint8_t* resp_len,
                                uint8_t own_addr);
void IPMB_Slave_Handle_SetFRUActivation(const uint8_t* req, uint8_t req_len,
                                          uint8_t* resp, uint8_t* resp_len,
                                          uint8_t own_addr);
void IPMB_Slave_Handle_PlatformEvent(const uint8_t* req, uint8_t req_len,
                                       uint8_t* resp, uint8_t* resp_len,
                                       uint8_t own_addr);
void IPMB_Slave_Handle_GetModuleInfo(const uint8_t* req, uint8_t req_len,
                                      uint8_t* resp, uint8_t* resp_len,
                                      uint8_t own_addr);
void IPMB_Slave_Handle_GetBoardIdentity(const uint8_t* req, uint8_t req_len,
                                          uint8_t* resp, uint8_t* resp_len,
                                          uint8_t own_addr);
void IPMB_Slave_Handle_SetBoardIdentity(const uint8_t* req, uint8_t req_len,
                                          uint8_t* resp, uint8_t* resp_len,
                                          uint8_t own_addr);
void IPMB_Slave_Handle_GetThresholdConfig(const uint8_t* req, uint8_t req_len,
                                          uint8_t* resp, uint8_t* resp_len,
                                          uint8_t own_addr);
void IPMB_Slave_Handle_SetThresholdConfig(const uint8_t* req, uint8_t req_len,
                                          uint8_t* resp, uint8_t* resp_len,
                                          uint8_t own_addr);

/* =================== FreeRTOS 任务入口 (定义在 task_ipmb.c) =================== */
void IPMB_A_Task(void* parameter);
void IPMB_B_Task(void* parameter);
extern TaskHandle_t IPMB_A_TaskHandle;
extern TaskHandle_t IPMB_B_TaskHandle;

/* =================== IPMB-A/B 总线仲裁 (A优先, 故障切B, A恢复自动切回) ===================
 * IPMB_A_EN/IPMB_B_EN 两路 buffer 使能 GPIO 始终保持高电平不变, 仲裁只在软件/协议层生效:
 * 非当前活跃总线收到的请求不会被处理、也不会回复有效响应. */
typedef enum { IPMB_BUS_A = 0, IPMB_BUS_B } IPMB_Bus_t;
extern volatile IPMB_Bus_t g_ipmb_active_bus;

void IPMB_Arbitration_Task(void* parameter);
extern TaskHandle_t IPMB_Arbitration_TaskHandle;

#endif


