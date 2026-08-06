
#define  TOOLS_NAME     "sysmonitortool"
#define  CopyRight      "(C)Copyright 2020-2026 ."
#define  TOOL_DEBUG     0
#define  ESC            0x1B
#define  TOOLS_VER      "V1.1"
#define SERIAL_BM_MAX_BUFFER_SIZE 250
#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))


extern unsigned char verbose;
extern unsigned int baudrate ;
extern unsigned short flow_ctrl_flag ;
extern unsigned short bits ;
extern char parity ;
extern unsigned short stop;
extern char flow ;
extern unsigned int timeout;
extern unsigned int retry;
extern int fd;


unsigned char serial_bm_get_escaped_char(unsigned char c);
int serial_bm_wait_for_data(int fd);
int serial_bm_send_msg(int fd, unsigned char * msg, int msg_len);
void serial_close(int fd);
int serial_open(const char *p_path, int baudrate, int bits, char parity, int stop, char flow);
int serial_bm_flush(int fd);
