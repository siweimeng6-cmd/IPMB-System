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

#endif
