#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
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



unsigned int baudrate = 115200;
unsigned short flow_ctrl_flag = 0;
unsigned short bits = 8;
char parity = 'n';
unsigned short stop = 1;
char flow = 'n';
unsigned int timeout = 3;
unsigned int retry = 3;
char  dev_name[32] = "/dev/ttyAMA1";
int fd = 0;
unsigned char verbose=0;


MNinfoData Monitorviewdata;

 
void display_usage(const char * progname)       
{    

	fprintf(stderr,"***************************************************************************\n");
	fprintf(stderr,"%s Utility %s, %s, %s \r\n",progname,TOOLS_VER, __DATE__, __TIME__);
	fprintf(stderr,"usage: %s <command>\n",progname);
	fprintf(stderr,"    -m        Monitor mode run\n");
	fprintf(stderr,"    -w        watchdog command\n");
	fprintf(stderr,"              -w reset\n");
	fprintf(stderr,"              -w get\n");
	fprintf(stderr,"              -w set 60\n");
	fprintf(stderr,"              -w off\n");
	fprintf(stderr,"    -f        Fan cmd\n");
	fprintf(stderr,"              -f get\n");
	fprintf(stderr,"              -f set 0/1 40 25 55 90\n");
	fprintf(stderr,"    -a 0/1    Set Power Mode(1:at;0:atx)\n");
	fprintf(stderr,"    -h        Show Help\n");
	fprintf(stderr,"***************************************************************************\n");
}


int kbhit(void)

{

    struct termios oldt, newt;

    int ch;

    int oldf;

    tcgetattr(STDIN_FILENO, &oldt);

    newt = oldt;

    newt.c_lflag &= ~(ICANON | ECHO);

    tcsetattr(STDIN_FILENO, TCSANOW, &newt);

    oldf = fcntl(STDIN_FILENO, F_GETFL, 0);

    fcntl(STDIN_FILENO, F_SETFL, oldf | O_NONBLOCK);

    ch = getchar();

    tcsetattr(STDIN_FILENO, TCSANOW, &oldt);

    fcntl(STDIN_FILENO, F_SETFL, oldf);

    if(ch != EOF)
    {
        ungetc(ch, stdin);
        return 1;
    }

    return 0;

}




# define OPTION_STRING	"ma:w:f:t:lh"

int main(int argc, char *argv[])
{
	

    int ret = 0;
	
	char ch = 0;
	unsigned char monitormode = 0;
	unsigned char wdtmode = 0;
	unsigned char fancmdmode = 0;
	unsigned char setledmode = 0,ledctrl=0;
	int port = 0;
	int timesecond = 0;
	int fandata = 0;
	unsigned char  fanmode=0;
	unsigned char  fanduty[4]={0};
	int argflag;
	char * progname = NULL;
	char *user_cmd = NULL;
	unsigned char unit_data=0,time_data=0;
	progname = strrchr(argv[0], '/');
	progname = ((progname == NULL) ? argv[0] : progname+1);
	char * cmdstr = NULL;
	unsigned long timestamp=0;
	unsigned char powerselmode = 0;
	unsigned char powermode = 0;

	if(argc <  2)
	{
		display_usage(progname);
		return 0;
	}

	while ((argflag = getopt(argc, (char **)argv, OPTION_STRING)) != -1)
	{
		switch (argflag) 
		{
			case 'm':
				monitormode=1;
				break;
			case 'a':
				powerselmode=1;
				powermode = atoi(optarg);
				printf("powermode=%d\n",powermode);
				break;
			case 'w':
				wdtmode=1;
				
				cmdstr = strdup(optarg);
				//fprintf(stderr,"cmdstr=%s,optind=%x,%s\n",cmdstr,optind,argv[optind]);
				if(!strcmp(cmdstr, "reset"))
				{
					wdtmode=RESET_WDT_CMD_FLAG;
				}
				else if(!strcmp(cmdstr, "get"))
				{
					wdtmode=GET_WDT_CMD_FLAG;
				}
				else if(!strcmp(cmdstr, "set"))
				{
					wdtmode=SET_WDT_CMD_FLAG;
					
					if (str2int(argv[optind], &timesecond) != 0) {
						fprintf(stderr, "watchdog time Invalid parameter=%x .\n",timesecond);
						goto out_free;
					}
					if (timesecond < 1 || timesecond > 65535) {
						fprintf(stderr, "watchdog time %i is out of range.\n", port);
						goto out_free;
					}
				}
				else if(!strcmp(cmdstr, "off"))
				{
					wdtmode=OFF_WDT_CMD_FLAG;
				}	
				break;
			case 'f':
				fancmdmode=1;
				
				cmdstr = strdup(optarg);
				//fprintf(stderr,"cmdstr=%s,optind=%x,%s\n",cmdstr,optind,argv[optind]);
				if(!strcmp(cmdstr, "get"))
				{
					fancmdmode=GET_FAN_CMD_FLAG;
				}
				else if(!strcmp(cmdstr, "set"))
				{
					fancmdmode=SET_FAN_CMD_FLAG;
					
					if (str2int(argv[optind], &fandata) != 0) {
						fprintf(stderr, "fan mode Invalid parameter=%x .\n",fandata);
						goto out_free;
					}
					fanmode = fandata;
					if(fanmode == FAN_MANUAL_MODE)
					{
						if (str2int(argv[optind+1], &fandata) != 0) {
							fprintf(stderr, "fan1 duty Invalid parameter=%x .\n",fandata);
							goto out_free;
						}
						fanduty[0] = fandata;
						if (str2int(argv[optind+2], &fandata) != 0) {
							fprintf(stderr, "fan2 duty Invalid parameter=%x .\n",fandata);
							goto out_free;
						}
						fanduty[1] = fandata;	
						if (str2int(argv[optind+3], &fandata) != 0) {
							fprintf(stderr, "fan3 duty Invalid parameter=%x .\n",fandata);
							goto out_free;
						}
						fanduty[2] = fandata;
						if (str2int(argv[optind+4], &fandata) != 0) {
							fprintf(stderr, "fan4 duty Invalid parameter=%x .\n",fandata);
							goto out_free;
						}
						fanduty[3] = fandata;	
						if ((fanduty[0] > 100)||(fanduty[1] > 100)||(fanduty[2] > 100)||(fanduty[3] > 100)) {
							fprintf(stderr, "fan duty %i %i %i %i is out of range.\n", fanduty[0],fanduty[1],fanduty[2],fanduty[3]);
							goto out_free;
						}
					}
					
				}
				//printf("fancmdmode=%d,fanmode=%d,fanduty[0]=%d,fanduty[1]=%d,fanduty[2]=%d,fanduty[3]=%d,optind=%d\n",fancmdmode,fanmode,fanduty[0],fanduty[1],fanduty[2],fanduty[3],optind);
				break;
			case 'l':
				verbose=1;
				break;	
			case 'h':
				fprintf(stderr,"%s Utility %s, %s, %s \r\n",progname,TOOLS_VER, __DATE__, __TIME__);
				display_usage(progname);
				return 0;
				break;	
			default:
				display_usage(progname);
				return 0;
		}
	}

	
    struct timeval tv;


	unsigned char memuse=0;
	


	fd = serial_open(dev_name,baudrate, 8, 'n', 1, flow_ctrl_flag ? 'h' : 'n');
    if (fd < 0) {
        fprintf(stderr,"\nOpen %s failed\n", dev_name);
        return 0;
    }


	if(monitormode)
	{
		
		while(ESC!=ch)
		{
			Get_Bmc_Info_Handle(GETTEMPFMFLAG);
			
			if(kbhit())
			{
				ch=getchar();
				if(ESC!=ch)
				{	
					ch=0;
				}
			}
			sleep(1);
		}
			
	}
	else if(wdtmode)
	{
		retry = 3;
		while(retry)
		{
			ret = Wdt_Handle(wdtmode,timesecond*10);
			if(ret == 0)
				break;
			retry--;
		}
	}
	else if(powerselmode)
	{
		retry = 3;
		while(retry)
		{
			ret = powermode_Handle(powermode);
			if(ret == 0)
				break;
			retry--;
		}
	}
	else if(fancmdmode)
	{
		retry = 3;
		while(retry)
		{
			ret = fan_Handle(fancmdmode,fanmode,fanduty);
			if(ret == 0)
				break;
			retry--;
		}
	}
	
	serial_close(fd);
out_free:
	exit(1);

    return 0;
}
