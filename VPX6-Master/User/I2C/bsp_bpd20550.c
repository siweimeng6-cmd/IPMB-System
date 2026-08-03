#include "bsp_init.h"
#include "bsp_bpd20550.h"
#include <string.h>

uint8_t borad_bpd20550_current[2]={0x00,0x00};
uint16_t	int_bpd20550_current ;
uint32_t	int_bpd20550_voltage ;

uint8_t bpd20550_start_operation(void)
{

	i2c_Start();

	
	i2c_SendByte(BPD20550_ADDRESS | I2C_WR);	
	

	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	
	}

	i2c_SendByte(BPD20550_OPERATION);
	
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	
	}
	i2c_Stop();
	
cmd_fail: 
	i2c_Stop();
	return 0;


}

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
		if(0x10 == (N & 0x1F)) 
		{

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

uint8_t bpd20550_ReadBytes(uint8_t *_pReadBuf, uint8_t command ,uint16_t _usSize)
{
	uint16_t i;

	i2c_Start();


	i2c_SendByte(BPD20550_ADDRESS | I2C_WR);	
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	
	}


	i2c_SendByte(command);
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;	
	}

	i2c_Start();
	i2c_SendByte(BPD20550_ADDRESS | I2C_RD);	
	if (i2c_WaitAck() != 0)
	{
		goto cmd_fail;
	}

	
	for (i = 0; i < _usSize; i++)
	{
		_pReadBuf[i] = i2c_ReadByte();	/* ��1���ֽ� */

		
		if (i != _usSize - 1)
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


void Board_BPD20550_current(char *outbuf)
{
		float cal_bpd20550_current=0.0f;
		float cal_bpd20550_voltage=0.0f;
		float cal_bpd20550_temp=0.0f;
		size_t off = strlen(outbuf);

		bpd20550_start_operation();

		if(bpd20550_ReadBytes(borad_bpd20550_current,BPD20550_MFR_ID,2) == 1)
		{
			off += sprintf(outbuf + off, "BPD20550 MFR_ID:0x%02X,0x%02X\r\n",borad_bpd20550_current[0],borad_bpd20550_current[1]);
		}
		else
		{
			off += sprintf(outbuf + off, "BPD20550 MFR_ID read failed.\r\n");
		}

		if(bpd20550_ReadBytes(borad_bpd20550_current,BPD20550_READ_VOLTAGE_IN, 2)==1)
		{
			VRLinear11Format(borad_bpd20550_current[0]+(borad_bpd20550_current[1]<<8),&cal_bpd20550_voltage);
			off += sprintf(outbuf + off, "Vin: %.2f V\r\n",cal_bpd20550_voltage);
		}
		else
		{
			off += sprintf(outbuf + off, "BPD20550 voltage in read failed.\r\n");
		}

		if(bpd20550_ReadBytes(borad_bpd20550_current,BPD20550_READ_CURRENT, 2)==1)
		{
			VRLinear11Format(borad_bpd20550_current[0]+(borad_bpd20550_current[1]<<8),&cal_bpd20550_current);
			off += sprintf(outbuf + off, "Iout: %.2f A\r\n",cal_bpd20550_current);
		}
		else
		{
			off += sprintf(outbuf + off, "BPD20550 current read failed.\r\n");
		}

		if(bpd20550_ReadBytes(borad_bpd20550_current,BPD20550_READ_TEMP, 2)==1)
		{
			VRLinear11Format(borad_bpd20550_current[0]+(borad_bpd20550_current[1]<<8),&cal_bpd20550_temp);
			off += sprintf(outbuf + off, "Temp: %.2f C\r\n",cal_bpd20550_temp);
		}
		else
		{
			off += sprintf(outbuf + off, "BPD20550 temp read failed.\r\n");
		}

		(void)off;
}