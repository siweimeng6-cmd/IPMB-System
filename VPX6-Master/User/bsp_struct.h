#ifndef __BSP_STRUCT_H__
#define __BSP_STRUCT_H__

#define UART_SEND_BUF_SIZE     	 512
#define FLASH_BUF_SIZE     		 1024
	
typedef enum{
	FALSE = 0,
	TRUE  = 1
}emBOOL;

typedef struct
{
	unsigned short size;
	unsigned char  buf[UART_SEND_BUF_SIZE];
}stSEND_BUF_t, *pstSEND_BUF_t;

typedef struct
{
	unsigned short size;
	unsigned char  buf[FLASH_BUF_SIZE];
}stFLASH_BUF_t, *pstFLASH_BUF_t;

typedef enum
{
	STA_ERROR = 0,
	STA_OK    = 1,
}emSTATUS_t;

typedef enum
{
	HDMI_EXTERN = 0,
	HDMI_INTER    = 1,
	DVO    = 2,
}emHDMI_STATUS_t;

typedef enum
{
	SELECT_SYSLOG 		= 0,
	SELECT_STM32LOG   = 1,
}emLOG_SELECT_t;

typedef struct
{
	emSTATUS_t Fan_1;
	emSTATUS_t Fan_2;
}stFAN_STATUS_t;

typedef struct
{
	float  Voltage_12V;
	float  Voltage_5V;
	float  Voltage_3V3;
	float  Voltage_VDDC;
	float  Voltage_LS7A_VDD;
	float  Voltage_DIMM_1V5;
	float  Voltage_CPU0;
	float  CUR_MT9221;
}stADC_t;

typedef struct
{
	float  Temp_1_Val;
	float  Temp_2_Val;
	float  Temp_3_Val;	
}stTEMP_MODULE_VAL_t;

typedef struct
{
	emSTATUS_t  Temp_1;
	emSTATUS_t  Temp_2;
	emSTATUS_t  Temp_3;
}stTEMP_MODULE_STATUS_t;

typedef struct
{
	unsigned char   Sys_Power;
	unsigned char   BL_Power;
	unsigned char   S3_Power;
	unsigned char   video_led_Power;
	unsigned char	ec_mute_Power;
	
	unsigned char   BL_PWM_Duty;
	unsigned char   Fan_PWM_Duty;
	unsigned char   Beep_PWM_Duty; //new add
	
	stFAN_STATUS_t  Fan_Status;
	stADC_t                stADC_Val;
	
  stTEMP_MODULE_VAL_t    stTemp_Val;
	stTEMP_MODULE_STATUS_t stTemp_Module_Sta;
	
	unsigned char   BL_Set_Duty; /* 0~100 */
	
}stSYS_STATUS_t, *pstSYS_STATUS_t;


extern stSYS_STATUS_t  stSys_Status;
extern stFLASH_BUF_t   stFlash_Buf;
extern stFLASH_BUF_t 	 stCpu_Buf;

#endif 