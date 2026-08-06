#include "bsp_eeprom.h"
#include <string.h>
#include <stdio.h>

/*
*********************************************************************************************************
*	函 数 名: eeprom_write_page
*	功能说明: 单次总线事务写入不超过一页(EEPROM_PAGE_SIZE)且不跨页的数据
*	形    参: addr:芯片内起始地址  buf:数据  len:长度(调用方保证不跨页)
*	返 回 值: 0 表示失败,1表示成功
*********************************************************************************************************
*/
static uint8_t eeprom_write_page(uint16_t addr, const uint8_t *buf, uint8_t len)
{
	uint8_t i;

	i2c_Start();

	i2c_SendByte(EEPROM_I2C_ADDR | I2C_WR);
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;
	}

	i2c_SendByte((uint8_t)(addr >> 8));		/* 地址高字节 */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;
	}

	i2c_SendByte((uint8_t)(addr & 0xFF));		/* 地址低字节 */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;
	}

	for (i = 0; i < len; i++)
	{
		i2c_SendByte(buf[i]);
		if (i2c_WaitAck() != 0)
		{
			goto cmd_fail;
		}
	}

	i2c_Stop();
	return 1;

	cmd_fail:
	i2c_Stop();
	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: eeprom_wait_write_done
*	功能说明: ACK轮询等待上一次页写入的内部写周期完成(芯片忙时会NACK控制字节)
*	形    参: 无
*	返 回 值: 0 表示超时失败,1表示写周期已完成
*********************************************************************************************************
*/
static uint8_t eeprom_wait_write_done(void)
{
	uint16_t retry;

	for (retry = 0; retry < 200; retry++)
	{
		i2c_Start();
		i2c_SendByte(EEPROM_I2C_ADDR | I2C_WR);
		if (i2c_WaitAck() == 0)
		{
			i2c_Stop();
			return 1;
		}
		i2c_Stop();
	}

	printf("[EEPROM] write cycle timeout (ACK poll)\r\n");
	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: EEPROM_WriteBytes
*	功能说明: 向EEPROM写入数据,按页边界自动切分,每页写完后等待写周期完成
*	形    参: addr:起始地址  buf:数据  len:长度
*	返 回 值: 0 表示失败,1表示成功
*********************************************************************************************************
*/
uint8_t EEPROM_WriteBytes(uint16_t addr, const uint8_t *buf, uint16_t len)
{
	uint16_t remaining = len;
	uint16_t cur_addr = addr;
	const uint8_t *cur_buf = buf;
	uint16_t space_in_page;
	uint8_t chunk;

	while (remaining > 0)
	{
		space_in_page = EEPROM_PAGE_SIZE - (cur_addr % EEPROM_PAGE_SIZE);
		chunk = (uint8_t)((remaining < space_in_page) ? remaining : space_in_page);

		if (!eeprom_write_page(cur_addr, cur_buf, chunk))
		{
			return 0;
		}
		if (!eeprom_wait_write_done())
		{
			return 0;
		}

		cur_addr += chunk;
		cur_buf += chunk;
		remaining -= chunk;
	}

	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: EEPROM_ReadBytes
*	功能说明: 从EEPROM指定地址开始顺序读取数据
*	形    参: addr:起始地址  buf:存放读到的数据的缓冲区指针  len:长度
*	返 回 值: 0 表示失败,1表示成功
*********************************************************************************************************
*/
uint8_t EEPROM_ReadBytes(uint16_t addr, uint8_t *buf, uint16_t len)
{
	uint16_t i;

	i2c_Start();

	i2c_SendByte(EEPROM_I2C_ADDR | I2C_WR);
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;
	}

	i2c_SendByte((uint8_t)(addr >> 8));
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;
	}

	i2c_SendByte((uint8_t)(addr & 0xFF));
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;
	}

	i2c_Start();		/* 重新启动,切换为读方向 */

	i2c_SendByte(EEPROM_I2C_ADDR | I2C_RD);
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;
	}

	for (i = 0; i < len; i++)
	{
		buf[i] = i2c_ReadByte();

		if (i != len - 1)
		{
			i2c_Ack();
		}
		else
		{
			i2c_NAck();
		}
	}

	i2c_Stop();
	return 1;

	cmd_fail:
	i2c_Stop();
	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: EEPROM_SelfTest
*	功能说明: 开机自检,向地址0写入固定测试图案再读回比对,验证MO_I2C总线上的EEPROM(SJ24C256, 0xA0)可正常读写
*	形    参: 无
*	返 回 值: 0 表示失败,1表示成功
*********************************************************************************************************
*/
uint8_t EEPROM_SelfTest(void)
{
	static const uint8_t s_test_pattern[16] = {
		0xA5, 0x5A, 0x3C, 0xC3, 0x01, 0x02, 0x03, 0x04,
		0x05, 0x06, 0x07, 0x08, 0xF0, 0x0F, 0x55, 0xAA
	};
	uint8_t readback[16] = {0};
	uint8_t i;

	if (!EEPROM_WriteBytes(0x0000, s_test_pattern, sizeof(s_test_pattern)))
	{
		printf("[EEPROM] self-test FAIL: write error (bus/ack)\r\n");
		return 0;
	}

	if (!EEPROM_ReadBytes(0x0000, readback, sizeof(readback)))
	{
		printf("[EEPROM] self-test FAIL: read error (bus/ack)\r\n");
		return 0;
	}

	if (memcmp(s_test_pattern, readback, sizeof(s_test_pattern)) != 0)
	{
		printf("[EEPROM] self-test FAIL: readback mismatch, got:");
		for (i = 0; i < sizeof(readback); i++)
		{
			printf(" %02X", readback[i]);
		}
		printf("\r\n");
		return 0;
	}

	printf("[EEPROM] self-test PASS (32KB SJ24C256, addr 0xA0)\r\n");
	return 1;
}
