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

uint32_t g_runtime_total_minutes = 0;
uint32_t g_runtime_countdown_minutes = RUNTIME_SAVE_INTERVAL_MIN;

static uint32_t s_base_minutes = 0;        /* 开机时从EEPROM读到的历史累计分钟数 */
static uint32_t s_elapsed_seconds = 0;     /* 本次开机已运行秒数(Sensor_Task每次+2) */
static uint32_t s_last_saved_minutes = 0;  /* 本次开机上一次写EEPROM时的已运行分钟数 */

/*
*********************************************************************************************************
*	函 数 名: Runtime_Init
*	功能说明: 开机时调用一次,从EEPROM读取历史累计运行时间(分钟),作为本次计时的起点
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void Runtime_Init(void)
{
	uint32_t stored = 0;

	if (EEPROM_ReadBytes(RUNTIME_EE_ADDR, (uint8_t *)&stored, sizeof(stored)))
	{
		if (stored == 0xFFFFFFFF)   /* EEPROM擦除态,视为第一次开机 */
		{
			stored = 0;
		}
	}
	else
	{
		stored = 0;
		printf("[RUNTIME] 读取EEPROM累计运行时间失败,按0开始计时\r\n");
	}

	s_base_minutes = stored;
	g_runtime_total_minutes = stored;
	g_runtime_countdown_minutes = RUNTIME_SAVE_INTERVAL_MIN;

	printf("[RUNTIME] 单片机累计运行时间: %u小时%u分钟\r\n",
		(unsigned int)(g_runtime_total_minutes / 60), (unsigned int)(g_runtime_total_minutes % 60));
}

/*
*********************************************************************************************************
*	函 数 名: Runtime_Task_Update
*	功能说明: 周期性调用(与调用者的轮询周期一致,当前为Sensor_Task每2秒调用一次),
*	         累加本次开机运行时长;每满RUNTIME_SAVE_INTERVAL_MIN分钟,把累计总时长写入EEPROM一次
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
void Runtime_Task_Update(void)
{
	uint32_t elapsed_minutes;

	s_elapsed_seconds += 2;
	elapsed_minutes = s_elapsed_seconds / 60;

	g_runtime_total_minutes = s_base_minutes + elapsed_minutes;

	if (elapsed_minutes - s_last_saved_minutes >= RUNTIME_SAVE_INTERVAL_MIN)
	{
		s_last_saved_minutes = elapsed_minutes;

		if (EEPROM_WriteBytes(RUNTIME_EE_ADDR, (uint8_t *)&g_runtime_total_minutes, sizeof(g_runtime_total_minutes)))
			printf("[RUNTIME] 累计运行时间 %u小时%u分钟 已写入EEPROM\r\n",
				(unsigned int)(g_runtime_total_minutes / 60), (unsigned int)(g_runtime_total_minutes % 60));
		else
			printf("[RUNTIME] 写入EEPROM失败\r\n");
	}

	g_runtime_countdown_minutes = RUNTIME_SAVE_INTERVAL_MIN - (elapsed_minutes - s_last_saved_minutes);
}
