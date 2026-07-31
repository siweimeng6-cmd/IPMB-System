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

/* ==================== 自动阈值检测门限(占位值!)====================
 * 仓库里没有任何地方写过这几路传感器的真实报警/恢复门限,下面数值是
 * 临时占位,仅用于把"自动检测→自主推送 PEM"这条链路跑通。接入前请
 * 按实际硬件规格改成真实值。每路都做迟滞(assert 门限 != clear 门限),
 * 避免读数在门限附近抖动时反复触发。 */
#define IPMB_TH_TEMP_ASSERT_C         70      /* 90Temp/92Temp: >=70C 报警 */
#define IPMB_TH_TEMP_CLEAR_C          65      /*                <=65C 恢复 */
#define IPMB_TH_V12_LOW_ASSERT        10.8f   /* V12: <10.8V 或 >13.2V 报警(±10%窗口) */
#define IPMB_TH_V12_LOW_CLEAR         11.0f
#define IPMB_TH_V12_HIGH_ASSERT       13.2f
#define IPMB_TH_V12_HIGH_CLEAR        13.0f
#define IPMB_TH_V081_LOW_ASSERT       0.675f  /* V0.81: ±10%窗口(仍按旧0.75V占位值, 未按0.81V重新计算, 见下方说明) */
#define IPMB_TH_V081_LOW_CLEAR        0.69f
#define IPMB_TH_V081_HIGH_ASSERT      0.825f
#define IPMB_TH_V081_HIGH_CLEAR       0.81f
#define IPMB_TH_IOUT_ASSERT_A         36.0f   /* Iout(12V/40A 路): >=36A 报警 */
#define IPMB_TH_IOUT_CLEAR_A          34.0f   /*                   <=34A 恢复 */

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

/* 真正的自动阈值检测:对 90Temp/92Temp/V12/V0.81/Iout 五路做迟滞比较,
 * 0→非0 跳变时触发一次 IPMB_Sensor_Notify_Threshold(assert=1),
 * 非0→0 跳变时触发一次 assert=0,由 task_usart.c 的 Sensor_Task 每 2 秒调用一次。 */
void IPMB_Sensor_CheckThresholds(void);

#endif


