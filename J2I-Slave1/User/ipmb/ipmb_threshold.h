#ifndef __IPMB_THRESHOLD_H
#define __IPMB_THRESHOLD_H

#include "stm32f10x.h"

/*
*********************************************************************************************************
*	模 块 名: ipmb_threshold.c
*	功能说明: 传感器报警/断电阈值统一存储(2026-08-21新增)——温度报警上限/下限、温度断电
*	          上限/下限(摄氏度,应用于全部3路温度传感器)、电压报警百分比、电压断电百分比
*	          (应用于全部6路电压传感器,各自按自己的标称值折算),持久化进本机EEPROM
*	          (bsp_eeprom.c,地址1024起,占1页64字节)。真正的周期性阈值检测/断电触发逻辑
*	          在 ipmb_sensor.c 的 IPMB_Sensor_CheckThresholds() 里,这个文件只管配置的
*	          读写/持久化,通过新增的 IPMB_CMD_GET/SET_THRESHOLD_CONFIG(0x19/0x1A)命令
*	          读写。EEPROM未配置过时(擦除态)回退到下面的硬编码默认值。
*********************************************************************************************************
*/

#define IPMB_THRESHOLD_CONFIG_LEN   6   /* Get/Set Threshold Config 帧里携带的数据字节数,
                                            顺序:温度报警上限/下限、温度断电上限/下限、
                                            电压报警%、电压断电% */

typedef struct {
	int8_t  temp_alarm_high;       /* 温度报警上限,摄氏度 */
	int8_t  temp_alarm_low;        /* 温度报警下限,摄氏度 */
	int8_t  temp_shutdown_high;    /* 温度断电上限,摄氏度 */
	int8_t  temp_shutdown_low;     /* 温度断电下限,摄氏度 */
	uint8_t volt_alarm_percent;    /* 电压报警百分比,0~100 */
	uint8_t volt_shutdown_percent; /* 电压断电百分比,0~100 */
} IPMB_ThresholdConfig_t;

extern IPMB_ThresholdConfig_t g_threshold_config;

/* 开机调一次(bsp_init.c 里跟 BoardIdentity_Init() 放一起):从 EEPROM 读回配置,
 * 未配置过(EEPROM 擦除态)或校验和不对时回退硬编码默认值 */
void ThresholdConfig_Init(void);

/* 取当前配置,按 IPMB_THRESHOLD_CONFIG_LEN 顺序打包进 out_buf(调用方保证至少
 * IPMB_THRESHOLD_CONFIG_LEN 字节),供 Get Threshold Config 命令用 */
void ThresholdConfig_Get(uint8_t *out_buf);

/* 设置配置:校验 value_len 必须等于 IPMB_THRESHOLD_CONFIG_LEN,上限必须大于下限、
 * 百分比不超过100(只挡明显颠倒/溢出的输入,不做更细的范围限制),校验通过后更新RAM
 * 并立即写EEPROM(低频人工操作,不做批量延迟写入)。
 * 返回 0=成功,1=校验失败,2=EEPROM写入失败 */
uint8_t ThresholdConfig_Set(const uint8_t *value, uint8_t value_len);

#endif
