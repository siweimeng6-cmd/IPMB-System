#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <termios.h>

#include <fcntl.h>
#include <sys/vfs.h>
#include <getopt.h>             /* getopt_long() */
#include <errno.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "uart.h"
#include "sensor.h"
#include "sys.h"

/*
 *	Table of special characters
 */
static const struct {
	unsigned char character;
	unsigned char escape;
} characters[] = {
	{ BM_START,		0xB0 },	/* start */
	{ BM_STOP,		0xB5 },	/* stop */
	{ BM_HANDSHAKE,	0xB6 },	/* packet handshake */
	{ BM_ESCAPE,	0xBA },	/* data escape */
	{ 0x1B, 0x3B }			/* escape */
};

unsigned char revbuffer[16];

typedef struct MonitorInfoStruct
{
	char InfoName[64];
    char InfoUnit[64];       // All info converted to character
}__attribute__( ( __packed__ ) )MCU_MonitorInfo;

MCU_MonitorInfo MCU_Info[]=
{
    {"V12VSB         :", "V"},
    {"DDR1V2         :", "V"},
    {"VCORE          :", "V"},
    {"X100_VDD       :", "V"},
    {"VCC3 VDD       :", "V"},
    {"CPU1 POWER     :", "W"},
    {"CPU1 CURRENT   :", "A"},
    {"AMB1 Temp      :", "C"},
    {"AMB2 Temp      :", "C"},
    {"CPU Temp       :", "C"},
    {"GPU Temp       :", "C"},
    {"FAN1 Speed     :", "RPM"},
    {"FAN2 Speed     :", "RPM"},
    {"FAN3 Speed     :", "RPM"},
    {"FAN4 Speed     :", "RPM"},

};


unsigned char get_maxcpu_temp()
{
    FILE *fd;
    int temp;
    char buff[256];
	unsigned char value=0;
	unsigned char maxcpucore=8;
	char n[256]={0};
	char i;
	for(i=0;i<maxcpucore;i++)
	{
		memset(n,0,sizeof(n));
		snprintf(n,256,"/sys/class/hwmon/hwmon0/temp%d_input",i);
		//printf("n%d:%s\n",i,n);
		if(fd = fopen(n,"r"))
		{
			fgets(buff,sizeof(buff),fd);
			sscanf(buff, "%d", &temp);
			fclose(fd);
			if(temp/1000 > value)
				value = temp/1000;
		}

	}
	return value;

}

void monitordataprocessor(MNinfoData *mndata,unsigned char *data)
{
	memcpy ((unsigned char *)mndata, data, sizeof (MNinfoData));
}



void Display_Info(void)
{
	unsigned int data;
	if (verbose == 0)
		system("clear");
	fprintf(stderr,"MCU Version:	V%d.%02d\n",Monitorviewdata.MajorVersion,Monitorviewdata.MinorVersion);

	fprintf(stderr,"System monitor:\n");
	data = BSWAP_32(Monitorviewdata.v12vsbdata);
	fprintf(stderr,"%s %d.%03d %s\n",MCU_Info[0].InfoName,data/1000,data%1000,MCU_Info[0].InfoUnit);
	data = BSWAP_32(Monitorviewdata.ddr1v2data);
	fprintf(stderr,"%s %d.%03d %s\n",MCU_Info[1].InfoName,data/1000,data%1000,MCU_Info[1].InfoUnit);
	data = BSWAP_32(Monitorviewdata.vcoredata);
	fprintf(stderr,"%s %d.%03d %s\n",MCU_Info[2].InfoName,data/1000,data%1000,MCU_Info[2].InfoUnit);
	data = BSWAP_32(Monitorviewdata.vcc5data);
	fprintf(stderr,"%s %d.%03d %s\n",MCU_Info[3].InfoName,data/1000,data%1000,MCU_Info[3].InfoUnit);
	data = BSWAP_32(Monitorviewdata.vcc3data);
	fprintf(stderr,"%s %d.%03d %s\n",MCU_Info[4].InfoName,data/1000,data%1000,MCU_Info[4].InfoUnit);

	data = BSWAP_32(Monitorviewdata.cpu112vp);
	fprintf(stderr,"%s %d %s\n",MCU_Info[5].InfoName,data,MCU_Info[5].InfoUnit);
	data = BSWAP_32(Monitorviewdata.cpu112vc);
	fprintf(stderr,"%s %d.%03d %s\n",MCU_Info[6].InfoName,data/1000,data%1000,MCU_Info[6].InfoUnit);


	fprintf(stderr,"%s %d %s",MCU_Info[7].InfoName,Monitorviewdata.amb1data,MCU_Info[7].InfoUnit);
	
	fprintf(stderr,"\n");


	fprintf(stderr,"%s %d %s",MCU_Info[9].InfoName,Monitorviewdata.cpu1data,MCU_Info[9].InfoUnit);
	fprintf(stderr,"\n");
	fprintf(stderr,"%s %d %s",MCU_Info[10].InfoName,Monitorviewdata.cpu2data,MCU_Info[10].InfoUnit);

	fprintf(stderr,"\n");
	fprintf(stderr,"%s %d %s\n",MCU_Info[11].InfoName,BSWAP_16(Monitorviewdata.fan1tachdata),MCU_Info[11].InfoUnit);


}


unsigned char checksum(unsigned char *buffer,unsigned long Size)
{
	unsigned char sum=0;
	unsigned long i ;
	for(i=0;i<Size;i++)
	{
		sum += buffer[i];
	
	}
	return sum;
}
unsigned char ipmi_csum(unsigned char * d, int s)
{
	unsigned char c = 0;
	for (; s > 0; s--, d++)
		c += *d;
	return -c;
}


/*
 *	Return escaped character for the given one
 */
unsigned char serial_bm_get_escaped_char(unsigned char c)
{
	int i;

	for (i = 0; i < 5; i++) {
		if (characters[i].character == c) {
			return characters[i].escape;
		}
	}

	return c;
}

/*
 *	Return unescaped character for the given one
 */
static inline unsigned char
serial_bm_get_unescaped_char(unsigned char c)
{
	int i;

	for (i = 0; i < 5; i++) {
		if (characters[i].escape == c) {
			return characters[i].character;
		}
	}

	return c;
}

/*
 *	This function parses incoming data in basic mode format to IPMB message
 */
static int
serial_bm_parse_buffer(const unsigned char * data, int data_len,
		struct serial_bm_parse_ctx * ctx)
{
	int i, tmp;

	for (i = 0; i < data_len; i++) 
	{
		/* check for start of new message */
		if (data[i] == BM_START) {
			ctx->state = MSG_IN_PROGRESS;
			ctx->escape = 0;
			ctx->msg_len = 0;
		/* check if message is not started */
		} 
		else if (ctx->state != MSG_IN_PROGRESS) 
		{
			/* skip character */
			continue;
		/* continue escape sequence */
		} 
		else if (ctx->escape) 
		{
			/* get original character */
			tmp = serial_bm_get_unescaped_char(data[i]);

			/* check if not special character */
			if (tmp == data[i]) 
			{
				fprintf(stderr, "ipmitool: bad response");
				/* reset message state */
				ctx->state = MSG_NONE;
				continue;
			}

			/* check message length */
			if (ctx->msg_len >= ctx->max_len) 
			{
				fprintf(stderr, "ipmitool: response is too long");
				/* reset message state */
				ctx->state = MSG_NONE;
				continue;
			}

			/* add parsed character */
			ctx->msg[ctx->msg_len++] = tmp;

			/* clear escape flag */
			ctx->escape = 0;
		/* check for escape character */
		} 
		else if (data[i] == BM_ESCAPE) 
		{
			ctx->escape = 1;
			continue;
		/* check for stop character */
		} 
		else if (data[i] == BM_STOP) 
		{
			ctx->state = MSG_DONE;
			return i + 1;
		/* check for packet handshake character */
		} 
		else if (data[i] == BM_HANDSHAKE) 
		{
			/* just skip it */
			continue;
		} 
		else 
		{
			/* check message length */
			if (ctx->msg_len >= ctx->max_len) 
			{
				fprintf(stderr, "ipmitool: response is too long");
				return -1;
			}

			/* add parsed character */
			ctx->msg[ctx->msg_len++] = data[i];
		}
	}

	/* return number of parsed characters */
	return i;
}


int Get_Bmc_Info_Handle(unsigned char mode)
{
	char mnbuf[250] = { 0 };
	unsigned char sendcmd[]={0x20,0xd0,0x10,0x81,0x04,0x10,0x00,0x00,0x00,0X6b};//get mcu monitor cmd
	int len = 0;
	int packagelen = 0;

	unsigned char checkdata=0;
	unsigned char state=1;
	int count=0;
	int i = 0;
	struct serial_bm_recv_ctx read_ctx;
	struct serial_bm_parse_ctx parse_ctx;
	int msg_len, netFn, rqSeq;
	unsigned char temp = 0;

	/* reset receive context */
	read_ctx.buffer_size = 0;
	read_ctx.max_buffer_size = SERIAL_BM_MAX_BUFFER_SIZE;
	parse_ctx.state = MSG_NONE;
	parse_ctx.msg = mnbuf;
	parse_ctx.max_len = sizeof (mnbuf);
	parse_ctx.msg_len = 0;
	sendcmd[6]=mode;
	if(mode == GETTEMPFMFLAG)
	{

		sendcmd[6]=0;
		sendcmd[7] = 0;
		sendcmd[8] = 0;
	}
	else
	{
		temp = get_maxcpu_temp();
		sendcmd[6]=mode;
		sendcmd[7] = temp&0x0f;
		sendcmd[8] = (temp&0xf0)>>4;
	}
	

	sendcmd[9]=0xff-sendcmd[3]-sendcmd[4]-sendcmd[5]-sendcmd[6]-sendcmd[7]-sendcmd[8]+0x01;

			serial_bm_flush(fd);
			serial_bm_send_msg(fd,sendcmd,sizeof(sendcmd));


			state = 1;
			memset(mnbuf, 0, sizeof(mnbuf));
			
			do
			{
				len=0;
				/* wait for data in the port */
				if (serial_bm_wait_for_data(fd)) 
				{
					fprintf(stderr, "wait serial timeout\n");
					break;
				}
				/* read data into buffer */
				len = read(fd, read_ctx.buffer + read_ctx.buffer_size, read_ctx.max_buffer_size - read_ctx.buffer_size);

				if (len < 0) 
				{
					fprintf(stderr, "read serial error\n");
					break;
				}		

				/* increment buffer size */
				read_ctx.buffer_size += len;
				if (verbose == 1) 
				{
						   for (i = 0; i < read_ctx.buffer_size; i++) 
							{
								fprintf(stderr,"%x ", (unsigned char)read_ctx.buffer[i]);
							}
							fprintf(stderr,"\n");
				}
				/* parse buffer */
				len = serial_bm_parse_buffer(read_ctx.buffer,read_ctx.buffer_size, &parse_ctx);
				if(len < 0 )
				{
					fprintf(stderr,"recv data outrang!\n");
					break;
				}
				if (len < read_ctx.buffer_size) {
					/* move non-parsed part of the buffer to the beginning */
					memmove(read_ctx.buffer, read_ctx.buffer + len,
							read_ctx.buffer_size - len);
				}
				/* decrement buffer size */
				read_ctx.buffer_size -= len;
				
			} while (parse_ctx.state != MSG_DONE);
			
			if (parse_ctx.msg_len < 8) 
			{
				fprintf(stderr,"response is too short\n");
				return -1;
			}
			/* validate checksum 1 */
			if (ipmi_csum(parse_ctx.msg, 3)) 
			{
				fprintf(stderr, "ipmitool: bad checksum 1\n");
				return -1;
			}
			/* validate checksum 2 */
			if (ipmi_csum(parse_ctx.msg + 3, parse_ctx.msg_len - 3)) 
			{
				fprintf(stderr, "ipmitool: bad checksum 2\n");
				return -1;
			}
	
			/* swap requester and responder LUNs */
			netFn = ((sendcmd[1]|4) & ~3) | (sendcmd[0] & 3);
			rqSeq = (sendcmd[4] & ~3) | (sendcmd[1] & 3);
			/* check for the waited response */
			if (parse_ctx.msg[0] == sendcmd[3]
					&& parse_ctx.msg[1] == netFn
					&& parse_ctx.msg[3] == sendcmd[0]
					&& parse_ctx.msg[4] == rqSeq
					&& parse_ctx.msg[5] == sendcmd[5]) 
			{
				if (verbose == 1) 
				{
				/* check if something new has been parsed */

					fprintf(stderr, "Got response:\n");
					fprintf(stderr, "  rsSA            = 0x%x\n", parse_ctx.msg[0]);
					fprintf(stderr, "  NetFN/rsLUN     = 0x%x\n", parse_ctx.msg[1]);
					fprintf(stderr, "  rqSA            = 0x%x\n", parse_ctx.msg[3]);
					fprintf(stderr, "  rqSeq/rqLUN     = 0x%x\n", parse_ctx.msg[4]);
					fprintf(stderr, "  cmd             = 0x%x\n", parse_ctx.msg[5]);
					fprintf(stderr, "  completion code = 0x%x\n", parse_ctx.msg[6]);
					if (parse_ctx.msg_len > 8) 
					{
						fprintf(stderr, "  data_len        = %ld\n", parse_ctx.msg_len - 8);
						 for (i = 0; i < (parse_ctx.msg_len - 8); i++) 
							fprintf(stderr,"data = %x ", (unsigned char)parse_ctx.msg[7 + i]);
						fprintf(stderr,"\n");

					}
				}

					if(parse_ctx.msg[6] == CC_SUCCESS)
					{
						monitordataprocessor(&Monitorviewdata,&parse_ctx.msg[7]);

						Display_Info();
					}
					return 0;
			}
			return -1;
}


const char *wdt_use_string[8] = {
	"Reserved",
	"BIOS FRB2",
	"BIOS/POST",
	"OS Load",
	"SMS/OS",
	"OEM",
	"Reserved",
	"Reserved"
};

const char *wdt_action_string[8] = {
	"No action",
	"Hard Reset",
	"Power Down",
	"Power Cycle",
	"Reserved",
	"Reserved",
	"Reserved",
	"Reserved"
};

int Wdt_Handle(char mode,unsigned short timeout)
{
	char mnbuf[250] = { 0 };
	unsigned char sendgetwdtcmd[]={0x20,0x18,0xc8,0x81,0x04,0x25,0X56};//get wdt cmd
	unsigned char sendresetwdtcmd[]={0x20,0x18,0xc8,0x81,0x04,0x22,0X56};//reset wdt cmd
	unsigned char sendoffwdtcmd[]={0x20,0x18,0xc8,0x81,0x04,0x24,0x04,0x00,0x00,0x10,0xb8,0x0b,0X56};//off wdt cmd
	unsigned char sendsetwdtcmd[]={0x20,0x18,0xc8,0x81,0x04,0x24,0x04,0x01,0x00,0x10,0xff,0xff,0X56};//set wdt cmd
	int len = 0;
	int packagelen = 0;

	unsigned char checkdata=0;
	unsigned char state=1;
	int count=0;
	int i = 0;
	struct serial_bm_recv_ctx read_ctx;
	struct serial_bm_parse_ctx parse_ctx;
	int msg_len, netFn, rqSeq;
	
	/* reset receive context */
	read_ctx.buffer_size = 0;
	read_ctx.max_buffer_size = SERIAL_BM_MAX_BUFFER_SIZE;
	parse_ctx.state = MSG_NONE;
	parse_ctx.msg = mnbuf;
	parse_ctx.max_len = sizeof (mnbuf);
	parse_ctx.msg_len = 0;
	
	
			serial_bm_flush(fd);
			switch(mode)
			{
				case GET_WDT_CMD_FLAG:
					sendgetwdtcmd[6]=0xff-sendgetwdtcmd[3]-sendgetwdtcmd[4]-sendgetwdtcmd[5]+0x01;
					serial_bm_send_msg(fd,sendgetwdtcmd,sizeof(sendgetwdtcmd));
				break;
				case RESET_WDT_CMD_FLAG:
					sendresetwdtcmd[6]=0xff-sendresetwdtcmd[3]-sendresetwdtcmd[4]-sendresetwdtcmd[5]+0x01;
					serial_bm_send_msg(fd,sendresetwdtcmd,sizeof(sendresetwdtcmd));
				break;
				case OFF_WDT_CMD_FLAG:
					sendoffwdtcmd[12]=0xff-sendoffwdtcmd[3]-sendoffwdtcmd[4]-sendoffwdtcmd[5]-sendoffwdtcmd[6]-sendoffwdtcmd[7]-sendoffwdtcmd[8]-sendoffwdtcmd[9]-sendoffwdtcmd[10]-sendoffwdtcmd[11]+0x01;

					serial_bm_send_msg(fd,sendoffwdtcmd,sizeof(sendoffwdtcmd));
				break;
				case SET_WDT_CMD_FLAG:
					sendsetwdtcmd[10]=timeout&0xff;
					sendsetwdtcmd[11]=(timeout>>8)&0xff;
					sendsetwdtcmd[12]=0xff-sendsetwdtcmd[3]-sendsetwdtcmd[4]-sendsetwdtcmd[5]-sendsetwdtcmd[6]-sendsetwdtcmd[7]-sendsetwdtcmd[8]-sendsetwdtcmd[9]-sendsetwdtcmd[10]-sendsetwdtcmd[11]+0x01;
					serial_bm_send_msg(fd,sendsetwdtcmd,sizeof(sendsetwdtcmd));
				break;
			}
			

			state = 1;
			memset(mnbuf, 0, sizeof(mnbuf));
			
			do
			{
				len=0;
				/* wait for data in the port */
				if (serial_bm_wait_for_data(fd)) 
				{
					fprintf(stderr, "wait serial timeout\n");
					break;
				}
				/* read data into buffer */
				len = read(fd, read_ctx.buffer + read_ctx.buffer_size, read_ctx.max_buffer_size - read_ctx.buffer_size);
				
				if (len < 0) 
				{
					fprintf(stderr, "read serial error\n");
					break;
				}		

				/* increment buffer size */
				read_ctx.buffer_size += len;
if (verbose == 1) 
{
						   for (i = 0; i < read_ctx.buffer_size; i++) 
							{
								fprintf(stderr,"%x ", (unsigned char)read_ctx.buffer[i]);
							}
							fprintf(stderr,"\n");
}
				/* parse buffer */
				len = serial_bm_parse_buffer(read_ctx.buffer,read_ctx.buffer_size, &parse_ctx);
				if(len < 0 )
				{
					fprintf(stderr,"recv data outrang!\n");
					break;
				}
				if (len < read_ctx.buffer_size) {
					/* move non-parsed part of the buffer to the beginning */
					memmove(read_ctx.buffer, read_ctx.buffer + len,
							read_ctx.buffer_size - len);
				}
				/* decrement buffer size */
				read_ctx.buffer_size -= len;
				
			} while (parse_ctx.state != MSG_DONE);
			
			if (parse_ctx.msg_len < 8) 
			{
				fprintf(stderr,"response is too short\n");
				return -1;
			}
			/* validate checksum 1 */
			if (ipmi_csum(parse_ctx.msg, 3)) 
			{
				fprintf(stderr, "ipmitool: bad checksum 1\n");
				return -1;
			}
			/* validate checksum 2 */
			if (ipmi_csum(parse_ctx.msg + 3, parse_ctx.msg_len - 3)) 
			{
				fprintf(stderr, "ipmitool: bad checksum 2\n");
				return -1;
			}

					
			/* swap requester and responder LUNs */
			netFn = ((sendgetwdtcmd[1]|4) & ~3) | (sendgetwdtcmd[0] & 3);
			rqSeq = (sendgetwdtcmd[4] & ~3) | (sendgetwdtcmd[1] & 3);
			switch(mode)
			{
				case GET_WDT_CMD_FLAG:
				{
					/* check for the waited response */
					if (parse_ctx.msg[0] == sendgetwdtcmd[3]
							&& parse_ctx.msg[1] == netFn
							&& parse_ctx.msg[3] == sendgetwdtcmd[0]
							&& parse_ctx.msg[4] == rqSeq
							&& parse_ctx.msg[5] == sendgetwdtcmd[5]) 
					{
						if (verbose == 1) 
						{
						/* check if something new has been parsed */

							fprintf(stderr, "Got response:\n");
							fprintf(stderr, "  rsSA            = 0x%x\n", parse_ctx.msg[0]);
							fprintf(stderr, "  NetFN/rsLUN     = 0x%x\n", parse_ctx.msg[1]);
							fprintf(stderr, "  rqSA            = 0x%x\n", parse_ctx.msg[3]);
							fprintf(stderr, "  rqSeq/rqLUN     = 0x%x\n", parse_ctx.msg[4]);
							fprintf(stderr, "  cmd             = 0x%x\n", parse_ctx.msg[5]);
							fprintf(stderr, "  completion code = 0x%x\n", parse_ctx.msg[6]);
							if (parse_ctx.msg_len > 8) 
							{
								fprintf(stderr, "  data_len        = %ld\n", parse_ctx.msg_len - 8);
								 for (i = 0; i < (parse_ctx.msg_len - 8); i++) 
									fprintf(stderr,"data = %x ", (unsigned char)parse_ctx.msg[7 + i]);
								fprintf(stderr,"\n");

							}
						}
							if(parse_ctx.msg[6] == CC_SUCCESS)
							{
								GetWDTRes_T  wdt_res;
									
								memcpy (( unsigned char*)&wdt_res,( unsigned char*)&parse_ctx.msg[6], sizeof (GetWDTRes_T));
													
fprintf(stderr,"Watchdog Timer Use:     %s (0x%02x)\r\n",
										wdt_use_string[(wdt_res.TmrUse & 0x07 )], wdt_res.TmrUse);
								fprintf(stderr,"Watchdog Timer Is:      %s\r\n",
									wdt_res.TmrUse & 0x40 ? "Started/Running" : "Stopped");
								fprintf(stderr,"Watchdog Timer Actions: %s (0x%02x)\r\n",
									 wdt_action_string[(wdt_res.TmrActions&0x07)], wdt_res.TmrActions);
										
								fprintf(stderr,"Pre-timeout interval:   %d seconds\r\n", wdt_res.PreTimeOutInterval);
										
								fprintf(stderr,"Timer Expiration Flags: 0x%02x\r\n", wdt_res.ExpirationFlag);
	
								fprintf(stderr,"Initial Countdown:      %i sec\r\n",wdt_res.InitCountDown/10);

								fprintf(stderr,"Present Countdown:      %i sec\r\n",wdt_res.PresentCountDown/10);

							}
							
							return 0;
					}
				}
				break;
				case RESET_WDT_CMD_FLAG:
				{
					/* check for the waited response */
					if (parse_ctx.msg[0] == sendresetwdtcmd[3]
							&& parse_ctx.msg[1] == netFn
							&& parse_ctx.msg[3] == sendresetwdtcmd[0]
							&& parse_ctx.msg[4] == rqSeq
							&& parse_ctx.msg[5] == sendresetwdtcmd[5]) 
					{
						if (verbose == 1) 
						{
						/* check if something new has been parsed */

							fprintf(stderr, "Got response:\n");
							fprintf(stderr, "  rsSA            = 0x%x\n", parse_ctx.msg[0]);
							fprintf(stderr, "  NetFN/rsLUN     = 0x%x\n", parse_ctx.msg[1]);
							fprintf(stderr, "  rqSA            = 0x%x\n", parse_ctx.msg[3]);
							fprintf(stderr, "  rqSeq/rqLUN     = 0x%x\n", parse_ctx.msg[4]);
							fprintf(stderr, "  cmd             = 0x%x\n", parse_ctx.msg[5]);
							fprintf(stderr, "  completion code = 0x%x\n", parse_ctx.msg[6]);
							if (parse_ctx.msg_len > 8) 
							{
								fprintf(stderr, "  data_len        = %ld\n", parse_ctx.msg_len - 8);
								 for (i = 0; i < (parse_ctx.msg_len - 8); i++) 
									fprintf(stderr,"data = %x ", (unsigned char)parse_ctx.msg[7 + i]);
								fprintf(stderr,"\n");

							}
						}
							if(parse_ctx.msg[6] == CC_SUCCESS)
							{
								fprintf(stderr,"Watchdog Timer Reset -  countdown restarted!\n");
							}
							
							return 0;
					}
				}
				break;
				case OFF_WDT_CMD_FLAG:
				{
					/* check for the waited response */
					if (parse_ctx.msg[0] == sendoffwdtcmd[3]
							&& parse_ctx.msg[1] == netFn
							&& parse_ctx.msg[3] == sendoffwdtcmd[0]
							&& parse_ctx.msg[4] == rqSeq
							&& parse_ctx.msg[5] == sendoffwdtcmd[5]) 
					{
						if (verbose == 1) 
						{
						/* check if something new has been parsed */

							fprintf(stderr, "Got response:\n");
							fprintf(stderr, "  rsSA            = 0x%x\n", parse_ctx.msg[0]);
							fprintf(stderr, "  NetFN/rsLUN     = 0x%x\n", parse_ctx.msg[1]);
							fprintf(stderr, "  rqSA            = 0x%x\n", parse_ctx.msg[3]);
							fprintf(stderr, "  rqSeq/rqLUN     = 0x%x\n", parse_ctx.msg[4]);
							fprintf(stderr, "  cmd             = 0x%x\n", parse_ctx.msg[5]);
							fprintf(stderr, "  completion code = 0x%x\n", parse_ctx.msg[6]);
							if (parse_ctx.msg_len > 8) 
							{
								fprintf(stderr, "  data_len        = %ld\n", parse_ctx.msg_len - 8);
								 for (i = 0; i < (parse_ctx.msg_len - 8); i++) 
									fprintf(stderr,"data = %x ", (unsigned char)parse_ctx.msg[7 + i]);
								fprintf(stderr,"\n");

							}
						}
							if(parse_ctx.msg[6] == CC_SUCCESS)
							{
								fprintf(stderr,"Watchdog Timer Shutoff successful -- timer stopped\n");
							}
							
							return 0;
					}
				}
				break;
				case SET_WDT_CMD_FLAG:
				{
					/* check for the waited response */
					if (parse_ctx.msg[0] == sendsetwdtcmd[3]
							&& parse_ctx.msg[1] == netFn
							&& parse_ctx.msg[3] == sendsetwdtcmd[0]
							&& parse_ctx.msg[4] == rqSeq
							&& parse_ctx.msg[5] == sendsetwdtcmd[5]) 
					{
						if (verbose == 1) 
						{
						/* check if something new has been parsed */

							fprintf(stderr, "Got response:\n");
							fprintf(stderr, "  rsSA            = 0x%x\n", parse_ctx.msg[0]);
							fprintf(stderr, "  NetFN/rsLUN     = 0x%x\n", parse_ctx.msg[1]);
							fprintf(stderr, "  rqSA            = 0x%x\n", parse_ctx.msg[3]);
							fprintf(stderr, "  rqSeq/rqLUN     = 0x%x\n", parse_ctx.msg[4]);
							fprintf(stderr, "  cmd             = 0x%x\n", parse_ctx.msg[5]);
							fprintf(stderr, "  completion code = 0x%x\n", parse_ctx.msg[6]);
							if (parse_ctx.msg_len > 8) 
							{
								fprintf(stderr, "  data_len        = %ld\n", parse_ctx.msg_len - 8);
								 for (i = 0; i < (parse_ctx.msg_len - 8); i++) 
									fprintf(stderr,"data = %x ", (unsigned char)parse_ctx.msg[7 + i]);
								fprintf(stderr,"\n");

							}
						}
							if(parse_ctx.msg[6] == CC_SUCCESS)
							{
								fprintf(stderr, "Watchdog Timer was successfully configured");
							}
							
							return 0;
					}
				}
				break;	
			}
			return -1;
}


int fan_Handle(char cmdmode,unsigned char fanmode,unsigned char *fanduty)
{
	char mnbuf[250] = { 0 };

	unsigned char sendgetfancmd[]={0x20,0xd0,0x10,0x81,0x04,0x25,0X56};//get fan cmd
	unsigned char sendsetfancmd[]={0x20,0xd0,0x10,0x81,0x04,0x24,0x00,0x00,0x00,0x00,0x00,0X57};//set fan cmd
	int len = 0;
	int packagelen = 0;

	unsigned char checkdata=0;
	unsigned char state=1;
	int count=0;
	int i = 0;
	struct serial_bm_recv_ctx read_ctx;
	struct serial_bm_parse_ctx parse_ctx;
	int msg_len, netFn, rqSeq;
	
	/* reset receive context */
	read_ctx.buffer_size = 0;
	read_ctx.max_buffer_size = SERIAL_BM_MAX_BUFFER_SIZE;
	parse_ctx.state = MSG_NONE;
	parse_ctx.msg = mnbuf;
	parse_ctx.max_len = sizeof (mnbuf);
	parse_ctx.msg_len = 0;
	//printf("cmdmode=%d,fanmode=%d,fanduty=%d,%d,%d,%d\n",cmdmode,fanmode,fanduty[0],fanduty[1],fanduty[2],fanduty[3]);
	
			serial_bm_flush(fd);
			switch(cmdmode)
			{
				case GET_FAN_CMD_FLAG:
					serial_bm_send_msg(fd,sendgetfancmd,sizeof(sendgetfancmd));
				break;
				case SET_FAN_CMD_FLAG:
					sendsetfancmd[6]=fanmode&0xff;
					sendsetfancmd[7]=fanduty[0]&0xff;
					sendsetfancmd[8]=fanduty[1]&0xff;
					sendsetfancmd[9]=fanduty[2]&0xff;
					sendsetfancmd[10]=fanduty[3]&0xff;
					sendsetfancmd[11]=0xff-sendsetfancmd[3]-sendsetfancmd[4]-sendsetfancmd[5]-sendsetfancmd[6]-sendsetfancmd[7]-sendsetfancmd[8]-sendsetfancmd[9]-sendsetfancmd[10]+0x01;
					serial_bm_send_msg(fd,sendsetfancmd,sizeof(sendsetfancmd));
				break;
			}
			

			state = 1;
			memset(mnbuf, 0, sizeof(mnbuf));
			
			do
			{
				len=0;
				/* wait for data in the port */
				if (serial_bm_wait_for_data(fd)) 
				{
					fprintf(stderr, "wait serial timeout\n");
					break;
				}
				/* read data into buffer */
				len = read(fd, read_ctx.buffer + read_ctx.buffer_size, read_ctx.max_buffer_size - read_ctx.buffer_size);
				
				if (len < 0) 
				{
					fprintf(stderr, "read serial error\n");
					break;
				}		

				/* increment buffer size */
				read_ctx.buffer_size += len;
if (verbose == 1) 
{
						   for (i = 0; i < read_ctx.buffer_size; i++) 
							{
								fprintf(stderr,"%x ", (unsigned char)read_ctx.buffer[i]);
							}
							fprintf(stderr,"\n");
}
				/* parse buffer */
				len = serial_bm_parse_buffer(read_ctx.buffer,read_ctx.buffer_size, &parse_ctx);
				if(len < 0 )
				{
					fprintf(stderr,"recv data outrang!\n");
					break;
				}
				if (len < read_ctx.buffer_size) {
					/* move non-parsed part of the buffer to the beginning */
					memmove(read_ctx.buffer, read_ctx.buffer + len,
							read_ctx.buffer_size - len);
				}
				/* decrement buffer size */
				read_ctx.buffer_size -= len;
				
			} while (parse_ctx.state != MSG_DONE);
			
			if (parse_ctx.msg_len < 8) 
			{
				fprintf(stderr,"response is too short\n");
				return -1;
			}
			/* validate checksum 1 */
			if (ipmi_csum(parse_ctx.msg, 3)) 
			{
				fprintf(stderr, "ipmitool: bad checksum 1\n");
				return -1;
			}
			/* validate checksum 2 */
			if (ipmi_csum(parse_ctx.msg + 3, parse_ctx.msg_len - 3)) 
			{
				fprintf(stderr, "ipmitool: bad checksum 2\n");
				return -1;
			}

					
			/* swap requester and responder LUNs */
			netFn = ((sendgetfancmd[1]|4) & ~3) | (sendgetfancmd[0] & 3);
			rqSeq = (sendgetfancmd[4] & ~3) | (sendgetfancmd[1] & 3);
			switch(cmdmode)
			{
				case GET_FAN_CMD_FLAG:
				{
					/* check for the waited response */
					if (parse_ctx.msg[0] == sendgetfancmd[3]
							&& parse_ctx.msg[1] == netFn
							&& parse_ctx.msg[3] == sendgetfancmd[0]
							&& parse_ctx.msg[4] == rqSeq
							&& parse_ctx.msg[5] == sendgetfancmd[5]) 
					{
						if (verbose == 1) 
						{
						/* check if something new has been parsed */

							fprintf(stderr, "Got response:\n");
							fprintf(stderr, "  rsSA            = 0x%x\n", parse_ctx.msg[0]);
							fprintf(stderr, "  NetFN/rsLUN     = 0x%x\n", parse_ctx.msg[1]);
							fprintf(stderr, "  rqSA            = 0x%x\n", parse_ctx.msg[3]);
							fprintf(stderr, "  rqSeq/rqLUN     = 0x%x\n", parse_ctx.msg[4]);
							fprintf(stderr, "  cmd             = 0x%x\n", parse_ctx.msg[5]);
							fprintf(stderr, "  completion code = 0x%x\n", parse_ctx.msg[6]);
							if (parse_ctx.msg_len > 8) 
							{
								fprintf(stderr, "  data_len        = %ld\n", parse_ctx.msg_len - 8);
								 for (i = 0; i < (parse_ctx.msg_len - 8); i++) 
									fprintf(stderr,"data = %x ", (unsigned char)parse_ctx.msg[7 + i]);
								fprintf(stderr,"\n");

							}
						}
							if(parse_ctx.msg[6] == CC_SUCCESS)
							{
								GetOemFANinfoRes_T  getfan_res;
									
								memcpy (( unsigned char*)&getfan_res,( unsigned char*)&parse_ctx.msg[6], sizeof (GetOemFANinfoRes_T));

								fprintf(stderr,"Fan mode: %s\r\n", (getfan_res.fanmode == FAN_MANUAL_MODE)?"manual":"auto");

								fprintf(stderr,"Fan1 Speed:      %d RPM\r\n",getfan_res.fanspeed[0]);
								fprintf(stderr,"Fan2 Speed:      %d RPM\r\n",getfan_res.fanspeed[1]);
								fprintf(stderr,"Fan3 Speed:      %d RPM\r\n",getfan_res.fanspeed[2]);
								fprintf(stderr,"Fan4 Speed:      %d RPM\r\n",getfan_res.fanspeed[3]);
								fprintf(stderr,"Fan1 Duty:       %d %% \r\n",getfan_res.fanduty[0]);
								fprintf(stderr,"Fan2 Duty:       %d %% \r\n",getfan_res.fanduty[1]);
								fprintf(stderr,"Fan3 Duty:       %d %% \r\n",getfan_res.fanduty[2]);
								fprintf(stderr,"Fan4 Duty:       %d %% \r\n",getfan_res.fanduty[3]);
							}
							
							return 0;
					}
				}
				break;
				case SET_FAN_CMD_FLAG:
				{
					/* check for the waited response */
					if (parse_ctx.msg[0] == sendsetfancmd[3]
							&& parse_ctx.msg[1] == netFn
							&& parse_ctx.msg[3] == sendsetfancmd[0]
							&& parse_ctx.msg[4] == rqSeq
							&& parse_ctx.msg[5] == sendsetfancmd[5]) 
					{
						if (verbose == 1) 
						{
						/* check if something new has been parsed */

							fprintf(stderr, "Got response:\n");
							fprintf(stderr, "  rsSA            = 0x%x\n", parse_ctx.msg[0]);
							fprintf(stderr, "  NetFN/rsLUN     = 0x%x\n", parse_ctx.msg[1]);
							fprintf(stderr, "  rqSA            = 0x%x\n", parse_ctx.msg[3]);
							fprintf(stderr, "  rqSeq/rqLUN     = 0x%x\n", parse_ctx.msg[4]);
							fprintf(stderr, "  cmd             = 0x%x\n", parse_ctx.msg[5]);
							fprintf(stderr, "  completion code = 0x%x\n", parse_ctx.msg[6]);
							if (parse_ctx.msg_len > 8) 
							{
								fprintf(stderr, "  data_len        = %ld\n", parse_ctx.msg_len - 8);
								 for (i = 0; i < (parse_ctx.msg_len - 8); i++) 
									fprintf(stderr,"data = %x ", (unsigned char)parse_ctx.msg[7 + i]);
								fprintf(stderr,"\n");

							}
						}
							if(parse_ctx.msg[6] == CC_SUCCESS)
							{
								fprintf(stderr, "Set Fan was successfully configured\n");
							}
							
							return 0;
					}
				}
				break;	
			}
			return -1;
}


int powermode_Handle(unsigned char powermode)
{
	char mnbuf[250] = { 0 };
	unsigned char sendcmd[]={0x20,0xd0,0x10,0x81,0x04,0x26,0x00,0X5a};//set sn info cmd
	int len = 0;
	int packagelen = 0;

	unsigned char checkdata=0;
	unsigned char state=1;
	int count=0;
	int i = 0;
	struct serial_bm_recv_ctx read_ctx;
	struct serial_bm_parse_ctx parse_ctx;
	int msg_len, netFn, rqSeq;
	unsigned char sum = 0;
	/* reset receive context */
	read_ctx.buffer_size = 0;
	read_ctx.max_buffer_size = SERIAL_BM_MAX_BUFFER_SIZE;
	parse_ctx.state = MSG_NONE;
	parse_ctx.msg = mnbuf;
	parse_ctx.max_len = sizeof (mnbuf);
	parse_ctx.msg_len = 0;


	sendcmd[6] = powermode;
	
	sendcmd[7]=0xff-sendcmd[3]-sendcmd[4]-sendcmd[5]-sendcmd[6]+0x01;

			serial_bm_flush(fd);
			serial_bm_send_msg(fd,sendcmd,sizeof(sendcmd));


			state = 1;
			memset(mnbuf, 0, sizeof(mnbuf));
			
			do
			{
				len=0;
				/* wait for data in the port */
				if (serial_bm_wait_for_data(fd)) 
				{
					fprintf(stderr, "wait serial timeout\n");
					break;
				}
				/* read data into buffer */
				len = read(fd, read_ctx.buffer + read_ctx.buffer_size, read_ctx.max_buffer_size - read_ctx.buffer_size);
		
				if (len < 0) 
				{
					fprintf(stderr, "read serial error\n");
					break;
				}		

				/* increment buffer size */
				read_ctx.buffer_size += len;
if (verbose == 1) 
{
						   for (i = 0; i < read_ctx.buffer_size; i++) 
							{
								fprintf(stderr,"%x ", (unsigned char)read_ctx.buffer[i]);
							}
							fprintf(stderr,"\n");
}
				/* parse buffer */
				len = serial_bm_parse_buffer(read_ctx.buffer,read_ctx.buffer_size, &parse_ctx);
				if(len < 0 )
				{
					fprintf(stderr,"recv data outrang!\n");
					break;
				}
				if (len < read_ctx.buffer_size) {
					/* move non-parsed part of the buffer to the beginning */
					memmove(read_ctx.buffer, read_ctx.buffer + len,
							read_ctx.buffer_size - len);
				}
				/* decrement buffer size */
				read_ctx.buffer_size -= len;
				
			} while (parse_ctx.state != MSG_DONE);
			
			if (parse_ctx.msg_len < 8) 
			{
				fprintf(stderr,"response is too short\n");
				return -1;
			}
			/* validate checksum 1 */
			if (ipmi_csum(parse_ctx.msg, 3)) 
			{
				fprintf(stderr, "ipmitool: bad checksum 1\n");
				return -1;
			}
			/* validate checksum 2 */
			if (ipmi_csum(parse_ctx.msg + 3, parse_ctx.msg_len - 3)) 
			{
				fprintf(stderr, "ipmitool: bad checksum 2\n");
				return -1;
			}
	
			/* swap requester and responder LUNs */
			netFn = ((sendcmd[1]|4) & ~3) | (sendcmd[0] & 3);
			rqSeq = (sendcmd[4] & ~3) | (sendcmd[1] & 3);
			/* check for the waited response */
			if (parse_ctx.msg[0] == sendcmd[3]
					&& parse_ctx.msg[1] == netFn
					&& parse_ctx.msg[3] == sendcmd[0]
					&& parse_ctx.msg[4] == rqSeq
					&& parse_ctx.msg[5] == sendcmd[5]) 
			{
				if (verbose == 1) 
				{
				/* check if something new has been parsed */

					fprintf(stderr, "Got response:\n");
					fprintf(stderr, "  rsSA            = 0x%x\n", parse_ctx.msg[0]);
					fprintf(stderr, "  NetFN/rsLUN     = 0x%x\n", parse_ctx.msg[1]);
					fprintf(stderr, "  rqSA            = 0x%x\n", parse_ctx.msg[3]);
					fprintf(stderr, "  rqSeq/rqLUN     = 0x%x\n", parse_ctx.msg[4]);
					fprintf(stderr, "  cmd             = 0x%x\n", parse_ctx.msg[5]);
					fprintf(stderr, "  completion code = 0x%x\n", parse_ctx.msg[6]);
					if (parse_ctx.msg_len > 8) 
					{
						fprintf(stderr, "  data_len        = %ld\n", parse_ctx.msg_len - 8);
						 for (i = 0; i < (parse_ctx.msg_len - 8); i++) 
							fprintf(stderr,"data = %x ", (unsigned char)parse_ctx.msg[7 + i]);
						fprintf(stderr,"\n");

					}
				}
					if(parse_ctx.msg[6] == CC_SUCCESS)
					{
						fprintf(stderr,"set power mode:%s success\n",powermode?"AT":"ATX");
					}
					else
						fprintf(stderr,"set power mode:%s failed\n",powermode?"AT":"ATX");
					
					return 0;
			}
			return -1;
}