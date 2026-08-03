#ifndef __BSP_MO_I2C_H
#define	__BSP_MO_I2C_H

#include "stm32f4xx.h"
#include <stdbool.h>
#include <stdio.h>

/****************************I2C3************************************/
//I2C3_SDA:PC9*******I2C3_SCL:PA8

#define MO_I2C_SCL_PIN                  GPIO_Pin_8                
#define MO_I2C_SCL_GPIO_PORT            GPIOA                      
#define MO_I2C_SCL_GPIO_CLK             RCC_AHB1Periph_GPIOA
#define MO_I2C_SCL_SOURCE               GPIO_PinSource8


#define MO_I2C_SDA_PIN                  GPIO_Pin_9                  
#define MO_I2C_SDA_GPIO_PORT            GPIOC                       
#define MO_I2C_SDA_GPIO_CLK             RCC_AHB1Periph_GPIOC
#define MO_I2C_SDA_SOURCE               GPIO_PinSource9


/* 定义读写SCL和SDA的宏，已增加代码的可移植性和可阅读性 */
#define I2C_SCL_1()  GPIO_SetBits(MO_I2C_SCL_GPIO_PORT, MO_I2C_SCL_PIN)		/* SCL = 1 */
#define I2C_SCL_0()  GPIO_ResetBits(MO_I2C_SCL_GPIO_PORT,MO_I2C_SCL_PIN)		/* SCL = 0 */

#define I2C_SDA_1()  GPIO_SetBits(MO_I2C_SDA_GPIO_PORT, MO_I2C_SDA_PIN )		/* SDA = 1 */
#define I2C_SDA_0()  GPIO_ResetBits(MO_I2C_SDA_GPIO_PORT, MO_I2C_SDA_PIN )		/* SDA = 0 */

#define I2C_SDA_READ()  GPIO_ReadInputDataBit(MO_I2C_SDA_GPIO_PORT, MO_I2C_SDA_PIN)	/* 读SDA口线状态 */
#define I2C_SCL_READ()  GPIO_ReadInputDataBit(MO_I2C_SCL_GPIO_PORT, MO_I2C_SCL_PIN)	/* 读SDA口线状态 */

#define I2C_WR	0		/* 写控制bit */
#define I2C_RD	1		/* 读控制bit */

/*******************AT24C为系列号,后面数字为Kbit**********************************/
//#define AT24C02
//#define AT24C04
//#define AT24C128
#define AT24C512

#ifdef  AT24C02
	#define EE_MODEL_NAME		"AT24C02"
	#define EE_DEV_ADDR			0xA0  	        /* 设备地址,其中地址A2、A1、A0为器件地址 */
	#define EE_PAGE_SIZE		8			          /* 页面大小(字节) */
	#define EE_SIZE				  256			        /* 总容量(字节) */
	#define EE_ADDR_BYTES		1			          /* 地址字节个数 */
#endif

#ifdef  AT24C04
	#define EE_MODEL_NAME		"AT24C04"
	#define EE_DEV_ADDR			0xA0  	        /* 设备地址,其中地址A2、A1为器件地址,A0为0时选择执行0~255地址内容,A0为1时执行256~511地址内容 */
	#define EE_PAGE_SIZE		16			        /* 页面大小(字节) */
	#define EE_SIZE				  512			        /* 总容量(字节) */
	#define EE_ADDR_BYTES		1			          /* 地址字节个数 */
#endif

#ifdef  AT24C128
	#define EE_MODEL_NAME		"AT24C128"
	#define EE_DEV_ADDR			0xA0		        /* 设备地址,其中地址A1、A0为器件地址*/
	#define EE_PAGE_SIZE		64		         	/* 页面大小(字节) */
	#define EE_SIZE				  (16*1024)	      /* 总容量(字节) */
	#define EE_ADDR_BYTES		2			          /* 地址字节个数 */
#endif

#ifdef  AT24C512
	#define EE_MODEL_NAME		"AT24C512"
	#define EE_DEV_ADDR			0xA0		        /* 设备地址,其中地址A2、A1、A0为器件地址*/
	#define EE_PAGE_SIZE		128		  	      /* 页面大小(字节) */
	#define EE_SIZE				 (64*1024)     	  /* 总容量(字节) */
	#define EE_ADDR_BYTES		2			          /* 地址字节个数 */
#endif

/*********************温度传感器**********************************/
//LM75A的地址为7位地址	1		0		0		1 	A2	A1	A0	R/W																		
#define BOARD_LM75A90_ADDRESS  0x90  
#define BOARD_LM75A92_ADDRESS	 0x92 

#define BPD20550_ADDRESS									0x80
#define BPD20550_OPERATION								0x01
#define BPD20550_READ_CURRENT							0x8C
#define BPD20550_READ_VOLTAGE_OUT					0x8B
#define BPD20550_READ_VOLTAGE_IN					0x88
#define BPD20550_READ_TEMP								0x8D
#define BPD20550_STATE_IOUT								0x7B
#define BPD20550_STATUS_BYTE						  0x78
#define BPD20550_MFR_ID						  			0x99


void Init_MO_I2C(void);
void i2c_Start(void);
void i2c_Stop(void);
void i2c_SendByte(uint8_t _ucByte);
uint8_t i2c_ReadByte(void);
uint8_t i2c_WaitAck(void);
void i2c_Ack(void);
void i2c_NAck(void);
uint8_t  i2c_CheckDevice(uint8_t _Address);
uint8_t  ee_CheckOk(void);
uint8_t  ee_ReadBytes(uint8_t *_pReadBuf,uint16_t _usAddress,uint16_t _usSize);
uint8_t  ee_WriteBytes(uint8_t *_pWriteBuf,uint16_t _usAddress,uint16_t _usSize);
void Board_ADDR90_temp(void);
void Board_ADDR92_temp(void);
void Board_ADDR94_temp(void);
float temp_calculate(uint8_t buf[2]);
int decimal_2_integer(float a,int b[2]);
uint8_t board_lm75a_temp_ReadBytes(uint8_t *_pReadBuf, uint16_t _usAddress, uint16_t _usSize, uint8_t board_Lm75a_Address);
uint8_t eeprom_test(void);


#endif
