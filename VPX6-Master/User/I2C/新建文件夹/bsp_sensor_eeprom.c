#include "bsp_i2c.h"

static __IO uint32_t  I2CTimeout = I2CT_LONG_TIMEOUT; 
int board_temp0[2]={0x00,0x00};           //存储整数化后的实际温度值
int board_temp1[2]={0x00,0x00};           //存储整数化后的实际温度值
int board_temp2[2]={0x00,0x00};           //存储整数化后的实际温度值

uint8_t board_lm75a_temp0[2]={0x00,0x00}; //温度采集数字化存储寄存器
uint8_t board_lm75a_temp1[2]={0x00,0x00}; //温度采集数字化存储寄存器
uint8_t board_lm75a_temp2[2]={0x00,0x00}; //温度采集数字化存储寄存器
/*																						
*********************************************************************************************************
*	函 数 名: I2C_EE_BufferWrite
*	功能说明: 将缓冲区中的数据写到I2C EEPROM中
*	形    参：pBuffer:缓冲区指针
						WriteAddr:写地址
						NumByteToWrite:写的字节数
*	返 回 值: 无
*********************************************************************************************************
*/
void I2C_EE_BufferWrite(u8* pBuffer, u8 WriteAddr, u16 NumByteToWrite)
{
  u8 NumOfPage = 0, NumOfSingle = 0, Addr = 0, count = 0;

  Addr = WriteAddr % I2C_PageSize;
  count = I2C_PageSize - Addr;
  NumOfPage =  NumByteToWrite / I2C_PageSize;
  NumOfSingle = NumByteToWrite % I2C_PageSize;
 
  /* If WriteAddr is I2C_PageSize aligned  */
  if(Addr == 0) 
  {
    /* If NumByteToWrite < I2C_PageSize */
    if(NumOfPage == 0) 
    {
      I2C_EE_PageWrite(pBuffer, WriteAddr, NumOfSingle);
      I2C_EE_WaitEepromStandbyState();
    }
    /* If NumByteToWrite > I2C_PageSize */
    else  
    {
      while(NumOfPage--)
      {
        I2C_EE_PageWrite(pBuffer, WriteAddr, I2C_PageSize); 
        I2C_EE_WaitEepromStandbyState();
        WriteAddr +=  I2C_PageSize;
        pBuffer += I2C_PageSize;
      }

      if(NumOfSingle!=0)
      {
        I2C_EE_PageWrite(pBuffer, WriteAddr, NumOfSingle);
        I2C_EE_WaitEepromStandbyState();
      }
    }
  }
  /* If WriteAddr is not I2C_PageSize aligned  */
  else 
  {
    /* If NumByteToWrite < I2C_PageSize */
    if(NumOfPage== 0) 
    {
      I2C_EE_PageWrite(pBuffer, WriteAddr, NumOfSingle);
      I2C_EE_WaitEepromStandbyState();
    }
    /* If NumByteToWrite > I2C_PageSize */
    else
    {
      NumByteToWrite -= count;
      NumOfPage =  NumByteToWrite / I2C_PageSize;
      NumOfSingle = NumByteToWrite % I2C_PageSize;	
      
      if(count != 0)
      {  
        I2C_EE_PageWrite(pBuffer, WriteAddr, count);
        I2C_EE_WaitEepromStandbyState();
        WriteAddr += count;
        pBuffer += count;
      } 
      
      while(NumOfPage--)
      {
        I2C_EE_PageWrite(pBuffer, WriteAddr, I2C_PageSize);
        I2C_EE_WaitEepromStandbyState();
        WriteAddr +=  I2C_PageSize;
        pBuffer += I2C_PageSize;  
      }
      if(NumOfSingle != 0)
      {
        I2C_EE_PageWrite(pBuffer, WriteAddr, NumOfSingle); 
        I2C_EE_WaitEepromStandbyState();
      }
    }
  }  
}


/*																						
*********************************************************************************************************
*	函 数 名: I2C_TIMEOUT_UserCallback
*	功能说明:  Basic management of the timeout situation.
*	形    参：errorCode：错误代码，可以用来定位是哪个环节出错.
*	返 回 值: 返回0，表示IIC读取失败
*********************************************************************************************************
*/
static  uint32_t I2C_TIMEOUT_UserCallback(uint8_t errorCode)
{
  /* Block communication and all processes */
  EEPROM_ERROR("I2C 等待超时!errorCode = %d",errorCode);
  
  return 0;
}

/*																						
*********************************************************************************************************
*	函 数 名: I2C_EE_BufferWrite
*	功能说明: 写一个字节到I2C EEPROM中
*	形    参：pBuffer:缓冲区指针
						WriteAddr:写地址
*	返 回 值: 无
*********************************************************************************************************
*/
uint32_t I2C_EE_ByteWrite(uint8_t* pBuffer, uint8_t WriteAddr)
{
  /* Send STRAT condition */
  I2C_GenerateSTART(COM_I2C3, ENABLE);

  I2CTimeout = I2CT_FLAG_TIMEOUT;

  /* Test on EV5 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_MODE_SELECT))
  {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(0);
  }    

  /* Send EEPROM address for write */
  I2C_Send7bitAddress(COM_I2C3, EEPROM_ADDRESS, I2C_Direction_Transmitter);
  
  
  I2CTimeout = I2CT_FLAG_TIMEOUT;
  /* Test on EV6 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))
  {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(1);
  }    
      
  /* Send the EEPROM's internal address to write to */
  I2C_SendData(COM_I2C3, WriteAddr);
  
  I2CTimeout = I2CT_FLAG_TIMEOUT;

  /* Test on EV8 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED))  
  {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(2);
  } 
  /* Send the byte to be written */
  I2C_SendData(COM_I2C3, *pBuffer); 
   
  I2CTimeout = I2CT_FLAG_TIMEOUT;

  /* Test on EV8 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
  {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(3);
  } 
  
  /* Send STOP condition */
  I2C_GenerateSTOP(COM_I2C3, ENABLE);
  
  return 1;
}



/*																						
*********************************************************************************************************
*	函 数 名: I2C_EE_PageWrite
*	功能说明: 在EEPROM的一个写循环中可以写多个字节，但一次写入的字节数不能超过EEPROM页的大小，AT24C02每页有8个字节
*	形    参：pBuffer:缓冲区指针
						WriteAddr:写地址
						NumByteToWrite:写的字节数
*	返 回 值: 无
*********************************************************************************************************
*/
uint32_t I2C_EE_PageWrite(uint8_t* pBuffer, uint8_t WriteAddr, uint8_t NumByteToWrite)
{
  I2CTimeout = I2CT_LONG_TIMEOUT;

  while(I2C_GetFlagStatus(COM_I2C3, I2C_FLAG_BUSY))  
   {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(4);
  } 
  
  /* Send START condition */
  I2C_GenerateSTART(COM_I2C3, ENABLE);
  
  
  I2CTimeout = I2CT_FLAG_TIMEOUT;

  /* Test on EV5 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_MODE_SELECT))
  {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(5);
  } 
  
  /* Send EEPROM address for write */
  I2C_Send7bitAddress(COM_I2C3, EEPROM_ADDRESS, I2C_Direction_Transmitter);
  
  I2CTimeout = I2CT_FLAG_TIMEOUT;

  /* Test on EV6 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) 
  {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(6);
  } 
  /* Send the EEPROM's internal address to write to */    
  I2C_SendData(COM_I2C3, WriteAddr);  

  I2CTimeout = I2CT_FLAG_TIMEOUT;

  /* Test on EV8 and clear it */
  while(! I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED)) 
  {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(7);
  } 
  /* While there is data to be written */
  while(NumByteToWrite--)  
  {
    /* Send the current byte */
    I2C_SendData(COM_I2C3, *pBuffer); 

    /* Point to the next byte to be written */
    pBuffer++; 
  
    I2CTimeout = I2CT_FLAG_TIMEOUT;

    /* Test on EV8 and clear it */
    while (!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(8);
    } 
  }

  /* Send STOP condition */
  I2C_GenerateSTOP(COM_I2C3, ENABLE);
  
  return 1;
}


/*																						
*********************************************************************************************************
*	函 数 名: I2C_EE_BufferRead
*	功能说明: 从EEPROM里面读取一块数据 
*	形    参：pBuffer:存放从EEPROM读取的数据的缓冲区指针
						WriteAddr:接收数据的EEPROM的地址
						NumByteToWrite:要从EEPROM读取的字节数
*	返 回 值: 无
*********************************************************************************************************
*/
uint32_t I2C_EE_BufferRead(uint8_t* pBuffer, uint8_t ReadAddr, uint16_t NumByteToRead)
{  
    I2CTimeout = I2CT_LONG_TIMEOUT;

  //*((u8 *)0x4001080c) |=0x80; 
    while(I2C_GetFlagStatus(COM_I2C3, I2C_FLAG_BUSY))   
    {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(9);
    }
  /* Send START condition */
  I2C_GenerateSTART(COM_I2C3, ENABLE);
  //*((u8 *)0x4001080c) &=~0x80;
  
  I2CTimeout = I2CT_FLAG_TIMEOUT;

  /* Test on EV5 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_MODE_SELECT))
  {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(10);
   }

  /* Send EEPROM address for write */
  I2C_Send7bitAddress(COM_I2C3, EEPROM_ADDRESS, I2C_Direction_Transmitter);

  I2CTimeout = I2CT_FLAG_TIMEOUT;
 
  /* Test on EV6 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED)) 
    {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(11);
   }
  /* Clear EV6 by setting again the PE bit */
  I2C_Cmd(COM_I2C3, ENABLE);

  /* Send the EEPROM's internal address to write to */
  I2C_SendData(COM_I2C3, ReadAddr);  

     I2CTimeout = I2CT_FLAG_TIMEOUT;

  /* Test on EV8 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_BYTE_TRANSMITTED))
    {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(12);
   }
  /* Send STRAT condition a second time */  
  I2C_GenerateSTART(COM_I2C3, ENABLE);
  
     I2CTimeout = I2CT_FLAG_TIMEOUT;

  /* Test on EV5 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_MODE_SELECT))
    {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(13);
   }
  /* Send EEPROM address for read */
  I2C_Send7bitAddress(COM_I2C3, EEPROM_ADDRESS, I2C_Direction_Receiver);
  
     I2CTimeout = I2CT_FLAG_TIMEOUT;

  /* Test on EV6 and clear it */
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
    {
    if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(14);
   }
  /* While there is data to be read */
  while(NumByteToRead)  
  {
    if(NumByteToRead == 1)
    {
      /* Disable Acknowledgement */
      I2C_AcknowledgeConfig(COM_I2C3, DISABLE);
      
      /* Send STOP Condition */
      I2C_GenerateSTOP(COM_I2C3, ENABLE);
    }

		
		I2CTimeout = I2CT_LONG_TIMEOUT;
		while(I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_BYTE_RECEIVED)==0)  
		{
			if((I2CTimeout--) == 0) return I2C_TIMEOUT_UserCallback(3);
		} 	
		{
		  /* Read a byte from the device */
      *pBuffer = I2C_ReceiveData(COM_I2C3);

      /* Point to the next location where the byte read will be saved */
      pBuffer++; 
      
      /* Decrement the read bytes counter */
      NumByteToRead--;
		}			
  }

  /* Enable Acknowledgement to be ready for another reception */
  I2C_AcknowledgeConfig(COM_I2C3, ENABLE);
  
  return 1;
}


/*																						
*********************************************************************************************************
*	函 数 名: I2C_EE_WaitEepromStandbyState
*	功能说明:  Wait for EEPROM Standby state 
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void I2C_EE_WaitEepromStandbyState(void)      
{
  uint16_t SR1_Tmp = 0;
	//I2CTimeout = I2CT_LONG_TIMEOUT;//这个延时删掉会导致无法读写故障
  do
  {
		//I2CTimeout--;
    /* Send START condition */
    I2C_GenerateSTART(COM_I2C3, ENABLE);
    /* Read COM_I2C3 SR1 register */
    SR1_Tmp = I2C_ReadRegister(COM_I2C3, I2C_Register_SR1);
    /* Send EEPROM address for write */
    I2C_Send7bitAddress(COM_I2C3, EEPROM_ADDRESS, I2C_Direction_Transmitter);
  }while(!(I2C_ReadRegister(COM_I2C3, I2C_Register_SR1) & 0x0002));
  
  /* Clear AF flag */
  I2C_ClearFlag(COM_I2C3, I2C_FLAG_AF);
  /* STOP condition */    
  I2C_GenerateSTOP(COM_I2C3, ENABLE); 
}




/*																						
*********************************************************************************************************
*	函 数 名: eeprom_test
*	功能说明:  EEPROM读写测试
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void eeprom_test(void)
{
	uint16_t i;
	uint8_t I2c_Buf_Write[256];
	uint8_t I2c_Buf_Read[256];
	EEPROM_INFO("write data:\r\n"); 
	for ( i=0; i<=255; i++ ) //填充缓冲
  {   
    I2c_Buf_Write[i] = i;
    //printf("0x%02X ", I2c_Buf_Write[i]);
    //if(i%16 == 15)    
        //printf("\n\r");    
   }
	//将I2c_Buf_Write中顺序递增的数据写入EERPOM中 
	I2C_EE_BufferWrite( I2c_Buf_Write, EEP_Firstpage, 256);
	 
	 EEPROM_INFO("write success:\r\n");   
	 EEPROM_INFO("read data:\r\n");
	//将EEPROM读出数据顺序保持到I2c_Buf_Read中
	I2C_EE_BufferRead(I2c_Buf_Read, EEP_Firstpage, 256); 
	 
	   //将I2c_Buf_Read中的数据通过串口打印
	for (i=0; i<256; i++)
	{	
		if(I2c_Buf_Read[i] != I2c_Buf_Write[i])
		{
			printf("0x%02X ", I2c_Buf_Read[i]);
			EEPROM_ERROR("error:I2C EEPROM read data and send data not same");
		}
    printf("0x%02X ", I2c_Buf_Read[i]);
    if(i%16 == 15)    
        printf("\n\r");
    
	}
  EEPROM_INFO("I2C(AT24C02)read test success\r\n");
}

/*
*********************************************************************************************************
*	函 数 名: board_lm75a_temp0_ReadBytes(模拟I2C读取数据)
*	功能说明: 从温度传感器的首地址读取两个字节的数据
*	形    参:  _usAddress : 起始地址
*			          _usSize : 数据长度,单位为字节
*			        _pReadBuf : 存放读到的数据的缓冲区指针
*	返 回 值: 0 表示失败,1表示成功
*********************************************************************************************************
*/
uint8_t board_lm75a_temp_ReadBytes(uint8_t *_pReadBuf, uint16_t _usAddress, uint16_t _usSize, uint8_t board_Lm75a_Address)
{
	/* 第1步：发起I2C总线启动信号 */
  I2CTimeout = I2CT_LONG_TIMEOUT;
  while(I2C_GetFlagStatus(COM_I2C3, I2C_FLAG_BUSY))  
   {
    if((I2CTimeout--) == 0) goto cmd_fail;
  } 
  I2C_GenerateSTART(COM_I2C3, ENABLE);
	
	
	/* 第2步：发起控制字节，高7bit是地址，bit0是读写控制位，0表示写，1表示读 */
	I2CTimeout = I2CT_FLAG_TIMEOUT;	
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_MODE_SELECT))							//EV5
  {
    if((I2CTimeout--) == 0) goto cmd_fail;
  } 
  I2C_Send7bitAddress(COM_I2C3, board_Lm75a_Address, I2C_Direction_Transmitter); //Send EEPROM address for write 

	/* 第3步：发送字节地址 */
	I2CTimeout = I2CT_FLAG_TIMEOUT;
	while(!I2C_CheckEvent(COM_I2C3,I2C_EVENT_MASTER_TRANSMITTER_MODE_SELECTED))// EV6
  {
    if((I2CTimeout--) == 0) goto cmd_fail;
  } 
  I2C_SendData(COM_I2C3, _usAddress); //发送寄存器地址
	
	
	/* 第4步：重新启动I2C总线。下面开始读取数据 */
	I2CTimeout = I2CT_FLAG_TIMEOUT;
  while(!I2C_CheckEvent(COM_I2C3,I2C_EVENT_MASTER_BYTE_TRANSMITTED))						//EV8  
   {
    if((I2CTimeout--) == 0) goto cmd_fail;
  } 
  I2C_GenerateSTART(COM_I2C3, ENABLE);
	

	/* 第5步：发起控制字节，高7bit是地址，bit0是读写控制位，0表示写，1表示读 */
	I2CTimeout = I2CT_FLAG_TIMEOUT;	
  while(!I2C_CheckEvent(COM_I2C3, I2C_EVENT_MASTER_MODE_SELECT))
  {
    if((I2CTimeout--) == 0) goto cmd_fail;
  } 
  I2C_Send7bitAddress(COM_I2C3, board_Lm75a_Address,I2C_Direction_Receiver); // 此处是读指令


	/* 第6步：循环读取数据 */
	I2CTimeout = I2CT_FLAG_TIMEOUT;	
	while(!I2C_CheckEvent(COM_I2C3,I2C_EVENT_MASTER_RECEIVER_MODE_SELECTED))
	{
    if((I2CTimeout--) == 0) goto cmd_fail;
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

/*
*********************************************************************************************************
*	函 数 名: get_two_complement
*	功能说明: 求补码的源码(得到的值可能为负值)
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
uint16_t get_two_complement(uint16_t t_b)
{
	 int i = 0;
	 int tmp[11] = {0};
	 uint16_t r_bin = 0;
	 for(i=0;i<11;i++)
	 {
		   tmp[i] = (t_b>>i)%2;
		   if(tmp[i] == 0)
		       tmp[i] = 1;
		   else
		       tmp[i] = 0;
		  r_bin |= (tmp[i]<<i);
	 }
	 r_bin += 1;
	 return r_bin;
}

/*
*********************************************************************************************************
*	函 数 名: temp_calculate
*	功能说明: 将温度换算成实际数值
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
float temp_calculate(uint8_t buf[2])
{
	  uint16_t t_cpu		= 0;
	  float temp				= 0;
	
  	t_cpu	=((buf[0]<<8)|(buf[1]))>>5;//去除低五位
  	if(((t_cpu>>10)&1) == 0)//判断温度正负值
		         temp	= ((float)t_cpu)*0.125;
	  else if(((t_cpu>>10)&1) == 1)
			{
		      t_cpu	= get_two_complement(t_cpu);
		      temp	= -((float)t_cpu)*0.125;
	    }
	   return temp;
}

/*
*********************************************************************************************************
*	函 数 名: decimal_2_integer
*	功能说明: 将ADC采集的电压信息整数化
*	形    参: 无
*	返 回 值: 无
*********************************************************************************************************
*/
int decimal_2_integer(float a,int b[2])
{
	float temp = 0.0;
	if(a<0){
		   a		= -a;
		   b[0]	= (int)(a);
		   temp	= a-b[0];
		   temp	*= 100;
		   b[1]	= (int)(temp);
		   b[0]	= -b[0];
		}
	else if(a>=0){
		   b[0]	= (int)(a);
		   temp	= a-b[0];
		   temp	*= 100;
		   b[1]	= (int)(temp);
	}
	return 0;
}

/*
*********************************************************************************************************
*	函 数 名: Board_ADDR90_temp
*	功能说明: 监测地址为0x90的温度芯片
*	形    参: 
*********************************************************************************************************
*/
void Board_ADDR90_temp(void)
{
		float temp;
		if(board_lm75a_temp_ReadBytes((uint8_t *)board_lm75a_temp0,0,2,BOARD_LM75A90_ADDRESS)==1)//温度传感器读取成功
		{
			temp = temp_calculate(board_lm75a_temp0); //采集转换成实际温度
			decimal_2_integer(temp,board_temp0); //整数化 	
		}
		else
		{
			printf("ADDR90 TEMP read failed.\r\n");
		}
}

/*
*********************************************************************************************************
*	函 数 名: Board_ADDR90_temp
*	功能说明: 监测地址为0x90的温度芯片
*	形    参: 
*********************************************************************************************************
*/
void Board_ADDR92_temp(void)
{
		float temp;
		if(board_lm75a_temp_ReadBytes((uint8_t *)board_lm75a_temp1,0,2,BOARD_LM75A92_ADDRESS)==1)//温度传感器读取成功
		{
			temp = temp_calculate(board_lm75a_temp1); //采集转换成实际温度
			decimal_2_integer(temp,board_temp1); //整数化 	
		}
		else
		{
			printf("ADDR92 TEMP read failed.\r\n");
		}
}




