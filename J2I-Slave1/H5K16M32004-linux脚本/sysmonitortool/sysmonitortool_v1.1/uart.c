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
#include <poll.h>
#include <termios.h>

#include <fcntl.h>

#include <getopt.h>             /* getopt_long() */
#include <errno.h>
#include <sys/time.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include "uart.h"



/*
 *	Table of supported baud rates
 */
static const struct {
	int baudinit;
	unsigned int baudrate;
} rates[] = {
	{ B2400, 2400 },
	{ B9600, 9600 },
	{ B19200, 19200 },
	{ B38400, 38400 },
	{ B57600, 57600 },
	{ B115200, 115200 },
	{ B230400, 230400 },
	{ B460800, 460800 },
	{ B500000, 500000 },
	{ B921600, 921600 },
};

/*
 *	Flush the buffers
 */
int serial_bm_flush(int fd)
{
#if defined(TCFLSH)
    return ioctl(fd, TCFLSH, TCIOFLUSH);
#elif defined(TIOCFLUSH)
    return ioctl(fd, TIOCFLUSH);
#else
#   error "unsupported platform, missing flush support (TCFLSH/TIOCFLUSH)"
#endif
}


int serial_open(const char *p_path, int baudrate, int bits, char parity, int stop, char flow)
{
	struct termios ti;
	unsigned int rate = baudrate;
	char *p;
	int i;
	int ret = 0;
	
	if (!p_path) {
		fprintf(stderr, "Serial device is not specified");
		return -1;
	}


	fd = open(p_path, O_RDWR | O_NONBLOCK, 0);
	if (fd < 0) {
		fprintf(stderr, "Could not open device at %s", p_path);
		return -1;
	}

	for (i = 0; i < ARRAY_SIZE(rates); i++) {
		if (rates[i].baudrate == rate) {
			break;
		}
	}
	if (i >= ARRAY_SIZE(rates)) {
		fprintf(stderr, "Unsupported baud rate %i specified", rate);
		return -1;
	}
	memset(&ti, 0, sizeof(ti));
	ret = tcgetattr(fd, &ti);
    if (ret == -1) {
        fprintf(stderr,"tcgetattr failed\n");
        return -1;
    }
	cfsetispeed(&ti, rates[i].baudinit);
	cfsetospeed(&ti, rates[i].baudinit);
	//cfsetspeed(&ti, rates[i].baudinit);


    switch (bits) {
        case 5:    
            ti.c_cflag &= ~CSIZE;
            ti.c_cflag |= CS5;
        break;
 
        case 6:    
            ti.c_cflag &= ~CSIZE;
            ti.c_cflag |= CS6;
        break;
 
        case 7:    
            ti.c_cflag &= ~CSIZE;
            ti.c_cflag |= CS7;
        break;
 
        case 8:     
            ti.c_cflag &= ~CSIZE;
            ti.c_cflag |= CS8;
        break;
 
        default:
            fprintf(stderr,"Data bits not supported\n");
            return -1;
    }
	
    switch (parity) {
        case 'n':  
        case 'N':
            ti.c_cflag &= ~PARENB;
            ti.c_iflag &= ~INPCK;      
        break;
 
        case 'o': 
        case 'O':
            ti.c_cflag |= PARENB;
            ti.c_cflag |= PARODD;
            ti.c_iflag |= INPCK;    
            ti.c_iflag |= ISTRIP;     
        break;
 
        case 'e':  
        case 'E':
            ti.c_cflag |= PARENB;
            ti.c_cflag &= ~PARODD;
            ti.c_iflag |= INPCK;    
            ti.c_iflag |= ISTRIP;     
        break;
 
        default:
            fprintf(stderr,"Parity not supported\n");
            return -1;
    }
 
    switch (stop) {
        case 1: ti.c_cflag &= ~CSTOPB; break; /* 1个停止位 */
        case 2: ti.c_cflag |= CSTOPB;  break; /* 2个停止位 */
        default: fprintf(stderr,"Stop bits not supported\n");
    }
    
    switch (flow) {
        case 'n':
        case 'N':   
			ti.c_cflag &= ~CRTSCTS;
			ti.c_iflag &= ~(IGNBRK | IGNCR | INLCR | ICRNL | INPCK | ISTRIP
					| IXON | IXOFF | IXANY);
        break;
 
        case 'h':
        case 'H':  
            ti.c_cflag |= CRTSCTS;
            ti.c_iflag &= ~(IXON | IXOFF | IXANY);
        break;
 
        case 's':
        case 'S':   
            ti.c_cflag &= ~CRTSCTS;
            ti.c_iflag |= (IXON | IXOFF | IXANY);
        break;
 
        default:
            fprintf(stderr,"Flow control parameter error\n");
            return -1;
    }
 
 
	/* enable the receiver and set local mode */
	ti.c_cflag |= (CLOCAL | CREAD);

	ti.c_oflag &= ~(OPOST);
	ti.c_lflag &= ~(ICANON | ISIG | ECHO | ECHONL | NOFLSH);

	/* set the new options for the port with flushing */
	tcsetattr(fd, TCSAFLUSH, &ti);


	return fd;
}

/*
 *	Close serial interface
 */
void serial_close(int fd)
{

	close(fd);

}

#if 0
/*
 *	Send message to serial port
 */
int serial_bm_send_msg(int fd, unsigned char * msg, int msg_len)
{
	int  tmp = 0;
	if (verbose == 1)
		fprintf(stderr, "serial_bm_send_msg\n");
	/* write data to serial port */
	tmp = write(fd, msg, msg_len);
	
	if (tmp <= 0) {
		fprintf(stderr,"ipmitool: write error");
		return -1;
	}

	return 0;
}
#endif



/*
 *	Send message to serial port
 */
int serial_bm_send_msg(int fd, unsigned char * msg, int msg_len)
{
	int i, size, tmp = 0;
	unsigned char * buf, * data;

	if (verbose == 1) 
	{
		fprintf(stderr, "Sending request:\n");
		fprintf(stderr, "  rsSA         = 0x%x\n", msg[0]);
		fprintf(stderr, "  NetFN/rsLUN  = 0x%x\n", msg[1]);
		fprintf(stderr, "  rqSA         = 0x%x\n", msg[3]);
		fprintf(stderr, "  rqSeq/rqLUN  = 0x%x\n", msg[4]);
		fprintf(stderr, "  cmd          = 0x%x\n", msg[5]);
		if (msg_len > 7) {
			fprintf(stderr, "  data_len     = %d\n", msg_len - 7);
			for (i = 0; i < (msg_len - 7); i++) 
				fprintf(stderr,"data = %x ", msg[6 + i]);
			fprintf(stderr, "\n");
		}

	}

	/* calculate escaped characters number */
	for (i = 0; i < msg_len; i++) 
	{
		if (serial_bm_get_escaped_char(msg[i]) != msg[i]) 
		{
			tmp++;
		}
	}

	/* calculate required buffer size */
	size = msg_len + tmp + 2;

	/* allocate buffer for output data */
	buf = data = (unsigned char *) alloca(size);

	if (!buf) {
		fprintf(stderr, "ipmitool: alloca error");
		return -1;
	}

	/* start character */
	*buf++ = 0xA0;

	for (i = 0; i < msg_len; i++) 
	{
		tmp = serial_bm_get_escaped_char(msg[i]);
		if (tmp != msg[i]) 
		{
			*buf++ = 0xAA;
		}

		*buf++ = tmp;
	}

	/* stop character */
	*buf++ = 0xA5;

	if (verbose == 1) 
	{
		for(i=0;i<size;i++)
		fprintf(stderr, "%x ", data[i]);
		fprintf(stderr, "\n");
	}

	/* write data to serial port */
	tmp = write(fd, data, size);
	if (tmp <= 0) 
	{
		fprintf(stderr, "ipmitool: write error");
		return -1;
	}

	return 0;
}

/*
 *	This function waits for incoming data
 */
int serial_bm_wait_for_data(int fd)
{
	int n;
	struct pollfd pfd;

	pfd.fd = fd;
	pfd.events = POLLIN;
	pfd.revents = 0;

	n = poll(&pfd, 1, timeout * 1000);
	if (n < 0) 
	{
		fprintf(stderr, "Poll for serial data failed");
		return -1;
	} 
	else if (!n) 
	{
		return -1;
	}
	return 0;
}



