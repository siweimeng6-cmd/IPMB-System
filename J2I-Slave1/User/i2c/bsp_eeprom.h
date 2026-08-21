#ifndef __BSP_EEPROM_H
#define __BSP_EEPROM_H

#include "bsp_mo_i2c.h"

/* SJ24C256PS, 32KB, 挂在 MO_I2C 总线(PB8=SCL, PB9=SDA)上
 * 原理图 A0/A1/A2/WP 均接地 -> 7位地址0x50, 8位控制字节: 写0xA0/读0xA1 */
#define EEPROM_I2C_ADDR                          0xA0
#define EEPROM_PAGE_SIZE                         64
#define EEPROM_SIZE_BYTES                        32768u

uint8_t EEPROM_WriteBytes(uint16_t addr, const uint8_t *buf, uint16_t len);
uint8_t EEPROM_ReadBytes(uint16_t addr, uint8_t *buf, uint16_t len);
uint8_t EEPROM_SelfTest(void);

/* 【2026-08-20移植自 BPCOME-3502】单片机累计运行时间统计:复用本文件的EEPROM读写,
 * 每满 RUNTIME_SAVE_INTERVAL_MIN 分钟把"历史累计+本次已运行"总分钟数写入EEPROM一次,
 * 断电不丢失。地址选在 128,避开 EEPROM_SelfTest() 占用的 0x0000 起 16 字节测试区,
 * 留足裕量(跟 BPCOME 的约定一致)。 */
#define RUNTIME_EE_ADDR            128
#define RUNTIME_SAVE_INTERVAL_MIN  30

extern uint32_t g_runtime_total_minutes;
extern uint32_t g_runtime_countdown_minutes;

void Runtime_Init(void);        /* 开机调一次:从EEPROM读历史累计值 */
void Runtime_Task_Update(void); /* 周期调用(Sensor_Task每2秒一次):计时、判断是否写EEPROM */

#endif
