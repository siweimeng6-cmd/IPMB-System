#include "bsp_init.h"

uint8_t borad_bpd20550_current[2]={0x00,0x00};
uint16_t	int_bpd20550_current ;
uint32_t	int_bpd20550_voltage ;
float g_bpd20550_current_A = 0.0f;
volatile uint8_t g_bpd20550_current_valid = 0;

__IO uint32_t  I2CTimeout = I2CT_LONG_TIMEOUT; 


#if	MO_IIC
/*
*********************************************************************************************************
*	函 数 名: bpd20550_start_operation
*	功能说明: 启动BPD20550操作，指令0x01
*	形    参: 
*	返 回 值: 
*********************************************************************************************************
*/

uint8_t bpd20550_start_operation(void)
{
	/* 第1步：发起I2C总线启动信号 */
	i2c_Start();

	/* 第2步：发起控制字节，高7bit是地址，bit0是读写控制位，0表示写，1表示读 */
	i2c_SendByte(BPD20550_ADDRESS | I2C_WR);	/* 此处是写指令 */
	
		/* 第3步：发送ACK */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	
	}

	/* 第4步：发送字节地址，24C02只有256字节，因此1个字节就够了，如果是24C04以上，那么此处需要连发多个地址 */
		i2c_SendByte(BPD20550_OPERATION);
	
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应答 */
	}
	i2c_Stop();
	
		cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设备 */
	/* 发送I2C总线停止信号 */
	i2c_Stop();
	return 0;


}
/*
*********************************************************************************************************
*	函 数 名: bpd20550_current_ReadBytes(模拟I2C读取数据)
*	功能说明: 从温度传感器的首地址读取两个字节的数据
*	形    参: _usSize : 数据长度,单位为字节
*			        _pReadBuf : 存放读到的数据的缓冲区指针
*	返 回 值: 0 表示失败,1表示成功
*********************************************************************************************************
*/
uint8_t bpd20550_ReadBytes(uint8_t *_pReadBuf, uint8_t command ,uint16_t _usSize)
{
	uint16_t i;
	/* 第1步：发起I2C总线启动信号 */
	i2c_Start();

	/* 第2步：发起控制字节，高7bit是地址，bit0是读写控制位，0表示写，1表示读 */
	i2c_SendByte(BPD20550_ADDRESS | I2C_WR);	/* 此处是写指令 */

	/* 第3步：发送ACK */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	
	}

	/* 第4步：发送指令 */
		i2c_SendByte(command);
		if (i2c_WaitAck() != 0)
		{
			goto cmd_fail;	/* EEPROM器件无应答 */
		}
	/* 第6步：重新启动I2C总线。下面开始读取数据 */
	i2c_Start();

	/* 第7步：发起控制字节，高7bit是地址，bit0是读写控制位，0表示写，1表示读 */
	i2c_SendByte(BPD20550_ADDRESS | I2C_RD);	/* 此处是读指令 */

	/* 第8步：发送ACK */
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	/* EEPROM器件无应答 */
	}

	/* 第9步：循环读取数据 */
	for (i = 0; i < _usSize; i++)
	{
		_pReadBuf[i] = i2c_ReadByte();	/* 读1个字节 */

		/* 每读完1个字节后，需要发送Ack， 最后一个字节不需要Ack，发Nack */
		if (i != _usSize - 1)
		{
			i2c_Ack();	/* 中间字节读完后，CPU产生ACK信号(驱动SDA = 0) */
		}
		else
		{
			i2c_NAck();	/* 最后1个字节读完后，CPU产生NACK信号(驱动SDA = 1) */
		}
	}
	/* 发送I2C总线停止信号 */
	i2c_Stop();
	return 1;	/* 执行成功 */

	cmd_fail: /* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设备 */
	/* 发送I2C总线停止信号 */
	i2c_Stop();
	return 0;
}


#else
void Init_HARD_I2C(void)
{
  GPIO_InitTypeDef  GPIO_InitStructure; 

	/* 使能与 I2C 有关的时钟 */
	COM_I2C_EPD20550_APBxClock_FUN ( COM_I2C_EPD20550_CLK, ENABLE );
	COM_I2C_EPD20550_GPIO_APBxClock_FUN ( COM_I2C_EPD20550_GPIO_CLK, ENABLE );
	
    
  /* I2C_SCL、I2C_SDA*/
  GPIO_InitStructure.GPIO_Pin = COM_I2C_EPD20550_SCL_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;	       // 开漏输出
  GPIO_Init(COM_I2C_EPD20550_SCL_PORT, &GPIO_InitStructure);
	
  GPIO_InitStructure.GPIO_Pin = COM_I2C_EPD20550_SDA_PIN;
  GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
  GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_OD;	       // 开漏输出
  GPIO_Init(COM_I2C_EPD20550_SDA_PORT, &GPIO_InitStructure);	
	
	
}

/*
*********************************************************************************************************
*	函 数 名: bpd20550_start_operation
*	功能说明: 启动BPD20550操作，指令0x01
*	形    参: 
*	返 回 值: 
*********************************************************************************************************
*/

uint8_t bpd20550_start_operation(void)
{
	/* 第1步：发起I2C总线启动信号 */
  I2CTimeout = I2CT_LONG_TIMEOUT;
  while(I2C_GetFlagStatus(COM_I2C3, I2C_FLAG_BUSY))  
   {
    if((I2CTimeout--) == 0) goto cmd_fail;
  } 
  I2C_GenerateSTART(COM_I2C3, ENABLE);
	
	
	/* 第2步：发起控制字节，高7bit是地址，bit0是读写控制位，0表示写，1表示读 */
	I2CTimeout = I2CT_LONG_TIMEOUT;	
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_MODE_SELECT))							//EV5
  {
    if((I2CTimeout--) == 0) 		
			{
			printf("start ack over time\r\n");
			goto cmd_fail;
		
}
  } 
  I2C_Send7bitAddress(COM_I2C3, BPD20550_ADDRESS, I2C_Direction_Transmitter); //Send EEPROM address for write 

	/* 第3步：发送字节地址 */
	I2CTimeout = I2CT_LONG_TIMEOUT;
	while(!I2C_CheckEvent(COM_I2C3,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))// EV6
  {
    if((I2CTimeout--) == 0) 
		{
			printf("address send ack over time\r\n");
			goto cmd_fail;
		}
  } 
  I2C_SendData(COM_I2C3, BPD20550_OPERATION); //发送寄存器地址,BPD20550_OPERATION操作指令0x01
	

	I2CTimeout = I2CT_LONG_TIMEOUT;
  while(!I2C_CheckEvent(COM_I2C3,I2C_EVENT_MASTER_BYTE_TRANSMITTED))						//EV8  
   {
    if((I2CTimeout--) == 0) goto cmd_fail;
  } 
  I2C_GenerateSTOP(COM_I2C3, ENABLE); 
	return 1;
	cmd_fail: 			/* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设备 */	
	/* 发送I2C总线停止信号 */
  I2C_ClearFlag(COM_I2C3, I2C_FLAG_AF);  
  I2C_GenerateSTOP(COM_I2C3, ENABLE); 
	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: bpd20550_current_ReadBytes(模拟I2C读取数据)
*	功能说明: 从温度传感器的首地址读取两个字节的数据
*	形    参: _usSize : 数据长度,单位为字节
*			        _pReadBuf : 存放读到的数据的缓冲区指针
*	返 回 值: 0 表示失败,1表示成功
*********************************************************************************************************
*/
uint8_t bpd20550_ReadBytes(uint8_t *_pReadBuf, uint8_t command ,uint16_t _usSize)
{
	/* 第1步：发起I2C总线启动信号 */
  I2CTimeout = I2CT_LONG_TIMEOUT;
  while(I2C_GetFlagStatus(COM_I2C3, I2C_FLAG_BUSY))  
   {
    if((I2CTimeout--) == 0) goto cmd_fail;
  } 
  I2C_GenerateSTART(COM_I2C3, ENABLE);
	
	
	/* 第2步：发起控制字节，高7bit是地址，bit0是读写控制位，0表示写，1表示读 */
	I2CTimeout = I2CT_LONG_TIMEOUT;	
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_MODE_SELECT))							//EV5
  {
    if((I2CTimeout--) == 0)
		{
			printf("start ack over time\r\n");
			goto cmd_fail;
		}
  } 
  I2C_Send7bitAddress(COM_I2C3, BPD20550_ADDRESS, I2C_Direction_Transmitter); //Send EEPROM address for write 

	/* 第3步：发送字节地址 */
	I2CTimeout = I2CT_LONG_TIMEOUT;
	while(!I2C_CheckEvent(COM_I2C3,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))// EV6
  {
    if((I2CTimeout--) == 0) 
		{
			printf("address send ack over time\r\n");
			goto cmd_fail;
		}
  } 
  I2C_SendData(COM_I2C3, command); //发送寄存器地址,BPD20550_READ_CURRENT操作指令0x8C
	//BPD20550_READ_VOLTAGE_OUT			//BPD20550_READ_CURRENT
	
	/* 第4步：重新启动I2C总线。下面开始读取数据 */
	I2CTimeout = I2CT_LONG_TIMEOUT;
  while(!I2C_CheckEvent(COM_I2C3,I2C_EVENT_MASTER_BYTE_TRANSMITTED))						//EV8  
   {
    if((I2CTimeout--) == 0) 
		{
			printf("read current ack over time\r\n");
			goto cmd_fail;
		}	
  } 
  I2C_GenerateSTART(COM_I2C3, ENABLE);
	

	/* 第5步：发起控制字节，高7bit是地址，bit0是读写控制位，0表示写，1表示读 */
	I2CTimeout = I2CT_LONG_TIMEOUT;	
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_MODE_SELECT))
  {
    if((I2CTimeout--) == 0) 
			{
			printf("start ack over time\r\n");
			goto cmd_fail;
		}	
  } 
  I2C_Send7bitAddress(COM_I2C3, BPD20550_ADDRESS,I2C_Direction_Receiver); // 此处是读指令


	/* 第6步：循环读取数据 */
	I2CTimeout = I2CT_LONG_TIMEOUT;	
	while(!I2C_CheckEvent(COM_I2C3,I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
	{
    if((I2CTimeout--) == 0) 
		{
			printf("address send ack over time\r\n");
			goto cmd_fail;
		}	
  } 
	
	while(_usSize)
    {
        if(_usSize == 1)//只剩下最后一个数据时进入 if 语句
        { 
					 I2C_AcknowledgeConfig(COM_I2C3,DISABLE); //最后有一个数据时关闭应答位
					 I2C_GenerateSTOP(COM_I2C3,ENABLE);    //最后一个数据时使能停止位
        }
        if(I2C_CheckEvent(COM_I2C3,I2C_EVENT_MASTER_BYTE_RECEIVED))//读取数据
        { 
					 *_pReadBuf = I2C_ReceiveData(COM_I2C3);//调用库函数将数据取出到 Buffer
					 _pReadBuf++; //指针移位
					 _usSize--; //字节数减 1 
        }
    }
  I2C_AcknowledgeConfig(COM_I2C3,ENABLE);
	return 1;				/* 执行成功 */

	cmd_fail: 			/* 命令执行失败后，切记发送停止信号，避免影响I2C总线上其他设备 */	
	/* 发送I2C总线停止信号 */
  I2C_ClearFlag(COM_I2C3, I2C_FLAG_AF);  
  I2C_GenerateSTOP(COM_I2C3, ENABLE); 
	return 0;
}
#endif

/*
*********************************************************************************************************
*	函 数 名: VRLinear11Format
*	功能说明: BPD20550电流值计算，值保存*pOut
*	形    参: uint16_t RAWData	BPD20550寄存器读取值
						float *pOut				计算结果输出
*	返 回 值: 
*********************************************************************************************************
*/
int VRLinear11Format(uint16_t RAWData, float *pOut)
{
	float RealValue=0;
	int Y=0;
	int N=0;

	if(NULL==pOut)
	{
		return -1;
	}

	N = (RAWData>>11) & 0x1F;
	Y = RAWData & 0x07FF;

	if(N>>4)
	{
		if(0x10 == (N & 0x1F)) //xulf bug zen475:when N=0x10000, do not ~N+1 due to ~(10000)+1 = 00000 
		{
			//TODO:Nothing
		}
		else
		{
			N = (~N + 1 )&(0x0F);
		}
		RealValue= (1.0*Y)/(1<<N);
	}
	else
	{
		RealValue= Y*(1<<N);
	}

	*pOut=RealValue;

	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: Board_ADDR90_temp
*	功能说明: 监测地址为0x90的温度芯片
*	形    参: 
*********************************************************************************************************
*/
void Board_BPD20550_current(void)
{
		float cal_bpd20550_current;
		float cal_bpd20550_voltage;
		float cal_bpd20550_temp;
		bpd20550_start_operation();
	
		if(bpd20550_ReadBytes(borad_bpd20550_current,BPD20550_READ_CURRENT, 2)==1)
		{
			//printf("current %d，%d\r\n",borad_bpd20550_current[0],borad_bpd20550_current[1]);
			VRLinear11Format(borad_bpd20550_current[0]+(borad_bpd20550_current[1]<<8),&cal_bpd20550_current);
			//printf("current float:%f\r\n",cal_bpd20550_current);
			int_bpd20550_current = (uint16_t)cal_bpd20550_current;//cal_bpd20550_current浮点数没有小数点后数据，直接取整
			g_bpd20550_current_A = cal_bpd20550_current;
			g_bpd20550_current_valid = 1;
			printf("BPD20550 current:%.2fA\r\n",cal_bpd20550_current);
		}
		else{
			printf("BPD20550 current read failed.\r\n");
			int_bpd20550_current = 0xFFFF;
			g_bpd20550_current_valid = 0;
		}
		if(bpd20550_ReadBytes(borad_bpd20550_current,BPD20550_READ_VOLTAGE_IN, 2)==1)
		{
			VRLinear11Format(borad_bpd20550_current[0]+(borad_bpd20550_current[1]<<8),&cal_bpd20550_voltage);
			//int_bpd20550_voltage = (uint32_t)(cal_bpd20550_voltage*100);//电流乘100强制取整
			printf("BPD20550 voltage in:%.2f V\r\n",cal_bpd20550_voltage);
		}
		if(bpd20550_ReadBytes(borad_bpd20550_current,BPD20550_READ_TEMP, 2)==1)
		{
			VRLinear11Format(borad_bpd20550_current[0]+(borad_bpd20550_current[1]<<8),&cal_bpd20550_temp);
			//int_bpd20550_voltage = (uint32_t)(cal_bpd20550_voltage*100);//电流乘100强制取整
			printf("BPD20550 TEMP :%.2f C\r\n",cal_bpd20550_temp);
		}
		
		
}
