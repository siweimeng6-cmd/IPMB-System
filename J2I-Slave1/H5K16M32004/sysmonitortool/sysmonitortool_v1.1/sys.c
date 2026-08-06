#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>


#include <fcntl.h>
#include <sys/vfs.h>
#include <getopt.h>             /* getopt_long() */
#include <errno.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include "uart.h"
#include "sys.h"
#include <stdint.h>



#define ull unsigned long long
 

// man proc
typedef struct CPUData{
    ull user;
    ull nice;
    ull system;
    ull idle;
    ull iowait;
    ull irq;
    ull softirq;
    ull steal;
    ull guest;
    ull guestnice;
    ull total;
}CPUData;


int get_memoccupy (unsigned char *memuse) 
{
    FILE *fd;
    int n;
    char buff[256];
    char buff1[256];
    char buff2[256];
    char buff3[256];
    char buff4[256];

    unsigned mem_use;
    
    fd = fopen ("/proc/meminfo", "r");

    fgets (buff1, sizeof(buff1), fd);

    int num1 = atoi(buff1+15); 
    
    fgets (buff2, sizeof(buff2), fd);



    int num2 = atoi(buff2+15);

    fgets (buff3, sizeof(buff3), fd);

    fgets (buff4, sizeof(buff4), fd);

    fgets (buff, sizeof(buff), fd); 


    int num5 = atoi(buff+15);

    mem_use = (float)100*(num1-num2-num5)/num1;

	*memuse = mem_use;
    fclose(fd);    
    
}


int get_hddoccupy (unsigned char *hddfree) 
{
    struct statfs sfs;
    int i = statfs("/", &sfs);
    int percent = (sfs.f_blocks - sfs.f_bfree ) * 100 / (sfs.f_blocks - sfs.f_bfree + sfs.f_bavail) + 1;
    *hddfree = 100-percent;
}

int get_cpuuse(unsigned char *cpuuse)
{
		float percent;
		float sys_percent;
		float usr_percent;
		float idle_percent;
		float sum;
		ull userPeriod;
		ull nicePeriod;
		ull systemAllPeriod;
		ull stealPeriod;
		ull guestPeriod;
		ull totalPeriod;
		ull idlePeriod;
	
		int fd;
		int cnt = 0;
		char buf[1024];
		
		CPUData cur, sav;
		
		memset(&sav, 0, sizeof(sav));
		if((fd = open("/proc/stat", O_RDONLY)) == -1)
			return -1;
        
        lseek(fd, 0, SEEK_SET);	
        if((cnt = read(fd, buf, sizeof(buf)-1)) < 0)
            return -1;
        buf[cnt] = '\0';
 
        memset(&cur, 0, sizeof(cur));
        sscanf(buf, "cpu%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu",
                &cur.user, &cur.nice, &cur.system,
                &cur.idle, &cur.iowait, &cur.irq, &cur.softirq,
                &cur.steal, &cur.guest, &cur.guestnice);
 
        cur.total = cur.user + cur.nice + cur.system + cur.idle
            + cur.iowait + cur.irq + cur.softirq + cur.steal;
 
        userPeriod = (cur.user - cur.guest) - (sav.user - sav.guest);
        nicePeriod = (cur.nice - cur.guestnice) - (sav.nice - sav.guestnice);
        systemAllPeriod = (cur.system + cur.irq + cur.softirq)
                            - (sav.system + sav.irq + sav.softirq);
        stealPeriod = cur.steal - sav.steal;
        guestPeriod = (cur.guest + cur.guestnice) - (sav.guest + sav.guestnice);
        totalPeriod = cur.total - sav.total;
        idlePeriod  = cur.idle - sav.idle;
 
        percent = (nicePeriod + userPeriod + systemAllPeriod + stealPeriod + guestPeriod) * 100.0 / totalPeriod;
        sys_percent = (systemAllPeriod) * 100.0 / totalPeriod;
        usr_percent = (userPeriod) * 100.0 / totalPeriod;
        idle_percent = (idlePeriod) * 100.0 / totalPeriod;
 
		sum = sys_percent + usr_percent + idle_percent;

		
        sav = cur;
        
        sleep(1);
        
        lseek(fd, 0, SEEK_SET);	
        if((cnt = read(fd, buf, sizeof(buf)-1)) < 0)
            return -1;
        buf[cnt] = '\0';
 
        memset(&cur, 0, sizeof(cur));
        sscanf(buf, "cpu%llu%llu%llu%llu%llu%llu%llu%llu%llu%llu",
                &cur.user, &cur.nice, &cur.system,
                &cur.idle, &cur.iowait, &cur.irq, &cur.softirq,
                &cur.steal, &cur.guest, &cur.guestnice);
 
        cur.total = cur.user + cur.nice + cur.system + cur.idle
            + cur.iowait + cur.irq + cur.softirq + cur.steal;
 
        userPeriod = (cur.user - cur.guest) - (sav.user - sav.guest);
        nicePeriod = (cur.nice - cur.guestnice) - (sav.nice - sav.guestnice);
        systemAllPeriod = (cur.system + cur.irq + cur.softirq)
                            - (sav.system + sav.irq + sav.softirq);
        stealPeriod = cur.steal - sav.steal;
        guestPeriod = (cur.guest + cur.guestnice) - (sav.guest + sav.guestnice);
        totalPeriod = cur.total - sav.total;
        idlePeriod  = cur.idle - sav.idle;
 
        percent = (nicePeriod + userPeriod + systemAllPeriod + stealPeriod + guestPeriod) * 100.0 / totalPeriod;
        sys_percent = (systemAllPeriod) * 100.0 / totalPeriod;
        usr_percent = (userPeriod) * 100.0 / totalPeriod;
        idle_percent = (idlePeriod) * 100.0 / totalPeriod;
 
		sum = sys_percent + usr_percent + idle_percent;
 
        if(sav.total)
        {
			*cpuuse = percent;
           // fprintf(stderr,"cpu_usage: %.2f%% sys:%.1f%% usr:%.1f%%,idle: %.1f%%, sum: %.1f%%.\n", percent, sys_percent, usr_percent, idle_percent, sum);
        }

        
}

/* str2long - safely convert string to int64_t
 *
 * @str: source string to convert from
 * @lng_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) invalid input, (-3) for *flow
 */
int str2long(const char * str, int64_t * lng_ptr)
{
	char * end_ptr = 0;
	if (!str || !lng_ptr)
		return (-1);

	*lng_ptr = 0;
	errno = 0;
	*lng_ptr = strtol(str, &end_ptr, 0);

	if (*end_ptr != '\0')
		return (-2);

	if (errno != 0)
		return (-3);

	return 0;
} /* str2long(...) */

/* str2int - safely convert string to int32_t
 *
 * @str: source string to convert from
 * @int_ptr: pointer where to store result
 *
 * returns zero on success
 * returns (-1) if one of args is NULL, (-2) invalid input, (-3) for *flow
 */
int str2int(const char * str, int32_t * int_ptr)
{
	int rc = 0;
	int64_t arg_long = 0;
	if (!str || !int_ptr)
		return (-1);

	if ( (rc = str2long(str, &arg_long)) != 0 ) {
		*int_ptr = 0;
		return rc;
	}

	if (arg_long < INT32_MIN || arg_long > INT32_MAX)
		return (-3);

	*int_ptr = (int32_t)arg_long;
	return 0;
} /* str2int(...) */
