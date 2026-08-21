#ifndef __IPMB_SENSOR_H
#define __IPMB_SENSOR_H

#include "stm32f10x.h"
#include "ipmb_protocol.h"

/* sensor 类型 (IPMI v2.0 §35.14 Table 35-14) */
#define IPMB_SENSOR_TYPE_TEMPERATURE  0x01
#define IPMB_SENSOR_TYPE_VOLTAGE      0x02
#define IPMB_SENSOR_TYPE_CURRENT      0x03
#define IPMB_SENSOR_TYPE_FAN          0x04

/* unit 编码 (IPMI v2.0 §35.16) */
#define IPMB_UNIT_UNSIGNED            0x00
#define IPMB_UNIT_DEGREES_C           0x01    /* 1°C 二进制补码 */
#define IPMB_UNIT_SIGNED              0x02
#define IPMB_UNIT_PERCENT             0x03

/* 占位值 */
#define IPMB_READING_UNAVAILABLE      0x7FFF
#define IPMB_THRESHOLD_UNAVAILABLE    0x7FFF

/* ==================== 自动阈值检测(2026-08-21接通)====================
 * 阈值不再是编译期占位常量,改成运行时可配置、EEPROM持久化的 g_threshold_config
 * (见 ipmb_threshold.h),网页可以通过 Get/Set Threshold Config(0x19/0x1A)读写。
 * 温度(3路,0x04/0x20/0x21)给绝对摄氏度上下限;电压(6路)给百分比,各自按自己的
 * 标称值折算。电流(0x07)这一路目前底层采集(Board_BPD20550_current)从未被调用、
 * 数据本身不可信,暂不纳入阈值检测。 */

/* 断电阈值需要连续多少轮(IPMB_Sensor_CheckThresholds 每被调用一次算一轮,本工程
 * 由 Sensor_Task 每2秒调用一次,即约6秒)持续超限才真正触发断电,避免传感器瞬时
 * 噪声/抖动误触发;硬编码,不做成可配置项 */
#define IPMB_THRESHOLD_SHUTDOWN_PERSIST_ROUNDS   3

typedef struct {
    uint8_t  sensor_num;        /* 协议 sensor 编号 (例如 0x03=12V) */
    uint8_t  sensor_type;       /* 温度/电压/电流/风扇 */
    uint8_t  unit_type;
    int16_t  last_reading;      /* 当前值, 0x7FFF=未采集 */
    int16_t  upper_nonrecov;
    int16_t  upper_crit;
    int16_t  upper_noncrit;
    int16_t  lower_noncrit;
    int16_t  lower_crit;
    int16_t  lower_nonrecov;
} IPMB_Sensor_Entry_t;

/* 初始化路由表 (按协议 sensor num 列表填充) */
void IPMB_Sensor_Init(void);

/* 根据 sensor_num 查表, 返回 0=成功 1=未找到 */
uint8_t IPMB_Sensor_Find(uint8_t sensor_num, IPMB_Sensor_Entry_t** out);

/* 写入 reading (供阶段2 ADC/温度采集任务调用) */
uint8_t IPMB_Sensor_Update(uint8_t sensor_num, int16_t reading);

/* 触发一次 PEM 主动上报 (供阈值超限检测调用, 阶段2 使用)。
 * 现阶段 Platform Event Message(cmd=0x02)改成"F407轮询、F103每次都实时汇报
 * g_fru_state当前真实状态"(见 IPMB_Slave_Handle_PlatformEvent),不需要单独
 * 入队,这个函数暂时用不到,留作以后如果要做真正的自动阈值检测时用。 */
void IPMB_Sensor_Notify_Threshold(uint8_t sensor_num, uint8_t event_high, uint8_t assert);

/* 真正的自动阈值检测(2026-08-21重写):对3路温度+6路电压做迟滞比较,阈值来自
 * g_threshold_config(见 ipmb_threshold.h)。报警(alarm)0→1/1→0跳变触发一次
 * IPMB_Sensor_Notify_Threshold;断电(shutdown)阈值需要连续
 * IPMB_THRESHOLD_SHUTDOWN_PERSIST_ROUNDS 轮都超限才真正触发断电,触发后锁存,
 * 直到 IPMB_Sensor_ClearShutdownLatch 被调用(从机重新上电时)才恢复检测。
 * 由 task_usart.c 的 Sensor_Task 每 2 秒调用一次。 */
void IPMB_Sensor_CheckThresholds(void);

/* 清除断电锁存 + 持续超限计数,由 ipmb_fru.c 的 case 0x01(上电)调用——手动
 * 重新上电后,阈值安全监测应该恢复生效,而不是永久失效直到MCU复位 */
void IPMB_Sensor_ClearShutdownLatch(void);

#endif


