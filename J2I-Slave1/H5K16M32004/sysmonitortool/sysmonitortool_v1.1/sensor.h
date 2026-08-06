
#define CC_SUCCESS                                      0x00

#define TRUE	1
#define FALSE	0


#define BSWAP_16(x) (x)
#define BSWAP_32(x) (x)

#define BM_START		0xA0
#define BM_STOP			0xA5
#define BM_HANDSHAKE	0xA6
#define BM_ESCAPE		0xAA

#define GETTEMPFMFLAG	0
#define GETTEMPFSFLAG	1

#define GET_FAN_CMD_FLAG	0X01
#define SET_FAN_CMD_FLAG	0X02

#define GET_WDT_CMD_FLAG	0X01
#define RESET_WDT_CMD_FLAG	0X02
#define OFF_WDT_CMD_FLAG	0X03
#define SET_WDT_CMD_FLAG	0X04
#define GET_SYSRESET_CMD_FLAG	0X01
#define SET_SYSRESET_CMD_FLAG	0X02
/*
 *	State for the received message
 */
enum {
	MSG_NONE,
	MSG_IN_PROGRESS,
	MSG_DONE
};


/*
 *	Receiving context
 */
struct serial_bm_recv_ctx {
	unsigned char buffer[SERIAL_BM_MAX_BUFFER_SIZE];
	size_t buffer_size;
	size_t max_buffer_size;
};


/*
 *	Message parsing context
 */
struct  serial_bm_parse_ctx{
	int state;
	unsigned char * msg;
	size_t msg_len;
	size_t max_len;
	int escape;
};

typedef struct MNinfoData_S
{

	unsigned int v12vsbdata;
	unsigned int ddr1v2data;
	unsigned int vcoredata;
	unsigned int vcc5data;
	unsigned int vcc3data;


	signed char amb1data;
	unsigned char amb1fradata;
	signed char amb2data;
	unsigned char amb2fradata;
	signed char cpu1data;
	signed char cpu2data;
	unsigned int cpu112vp;
	unsigned int cpu112vc;
	unsigned short fan1tachdata;
	unsigned short fan2tachdata;
	unsigned short fan3tachdata;
	unsigned short fan4tachdata;
	unsigned char psustate;
	unsigned char MajorVersion;
	unsigned char MinorVersion;
	unsigned char CpldMajorVersion;
	unsigned char CpldMinorVersion;
} __attribute__( ( __packed__ ) )MNinfoData;


typedef struct GetOemFANinfoRes_S
{
	unsigned char		CompletionCode;
	unsigned char		fanmode;
	unsigned char		fanduty[4];
	unsigned short		fanspeed[4];

}__attribute__( ( __packed__ ) ) GetOemFANinfoRes_T;

/* SetWDTReq_T */
typedef struct SetWDTReq_S
{
    unsigned char   TmrUse;
    unsigned char   TmrActions;
    unsigned char   PreTimeOutInterval;
    unsigned char   ExpirationFlag;
    unsigned short  InitCountDown;

} __attribute__( ( __packed__ ) ) SetWDTReq_T;

/* GetWDTRes_T */
typedef struct GetWDTRes_S
{
    unsigned char       CompletionCode;
    unsigned char   TmrUse;
    unsigned char   TmrActions;
    unsigned char   PreTimeOutInterval;
    unsigned char   ExpirationFlag;
    unsigned short  InitCountDown;
    unsigned short      PresentCountDown;

} __attribute__( ( __packed__ ) ) GetWDTRes_T;


/* GetOemP0RESETRes_T */
typedef struct GetOemP0RESETRes_S
{
    unsigned char       CompletionCode;
    unsigned char   mode;

} __attribute__( ( __packed__ ) ) GetOemP0RESETRes_T;

#define FAN_AUTO_MODE 0
#define FAN_MANUAL_MODE 1
extern MNinfoData Monitorviewdata;


int Get_Bmc_Info_Handle(unsigned char mode);

int fan_Handle(char cmdmode,unsigned char fanmode,unsigned char *fanduty);
int Wdt_Handle(char mode,unsigned short timeout);
int Set_LED_Handle(char port);
int powermode_Handle(unsigned char powermode);
int Set_sysreset_Handle(unsigned char p0resetcmdmode,unsigned char p0resetdata);
