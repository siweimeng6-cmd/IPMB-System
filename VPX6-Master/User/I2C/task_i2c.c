#include "task_i2c.h"
#include "bsp_pwm.h"
#include "bsp_gpio.h"
#include "bsp_bpd20550.h"
#include "task_slot_cache.h"   /* 网页控制台改造(2026-07-27):PEM主动上报缓存,见下方两处 IPMB_Test_Command_Task 分支 */
#include "task_ctrl_dispatch.h" /* 同上:异步控制通道专属响应mailbox,见下方 I2C1_Task/I2C2_Task 响应分流 */

TaskHandle_t I2C1_Task_Handle = NULL;
TaskHandle_t I2C2_Task_Handle = NULL;
TaskHandle_t Temp_Task_Handle = NULL;
TaskHandle_t IPMB_Test_Cmd_Task_Handle = NULL;
TaskHandle_t IPMB_Discovery_Task_Handle = NULL;

/* BPD20550 最近一次采集到的输入电压 / 输出电流 / 芯片温度
 * 由 Board_BPD20550_current() 写入,本任务只读 */
static float s_bpd_vin  = 0.0f;
static float s_bpd_iout = 0.0f;
static float s_bpd_temp = 0.0f;

extern int board_temp0[2];
extern int board_temp1[2];
extern int board_temp2[2];

extern void set_pwm_duty(uint8_t channel, uint16_t duty);
extern uint16_t calculate_fan_duty(float temp);
extern uint16_t calculate_fan_rpm(TIM_ICUserValueTypeDef *tach_struct);

/* 风扇占空比:改由串口 fan <0-100> 命令手动设置,4 路共用一个值,不再按温度自动调速。
 * 默认 30%(与原温控最低档 PWM_MIN 一致),开机风扇即低速运转。 */
uint16_t g_fan_duty = 30;

/* 设置 4 路风扇占空比并立即下发(供串口 fan 命令调用,改完立刻生效,
 * 不用等 Temp_Task 的 2 秒周期)。Temp_Task 每周期也会重新下发 g_fan_duty。
 * 放在 set_pwm_duty extern 声明之后,避免隐式声明冲突。 */
void Fan_Set_All_Duty(uint16_t duty)
{
    if (duty > 100) duty = 100;
    g_fan_duty = duty;
    set_pwm_duty(OCPWM1_CHANNEL, duty);
    set_pwm_duty(OCPWM2_CHANNEL, duty);
    set_pwm_duty(OCPWM3_CHANNEL, 100-duty);
    set_pwm_duty(OCPWM4_CHANNEL, duty);
}

xTaskHandle I2C1ReceiveSemaphore    = NULL;
xTaskHandle I2C2ReceiveSemaphore    = NULL;

xQueueHandle xIPMB_CmdQueue = NULL;
xQueueHandle xIPMB_RspQueue = NULL;
xQueueHandle xIPMB_CmdQueue2 = NULL;
xQueueHandle xIPMB_RspQueue2 = NULL;

/* --- 业务地址(7bit) ---
 *   HOST_IPMB_ADDR_7BIT: 主控板 IPMB 地址(本机是主控),协议中"主控板 IPMB 地址 0x10h"
 *   g_slave_ipmb_addr: 业务板卡 IPMB 地址(本机寻址的对端), 不再固定 0x8E ——
 *     业务板地址改由槽位号动态决定(0x30 + Slot_ID<<1), 由 IPMB_Discovery_Task
 *     后台扫描发现后写入, 发现完成前保留 0x8E 作为历史默认值兜底.
 */
#define HOST_IPMB_ADDR_7BIT    0x10
volatile uint8_t g_slave_ipmb_addr = 0x8E;

/* 发现地址表(见 task_i2c.h): 每轮扫描把所有应答的从机地址去重记入,
 * 供 IPMB_PEM_Poll_Task 遍历轮询每一块从机。g_slave_count=0 表示尚未发现。 */
volatile uint8_t g_slave_addrs[IPMB_MAX_SLAVES] = {0};
volatile uint8_t g_slave_count = 0;

/* 地址发现探测专用的 rqSA 标记(区别于 HOST_IPMB_ADDR_7BIT), 供
 * IPMB_Test_Command_Task 识别探测响应并分流到发现任务专属 mailbox,
 * 不与正常用户命令的回显混在一起。用 0x00(IPMB/SMBus 保留广播地址,
 * 真实主机不会以此为 rqSA)做标记,避免和 0x30~0x6E 候选从机地址范围混淆 */
#define IPMB_DISC_HOST_ADDR_7BIT   0x00
xQueueHandle xIPMB_DiscRspQueue  = NULL;
xQueueHandle xIPMB_DiscRspQueue2 = NULL;

/* 网页监控轮询专属响应 mailbox,见 task_i2c.h 里 IPMB_WEB_HOST_ADDR_7BIT 的说明 */
xQueueHandle xIPMB_WebRspQueue  = NULL;
xQueueHandle xIPMB_WebRspQueue2 = NULL;

/* PEM 主动上报专用队列,见 task_i2c.h 顶部说明 */
xQueueHandle xIPMB_PemQueue  = NULL;
xQueueHandle xIPMB_PemQueue2 = NULL;

/* ============================================================
 * 主机端 IPMB 仲裁: A 优先 / A 失败超时切 B / A 恢复自动切回
 *   - 普通命令(rqSA=0x20)只在当前活跃总线发送, 不再同时双发;
 *   - 发现探测(rqSA=IPMB_DISC_HOST_ADDR_7BIT)不受活跃总线限制, 用于
 *     探知从机在哪一路可达, 其超时静默(探测 miss 属正常);
 *   - g_host_active_bus 由 IPMB_Discovery_Task 的"先A后B"发现结果驱动,
 *     并由普通命令连续超时(≥IPMB_HOST_FAILOVER_CNT)触发快速切换。
 * ============================================================ */
volatile Host_Bus_t g_host_active_bus = HOST_BUS_A;   /* 上电 A 优先; Host_Bus_t 定义已移到 task_i2c.h,
                                                          供 bsp_httpd.c 的 /api/v1/health 端点读取 */
#define IPMB_HOST_FAILOVER_CNT   3                    /* 活跃总线普通命令连续超时阈值 */
static uint8_t s_host_a_fail = 0;
static uint8_t s_host_b_fail = 0;

uint8_t power_on_cnt;

static void read_current(void);

/* ============================================================
 * IPMI 响应辅助
 * ============================================================ */
#define IPMI_MAX_DATA_LEN   48
typedef struct {
	uint8_t resp_addr;
	uint8_t netfn_lun;
	uint8_t chk_head;
	uint8_t req_addr;
	uint8_t rqseq_lun;
	uint8_t cmd;
	uint8_t data[IPMI_MAX_DATA_LEN];
	uint8_t data_len;
	uint8_t chk_tail;
} ipmi_resp_t;

static uint8_t ipmi_calc_chk(const uint8_t *data, uint16_t start, uint16_t end_exclusive)
{
	uint8_t s = 0;
	uint16_t i;
	for (i = start; i < end_exclusive; i++) s = (uint8_t)(s + data[i]);
	return (uint8_t)(-s);
}

static uint8_t ipmi_verify_chk(const uint8_t *data, uint16_t total_len)
{
	if (total_len < 4) return 0;
	if (data[2] != ipmi_calc_chk(data, 0, 2)) return 0;
	if (data[total_len - 1] != ipmi_calc_chk(data, 3, (uint16_t)(total_len - 1))) return 0;
	return 1;
}

/**
 * @brief  判断一帧是否是 PEM(Platform Event Message)主动上报/轮询答复帧
 *         形状:从机侧统一用 IPMB_Build_Request 风格拼这条帧(netFn=04h 原始
 *         值、不带响应位,cmd=0x02),字节[1]的高6位(netFn)==0x04、cmd==0x02、
 *         校验和通过。轮询答复和真正的自主推送在这一层完全同形,只能靠
 *         "是不是在等一条匹配的请求"这个时机区分,由调用方(SendRequest 的
 *         丢帧点、I2C1/2_Task 的空闲分支)各自决定命中时要不要转发。
 */
uint8_t is_pem_push_frame(const uint8_t *buf, uint16_t len)
{
	if (buf == NULL || len < 6) return 0;
	if ((buf[1] & 0xFC) != 0x10) return 0;   /* netFn=04h<<2=0x10, lun 位不管 */
	if (buf[5] != 0x02) return 0;             /* cmd = Platform Event Message */
	if (!ipmi_verify_chk(buf, len)) return 0;
	return 1;
}

static void ipmi_fill_resp(ipmi_resp_t *r,
                           uint8_t resp_addr,
                           uint8_t req_netfn,
                           uint8_t req_addr,
                           uint8_t rq_seq_lun_byte,
                           uint8_t cmd,
                           const uint8_t *data, uint8_t data_len)
{
	uint8_t s = 0;
	uint16_t i;
	uint16_t end;

	r->resp_addr   = resp_addr;
	r->netfn_lun   = (uint8_t)((req_netfn + 0x04) << 2);
	r->req_addr    = req_addr;
	r->rqseq_lun   = rq_seq_lun_byte;
	r->cmd         = cmd;
	if (data_len > IPMI_MAX_DATA_LEN) data_len = IPMI_MAX_DATA_LEN;
	if (data && data_len) {
		for (i = 0; i < data_len; i++) r->data[i] = data[i];
	}
	r->data_len = data_len;

	r->chk_head = (uint8_t)(-(r->resp_addr + r->netfn_lun));

	end = (uint16_t)(3 + 1 + 1 + 1 + data_len); /* byte3..data end */
	for (i = 3; i < end; i++) s = (uint8_t)(s + ((uint8_t *)r)[i]);
	r->chk_tail = (uint8_t)(-s);
}

static uint8_t ipmi_resp_to_buf(const ipmi_resp_t *r, uint8_t *out)
{
	uint8_t total = 0;
	uint16_t i;
	out[total++] = r->resp_addr;
	out[total++] = r->netfn_lun;
	out[total++] = r->chk_head;
	out[total++] = r->req_addr;
	out[total++] = r->rqseq_lun;
	out[total++] = r->cmd;
	for (i = 0; i < r->data_len; i++) out[total++] = r->data[i];
	out[total++] = r->chk_tail;
	return total;
}

/* ============================================================
 * 业务响应构造器
 * ============================================================ */
static void resp_get_device_id(const uint8_t *req, uint8_t req_len, ipmi_resp_t *r)
{
	uint8_t data[12];
	(void)req; (void)req_len;

	data[0]  = 0x00;
	data[1]  = 0x51;
	data[2]  = 0x02;
	data[3]  = 0xD2;
	data[4]  = 0x51;
	data[5]  = 0x51;
	data[6]  = 0x23;
	data[7]  = 0x00;
	data[8]  = 0x00;
	data[9]  = 0x00;
	data[10] = 0x00;
	data[11] = 0x00;

	ipmi_fill_resp(r,
		(uint8_t)(HOST_IPMB_ADDR_7BIT << 1),
		0x06, req[3], req[4], 0x01,
		data, sizeof(data));
}

static void resp_get_device_sdr(const uint8_t *req, uint8_t req_len, ipmi_resp_t *r)
{
	uint8_t data[32];
	uint8_t rec[16];
	uint8_t i;
	(void)req_len;

	rec[0] = 0x00; rec[1] = 0x00; rec[2] = 0x01; rec[3] = 0x00;
	rec[4] = 0x12;
	rec[5]  = 0x00; rec[6]  = 0x00; rec[7]  = 0x00; rec[8]  = 0x00;
	rec[9]  = 0x04;
	rec[10] = 0x01;
	rec[11] = 0x00;
	rec[12] = 0xC0;
	rec[13] = 0x40;
	rec[14] = 0x01;
	rec[15] = 0x01;

	data[0] = 0x00;
	data[1] = 0xFF;
	for (i = 0; i < sizeof(rec); i++) data[2 + i] = rec[i];

	ipmi_fill_resp(r,
		(uint8_t)(HOST_IPMB_ADDR_7BIT << 1),
		0x04, req[3], req[4], 0x21,
		data, (uint8_t)(2 + sizeof(rec)));
}

static uint8_t adc_volts_to_byte(uint8_t ch)
{
	float v;
	if (ch >= ADC_CONVERT_CHANNEL) return 0;
	v = stADC_Data.adc_voltage_val[ch];
	if (v < 0) v = 0;
	if (v > 255.0f) v = 255.0f;
	return (uint8_t)v;
}

static uint8_t read_bpd20550_current_byte(void)
{
	uint8_t buf[2] = {0,0};
	float cur = 0;
	bpd20550_start_operation();
	if (bpd20550_ReadBytes(buf, BPD20550_READ_CURRENT, 2) == 1)
	{
		VRLinear11Format((uint16_t)(buf[0] | (buf[1] << 8)), &cur);
		if (cur < 0) cur = 0;
		if (cur > 255.0f) cur = 255.0f;
		return (uint8_t)cur;
	}
	return 0;
}

static uint8_t g_fru_state = 0;
static uint8_t g_fan_default_level = 3;
static uint16_t g_fan_max_rpm = 90;
static uint8_t g_ipmc_sw_major = 2;
static uint8_t g_ipmc_sw_minor = 5;
static uint32_t g_i2c_speed_khz = 400;

static void resp_get_sensor_reading(const uint8_t *req, uint8_t req_len, ipmi_resp_t *r)
{
	uint8_t cc = 0x00;
	uint8_t sensor_no;
	uint8_t data[3];
	uint8_t val = 0x80;

	if (req_len < 7) { cc = 0xC1; goto fill; }
	sensor_no = req[6];

	switch (sensor_no) {
	case 0x04: { int t = board_temp0[0] + 128; val = (uint8_t)((t < 0) ? 0 : (t > 255 ? 255 : t)); break; }
	case 0x20: { int t = board_temp1[0] + 128; val = (uint8_t)((t < 0) ? 0 : (t > 255 ? 255 : t)); break; }
	case 0x21: { int t = board_temp2[0] + 128; val = (uint8_t)((t < 0) ? 0 : (t > 255 ? 255 : t)); break; }
	case 0x22: { int t = board_temp2[0] + 128; val = (uint8_t)((t < 0) ? 0 : (t > 255 ? 255 : t)); break; }
	case 0x03: val = adc_volts_to_byte(0); break;
	case 0x10: val = adc_volts_to_byte(2); break;
	case 0x11: val = adc_volts_to_byte(2); break;
	case 0x12: val = adc_volts_to_byte(2); break;
	case 0x13: val = 0x80; break;
	case 0x14: val = adc_volts_to_byte(1); break;
	case 0x15: val = adc_volts_to_byte(1); break;
	case 0x16: val = adc_volts_to_byte(1); break;
	case 0x17: val = 0x80; break;
	case 0x31: val = adc_volts_to_byte(2); break;
	case 0x19: val = adc_volts_to_byte(3); break;
	case 0x50: val = adc_volts_to_byte(0); break;
	case 0x51: val = adc_volts_to_byte(1); break;
	case 0x52: val = adc_volts_to_byte(1); break;
	case 0x53: val = adc_volts_to_byte(2); break;
	case 0x54: val = adc_volts_to_byte(2); break;
	case 0x55: val = adc_volts_to_byte(2); break;
	case 0x56: val = adc_volts_to_byte(2); break;
	case 0x57: val = adc_volts_to_byte(3); break;
	case 0x58: val = adc_volts_to_byte(3); break;
	case 0x07: val = read_bpd20550_current_byte(); break;
	case 0x30: val = 0x80; break;
	case 0x18: val = 0x80; break;
	case 0x41: val = g_fan_default_level; break;
	default:   val = 0x80; break;
	}

fill:
	data[0] = cc;
	data[1] = val;
	data[2] = 0x80;

	ipmi_fill_resp(r,
		(uint8_t)(HOST_IPMB_ADDR_7BIT << 1),
		0x04, req[3], req[4], 0x2D,
		data, sizeof(data));
}

static void resp_set_fru_activation(const uint8_t *req, uint8_t req_len, ipmi_resp_t *r)
{
	uint8_t cc = 0x00;
	uint8_t data[2];
	uint8_t vso, fru_id, ctrl;

	if (req_len < 9) { cc = 0xC1; data[0] = cc; data[1] = 0x03; goto fill; }
	vso    = req[6];
	fru_id = req[7];
	ctrl   = req[8];
	(void)fru_id;

	switch (ctrl) {
	case 0x00:
		g_fru_state = 0;
		printf("[IPMI] Set FRU: Power OFF\r\n");
		break;
	case 0x01:
		g_fru_state = 1;
		printf("[IPMI] Set FRU: Power ON\r\n");
		break;
	case 0x02:
	case 0x04:
		BMC_OUT_SYS_RST_Reset();
		break;
	case 0x03:
	case 0x05:
	case 0x06:
		printf("[IPMI] Set FRU: ctrl=0x%02X\r\n", ctrl);
		break;
	case 0x07:
		if (req_len > 9) g_fan_default_level = req[9];
		break;
	case 0x08:
		if (req_len > 9) g_fan_max_rpm = req[9];
		break;
	case 0x09:
		if (req_len > 9) {
			g_ipmc_sw_major = req[9] & 0x0F;
			g_ipmc_sw_minor = (req[9] >> 4) & 0x0F;
		}
		break;
	case 0x0A:
		g_i2c_speed_khz = 100;
		break;
	case 0x0B:
		g_i2c_speed_khz = 400;
		break;
	default:
		cc = 0xC1;
		break;
	}

fill:
	data[0] = cc;
	data[1] = vso;
	ipmi_fill_resp(r,
		(uint8_t)(HOST_IPMB_ADDR_7BIT << 1),
		0x2C, req[3], req[4], 0x0C,
		data, sizeof(data));
}

static void resp_get_board_system_status(const uint8_t *req, uint8_t req_len, ipmi_resp_t *r)
{
	uint8_t data[12];
	uint16_t cpu_use = 15, mem_use = 15;
	int t;
	(void)req_len;

	data[0]  = 0x00;
	data[1]  = 0x01;
	data[2]  = 0x01;
	data[3]  = (uint8_t)(cpu_use & 0xFF);
	data[4]  = (uint8_t)(cpu_use >> 8);
	data[5]  = (uint8_t)(mem_use & 0xFF);
	data[6]  = (uint8_t)(mem_use >> 8);
	data[7]  = 0xFF; data[8]  = 0xFF;
	data[9]  = 0xFF; data[10] = 0xFF;
	t = board_temp0[0] + 128;
	if (t < 0) t = 0; if (t > 255) t = 255;
	data[11] = (uint8_t)t;

	ipmi_fill_resp(r,
		(uint8_t)(HOST_IPMB_ADDR_7BIT << 1),
		0x06, req[3], req[4], 0x16,
		data, sizeof(data));
}

static void resp_get_slot(const uint8_t *req, uint8_t req_len, ipmi_resp_t *r)
{
	uint8_t data[2];
	(void)req_len;
	data[0] = 0x00;
	data[1] = 0x01;
	ipmi_fill_resp(r,
		(uint8_t)(HOST_IPMB_ADDR_7BIT << 1),
		0x06, req[3], req[4], 0x15,
		data, sizeof(data));
}

static void resp_platform_event(const uint8_t *req, uint8_t req_len, ipmi_resp_t *r)
{
	uint8_t data[1];
	(void)req_len;
	data[0] = 0x00;
	ipmi_fill_resp(r,
		(uint8_t)(HOST_IPMB_ADDR_7BIT << 1),
		0x04, req[3], req[4], 0x02,
		data, sizeof(data));
}

typedef struct {
	uint8_t netfn;
	uint8_t cmd;
	void (*handler)(const uint8_t *req, uint8_t req_len, ipmi_resp_t *r);
} ipmi_route_t;

static const ipmi_route_t g_ipmi_routes[] = {
	{ 0x06, 0x01, resp_get_device_id          },
	{ 0x04, 0x21, resp_get_device_sdr         },
	{ 0x04, 0x2D, resp_get_sensor_reading     },
	{ 0x2C, 0x0C, resp_set_fru_activation     },
	{ 0x06, 0x16, resp_get_board_system_status},
	{ 0x06, 0x15, resp_get_slot               },
	{ 0x04, 0x02, resp_platform_event         },
};
#define IPMI_ROUTE_CNT (sizeof(g_ipmi_routes)/sizeof(g_ipmi_routes[0]))

static uint8_t ipmi_route(const uint8_t *req, uint8_t req_len, ipmi_resp_t *r)
{
	uint8_t netfn = (req[1] >> 2) & 0x3F;
	uint8_t cmd = req[5];
	uint8_t i;
	for (i = 0; i < IPMI_ROUTE_CNT; i++) {
		if (g_ipmi_routes[i].netfn == netfn && g_ipmi_routes[i].cmd == cmd) {
			g_ipmi_routes[i].handler(req, req_len, r);
			return 1;
		}
	}
	return 0;
}

/* ============================================================
 * Tasks
 * ============================================================ */
void I2C1_Task(void* parameter)
{
#if (IPMB_A_ROLE == IPMB_A_SLAVE)
	while (1)
	{
		if (xSemaphoreTake(I2C1ReceiveSemaphore, (TickType_t)0) == pdTRUE)
		{
			uint8_t *frm = i2c1_recv_data_struct.recv_buf;
			uint16_t len = i2c1_recv_data_struct.recv_data_len;
			ipmi_resp_t resp;
			uint8_t out[64];
			uint8_t out_len;
			uint8_t chk_ok;
			uint16_t k;

			printf("I2C1(Slave) RX len=%d: ", len);
			for (k = 0; k < len; k++) printf("%02X ", frm[k]);
			printf("\r\n");

			if (len < 6) {
				printf("I2C1(Slave): frame too short\r\n");
				continue;
			}
			chk_ok = ipmi_verify_chk(frm, (uint16_t)len);
			if (!chk_ok) {
				printf("I2C1(Slave): checksum error, drop\r\n");
				continue;
			}

			if (!ipmi_route(frm, (uint8_t)len, &resp)) {
				uint8_t data[1] = { 0xC1 };
				ipmi_fill_resp(&resp,
					(uint8_t)(HOST_IPMB_ADDR_7BIT << 1),
					(frm[1] >> 2) & 0x3F,
					frm[3], frm[4], frm[5],
					data, 1);
			}

			out_len = ipmi_resp_to_buf(&resp, out);
			printf("I2C1(Slave) TX len=%d: ", out_len);
			for (k = 0; k < out_len; k++) printf("%02X ", out[k]);
			printf("\r\n");

			I2C1_SlaveSendResponse(out, out_len);
		}
		vTaskDelay(2);
	}
	#else
		/* IPMB-A (I2C1) 主机模式:从队列取命令,发请求,收响应,回传 */
		ipmb_pkt_t pkt;
		uint8_t rx_target_len;

		while (1)
		{
			Ipmb_I2C1_ApplyPendingSpeed();   /* 消费"设置I2C速率"控制命令留下的待生效标志,见 bsp_i2c.c */

			if (xQueueReceive(xIPMB_CmdQueue, &pkt, (TickType_t)10) == pdTRUE)
			{
				uint8_t is_disc;
				if (pkt.len < 6) {
					Usart_SendString(IPMB_TEST_COM, "<ERR 1 badlen>\r\n");
					continue;
				}

				/* 主机端仲裁: 普通命令只在活跃总线发送; 发现探测(rqSA=0x00)不受限 */
				is_disc = (pkt.buf[3] == IPMB_DISC_HOST_ADDR_7BIT);
				if (!is_disc && g_host_active_bus != HOST_BUS_A)
					continue;   /* 非活跃总线: 普通命令丢弃不发 */

				/* 根据 IPMI 命令估算响应长度(头 6 字节 + 数据 + 校验) */
				{
					uint8_t cmd = pkt.buf[5];
					switch (cmd) {
					case 0x01: rx_target_len = 19; break;  /* Get Device ID:8+11 */
					case 0x21: {   /* Get Device SDR:长度视请求的offset/读取长度而定,按记录最多16字节估算 */
						uint8_t offset, rlen, avail, copy_len;
						if (pkt.len >= 12) {
							offset = pkt.buf[10];
							rlen   = pkt.buf[11];
							avail  = (offset < 16) ? (uint8_t)(16 - offset) : 0;
							copy_len = (rlen == 0xFF || rlen > avail) ? avail : rlen;
							rx_target_len = (uint8_t)(10 + copy_len);
						} else {
							rx_target_len = 10;   /* 请求过短,响应固定10字节(长度校验失败) */
						}
					} break;
					case 0x2D: rx_target_len = 11; break;  /* Get Sensor Reading:8+3 */
					case 0x0C: rx_target_len = 9;  break;  /* Set FRU Activation:8+1 */
					case 0x16: rx_target_len = 22; break;  /* Get Board Status:8+14 */
					case 0x15: rx_target_len = 9;  break;  /* Get Slot:8+1 */
					case 0x02: rx_target_len = 14; break;  /* Platform Event(PEM轮询):模拟主动上报帧,无完成码,6头+7data+1尾校验 */
					case 0x12: {   /* OEM Get Module Info:长度由请求 item 列表推算 */
						uint8_t ic = (pkt.len > 6) ? pkt.buf[6] : 0, q;
						uint16_t n = 6 + 1 + 1 + 1;                 /* 头6 + cc + item_count + cs */
						for (q = 0; q < ic && (uint16_t)(7 + q) < pkt.len; q++) {
							uint8_t id = pkt.buf[7 + q];
							if      (id == 0x11) n += 3 + 5 * 16;   /* 传感器信息 3+80 */
							else if (id == 0x12) n += 3 + 5 * 4;    /* 传感器值   3+20 */
							else                 n += 3;
						}
						rx_target_len = (n > BUFFER_RX_SIZE) ? BUFFER_RX_SIZE : (uint8_t)n;
					} break;
					default:   rx_target_len = 25; break;  /* 安全上界,响应短则读到 0xFF 填充 */
					}
					if (rx_target_len > BUFFER_RX_SIZE) rx_target_len = BUFFER_RX_SIZE;
				}

				i2c1_recv_data_struct.recv_data_len = 0;
				I2C1_SendRequest(pkt.buf, pkt.len, rx_target_len, pkt.buf[0]);

				{
					uint16_t waited_ms = 0;
					while (waited_ms < IPMB_TEST_TIMEOUT_MS * 2)
					{
						vTaskDelay(1);
						waited_ms++;
						if (i2c1_recv_data_struct.recv_data_len >= rx_target_len)
							break;
					}
					if (i2c1_recv_data_struct.recv_data_len == 0) {
						if (!is_disc) {   /* 探测 miss 属正常, 静默; 仅普通命令超时计失败 */
							// Usart_SendString(IPMB_TEST_COM, "<ERR 2 timeout>\r\n");   /* 日志太吵,注释掉打印,失败计数/failover逻辑保留 */
							if (++s_host_a_fail >= IPMB_HOST_FAILOVER_CNT) {
								g_host_active_bus = HOST_BUS_B;
								s_host_a_fail = 0;
								printf("[IPMB-HOST-ARB] A timeout, switch to B\r\n");
							}
						}
					} else if (!is_disc) {
						s_host_a_fail = 0;   /* 普通命令成功, 清零失败计数 */
					}
				}

				{
					ipmb_pkt_t rsp;
					uint8_t i;
					rsp.len = i2c1_recv_data_struct.recv_data_len;
					if (rsp.len > IPMB_TEST_BUF_SIZE) rsp.len = IPMB_TEST_BUF_SIZE;
					for (i = 0; i < rsp.len; i++) rsp.buf[i] = i2c1_recv_data_struct.recv_buf[i];
					if (rsp.len > 0) {   /* 空响应(探测 miss / 重试耗尽的真超时)都不入队, 避免空 <RSP-A> */
						if (rsp.len >= 4 && rsp.buf[0] == IPMB_DISC_HOST_ADDR_7BIT) {
							/* 探测响应直接送发现 mailbox,绕开会被 USART 阻塞的打印任务,
							   否则 Temp_Task 占串口时发现响应转发不及时,会丢掉从机(如0x30) */
							if (xIPMB_DiscRspQueue != NULL) xQueueOverwrite(xIPMB_DiscRspQueue, &rsp);
						} else if (rsp.len >= 4 && rsp.buf[0] == IPMB_WEB_HOST_ADDR_7BIT) {
							/* 网页监控轮询的响应,送专属 mailbox,避免和 IPMB_Test_Command_Task
							   抢 xIPMB_RspQueue */
							if (xIPMB_WebRspQueue != NULL) xQueueOverwrite(xIPMB_WebRspQueue, &rsp);
						} else if (rsp.len >= 4 && rsp.buf[0] == IPMB_CTRL_HOST_ADDR_7BIT) {
							/* 网页控制台异步控制命令的响应,送专属 mailbox,同样避免抢 xIPMB_RspQueue */
							if (xIPMB_CtrlRspQueue != NULL) xQueueOverwrite(xIPMB_CtrlRspQueue, &rsp);
						} else {
							xQueueSend(xIPMB_RspQueue, &rsp, 0);   /* 深度5 FIFO: 0超时,满则丢新,不阻塞 I2C 任务 */
						}
					}
				}
			}
			vTaskDelay(1);
		}
	#endif
	}

void I2C2_Task(void* parameter)
{
	ipmb_pkt_t pkt;
	uint8_t rx_target_len;

	while (1)
	{
		Ipmb_I2C2_ApplyPendingSpeed();   /* 消费"设置I2C速率"控制命令留下的待生效标志,见 bsp_i2c.c */

		if (xQueueReceive(xIPMB_CmdQueue2, &pkt, (TickType_t)10) == pdTRUE)
		{
			uint8_t is_disc;
			if (pkt.len < 6) {
				Usart_SendString(IPMB_TEST_COM, "<ERR 1 badlen>\r\n");
				continue;
			}

			/* 主机端仲裁: 普通命令只在活跃总线发送; 发现探测(rqSA=0x00)不受限 */
			is_disc = (pkt.buf[3] == IPMB_DISC_HOST_ADDR_7BIT);
			if (!is_disc && g_host_active_bus != HOST_BUS_B)
				continue;   /* 非活跃总线: 普通命令丢弃不发 */

			/* 响应长度估算:与 I2C1_Task 完全一致(两路对同类从机,帧长相同) */
			{
				uint8_t cmd = pkt.buf[5];
				switch (cmd) {
				case 0x01: rx_target_len = 19; break;  /* Get Device ID:8+11 */
				case 0x21: {   /* Get Device SDR:长度视请求的offset/读取长度而定,按记录最多16字节估算 */
						uint8_t offset, rlen, avail, copy_len;
						if (pkt.len >= 12) {
							offset = pkt.buf[10];
							rlen   = pkt.buf[11];
							avail  = (offset < 16) ? (uint8_t)(16 - offset) : 0;
							copy_len = (rlen == 0xFF || rlen > avail) ? avail : rlen;
							rx_target_len = (uint8_t)(10 + copy_len);
						} else {
							rx_target_len = 10;   /* 请求过短,响应固定10字节(长度校验失败) */
						}
					} break;
				case 0x2D: rx_target_len = 11; break;  /* Get Sensor Reading:8+3 */
				case 0x0C: rx_target_len = 9;  break;  /* Set FRU Activation:8+1 */
				case 0x16: rx_target_len = 22; break;  /* Get Board Status:8+14 */
				case 0x15: rx_target_len = 9;  break;  /* Get Slot:8+1 */
				case 0x02: rx_target_len = 14; break;  /* Platform Event(PEM轮询):模拟主动上报帧,无完成码,6头+7data+1尾校验 */
				case 0x12: {   /* OEM Get Module Info:长度由请求 item 列表推算 */
					uint8_t ic = (pkt.len > 6) ? pkt.buf[6] : 0, q;
					uint16_t n = 6 + 1 + 1 + 1;                 /* 头6 + cc + item_count + cs */
					for (q = 0; q < ic && (uint16_t)(7 + q) < pkt.len; q++) {
						uint8_t id = pkt.buf[7 + q];
						if      (id == 0x11) n += 3 + 5 * 16;   /* 传感器信息 3+80 */
						else if (id == 0x12) n += 3 + 5 * 4;    /* 传感器值   3+20 */
						else                 n += 3;
					}
					rx_target_len = (n > BUFFER_RX_SIZE) ? BUFFER_RX_SIZE : (uint8_t)n;
				} break;
				default:   rx_target_len = 25; break;  /* 安全上界 */
				}
				if (rx_target_len > BUFFER_RX_SIZE) rx_target_len = BUFFER_RX_SIZE;
			}

			i2c2_recv_data_struct.recv_data_len = 0;
			I2C2_SendRequest(pkt.buf, pkt.len, rx_target_len, pkt.buf[0]);

			{
				ipmb_pkt_t rsp;
				uint8_t i;
				rsp.len = i2c2_recv_data_struct.recv_data_len;
				if (rsp.len > IPMB_TEST_BUF_SIZE) rsp.len = IPMB_TEST_BUF_SIZE;
				for (i = 0; i < rsp.len; i++) rsp.buf[i] = i2c2_recv_data_struct.recv_buf[i];
				if (rsp.len == 0) {
					if (!is_disc) {   /* 探测 miss 静默; 仅普通命令超时计失败 */
						// Usart_SendString(IPMB_TEST_COM, "<ERR 2 timeout B>\r\n");   /* 日志太吵,注释掉打印,失败计数/failover逻辑保留 */
						if (++s_host_b_fail >= IPMB_HOST_FAILOVER_CNT) {
							g_host_active_bus = HOST_BUS_A;
							s_host_b_fail = 0;
							printf("[IPMB-HOST-ARB] B timeout, switch to A\r\n");
						}
					}
				} else if (!is_disc) {
					s_host_b_fail = 0;   /* 普通命令成功, 清零失败计数 */
				}
				if (rsp.len > 0) {   /* 空响应(探测 miss / 重试耗尽的真超时)都不入队, 避免空 <RSP-B> */
					if (rsp.len >= 4 && rsp.buf[0] == IPMB_DISC_HOST_ADDR_7BIT) {
						/* 探测响应直接送发现 mailbox,绕开会被 USART 阻塞的打印任务,
						   否则 Temp_Task 占串口时发现响应转发不及时,会丢掉从机(如0x30) */
						if (xIPMB_DiscRspQueue2 != NULL) xQueueOverwrite(xIPMB_DiscRspQueue2, &rsp);
					} else if (rsp.len >= 4 && rsp.buf[0] == IPMB_WEB_HOST_ADDR_7BIT) {
						/* 网页监控轮询的响应,送专属 mailbox,避免和 IPMB_Test_Command_Task
						   抢 xIPMB_RspQueue2 */
						if (xIPMB_WebRspQueue2 != NULL) xQueueOverwrite(xIPMB_WebRspQueue2, &rsp);
					} else if (rsp.len >= 4 && rsp.buf[0] == IPMB_CTRL_HOST_ADDR_7BIT) {
						/* 网页控制台异步控制命令的响应,送专属 mailbox,同样避免抢 xIPMB_RspQueue2 */
						if (xIPMB_CtrlRspQueue2 != NULL) xQueueOverwrite(xIPMB_CtrlRspQueue2, &rsp);
					} else {
						xQueueSend(xIPMB_RspQueue2, &rsp, 0);   /* 深度5 FIFO: 0超时,满则丢新,不阻塞 I2C 任务 */
					}
				}
			}
		}
		vTaskDelay(1);
	}
}

void IPMB_Test_Command_Task(void* parameter)
{
	(void)parameter;
	ipmb_pkt_t rsp;
	ipmb_pem_pkt_t pem;
	char outbuf[IPMB_TEST_BUF_SIZE * 4 + 32];
	uint16_t p;
	uint16_t k;

	while (1)
	{
		/* 两路主机各有独立响应队列:I2C1→xIPMB_RspQueue(<RSP-A>),
		   I2C2→xIPMB_RspQueue2(<RSP-B>)。分别回显到 USART1。
		   地址发现探测的响应(rqSA 标记为 IPMB_DISC_HOST_ADDR_7BIT)不在此打印,
		   转发给 IPMB_Discovery_Task 专属 mailbox,避免和手动命令回显混在一起。 */
		if (xQueueReceive(xIPMB_RspQueue, &rsp, (TickType_t)50) == pdTRUE)
		{
			if (rsp.len >= 4 && rsp.buf[0] == IPMB_DISC_HOST_ADDR_7BIT) {
				if (xIPMB_DiscRspQueue != NULL) xQueueOverwrite(xIPMB_DiscRspQueue, &rsp);
			} else {
				p = 0;
				p += sprintf(outbuf + p, "<RSP-A");
				for (k = 0; k < rsp.len && p < sizeof(outbuf) - 10; k++)
					p += sprintf(outbuf + p, " %02X", rsp.buf[k]);
				p += sprintf(outbuf + p, ">\r\n");
				Usart_SendString(IPMB_TEST_COM, outbuf);

				/* 附加一行 ASCII 视图:可打印字符原样显示,其余显示为'.',
				 * 方便直接读出 sensorName 等文本字段,不用手动逐字节查表换算 */
				p = 0;
				p += sprintf(outbuf + p, "<RSP-A-ASCII ");
				for (k = 0; k < rsp.len && p < sizeof(outbuf) - 4; k++) {
					uint8_t c = rsp.buf[k];
					outbuf[p++] = (c >= 0x20 && c <= 0x7E) ? (char)c : '.';
				}
				outbuf[p++] = '>'; outbuf[p++] = '\r'; outbuf[p++] = '\n'; outbuf[p] = '\0';
				Usart_SendString(IPMB_TEST_COM, outbuf);
			}
		}

		if (xQueueReceive(xIPMB_RspQueue2, &rsp, (TickType_t)50) == pdTRUE)
		{
			if (rsp.len >= 4 && rsp.buf[0] == IPMB_DISC_HOST_ADDR_7BIT) {
				if (xIPMB_DiscRspQueue2 != NULL) xQueueOverwrite(xIPMB_DiscRspQueue2, &rsp);
			} else {
				p = 0;
				p += sprintf(outbuf + p, "<RSP-B");
				for (k = 0; k < rsp.len && p < sizeof(outbuf) - 10; k++)
					p += sprintf(outbuf + p, " %02X", rsp.buf[k]);
				p += sprintf(outbuf + p, ">\r\n");
				Usart_SendString(IPMB_TEST_COM, outbuf);

				p = 0;
				p += sprintf(outbuf + p, "<RSP-B-ASCII ");
				for (k = 0; k < rsp.len && p < sizeof(outbuf) - 4; k++) {
					uint8_t c = rsp.buf[k];
					outbuf[p++] = (c >= 0x20 && c <= 0x7E) ? (char)c : '.';
				}
				outbuf[p++] = '>'; outbuf[p++] = '\r'; outbuf[p++] = '\n'; outbuf[p] = '\0';
				Usart_SendString(IPMB_TEST_COM, outbuf);
			}
		}

		/* PEM(Platform Event Message)真正的主动上报:从机在没有被轮询的情况下
		   自己抢总线 write 过来的帧,由 I2C1_SendRequest/I2C2_SendRequest 开头的
		   forward_pem_and_clear 在下一次请求时认出并转发到这里。这就是之前
		   完全看不到的那条 LOG。(用 ipmb_pem_pkt_t 小结构,和 rsp 分开) */
		if (xIPMB_PemQueue != NULL && xQueueReceive(xIPMB_PemQueue, &pem, (TickType_t)10) == pdTRUE)
		{
			p = 0;
			p += sprintf(outbuf + p, "<PEM-PUSH-A");
			for (k = 0; k < pem.len && p < sizeof(outbuf) - 10; k++)
				p += sprintf(outbuf + p, " %02X", pem.buf[k]);
			p += sprintf(outbuf + p, ">\r\n");
			Usart_SendString(IPMB_TEST_COM, outbuf);

			if (pem.len >= 12) {
				/* 帧布局见 ipmb_slave.c IPMB_Slave_Handle_PlatformEvent:
				   buf[3]=源地址(从机自己) buf[9]=eventType buf[10]=eventData1
				   buf[11]=eventData2,分别编码 threshold_exceeded/sys_state/cause */
				uint8_t evt = pem.buf[9];
				uint8_t exceeded = (evt & 0x80) ? 0 : 1;   /* bit7=1 正常, 0 超限 */
				uint8_t sys_state = pem.buf[10] & 0x0F;
				uint8_t cause = (pem.buf[11] >> 4) & 0x0F;
				p = 0;
				p += sprintf(outbuf + p,
					"[PEM-PUSH-A] slave=0x%02X threshold_exceeded=%u sys_state=M%u cause=%u\r\n",
					pem.buf[3], exceeded, sys_state, cause);
				Usart_SendString(IPMB_TEST_COM, outbuf);

				Ipmb_SlotCache_StorePem(pem.buf[3], &pem);   /* 网页控制台改造:缓存供 bsp_httpd.c 读取 */
			}
		}

		if (xIPMB_PemQueue2 != NULL && xQueueReceive(xIPMB_PemQueue2, &pem, (TickType_t)10) == pdTRUE)
		{
			p = 0;
			p += sprintf(outbuf + p, "<PEM-PUSH-B");
			for (k = 0; k < pem.len && p < sizeof(outbuf) - 10; k++)
				p += sprintf(outbuf + p, " %02X", pem.buf[k]);
			p += sprintf(outbuf + p, ">\r\n");
			Usart_SendString(IPMB_TEST_COM, outbuf);

			if (pem.len >= 12) {
				uint8_t evt = pem.buf[9];
				uint8_t exceeded = (evt & 0x80) ? 0 : 1;
				uint8_t sys_state = pem.buf[10] & 0x0F;
				uint8_t cause = (pem.buf[11] >> 4) & 0x0F;
				p = 0;
				p += sprintf(outbuf + p,
					"[PEM-PUSH-B] slave=0x%02X threshold_exceeded=%u sys_state=M%u cause=%u\r\n",
					pem.buf[3], exceeded, sys_state, cause);
				Usart_SendString(IPMB_TEST_COM, outbuf);

				Ipmb_SlotCache_StorePem(pem.buf[3], &pem);   /* 网页控制台改造:缓存供 bsp_httpd.c 读取 */
			}
		}
	}
}

/* ============================================================
 * IPMB 从机地址动态发现: 从机地址不再固定为 0x8E, 改为按背板槽位
 * 0x30 + (Slot_ID<<1)(Slot_ID 0-31), 后台周期扫描候选地址,发现后
 * 写入 g_slave_ipmb_addr 供 I2C1/I2C2_SendRequest 及自动构帧命令使用。
 * 复用现有 Get Device ID(cmd=0x01)请求-响应通路做探测,不新增底层
 * 探测原语;探测请求用 IPMB_DISC_HOST_ADDR_7BIT 作为 rqSA 标记,
 * IPMB_Test_Command_Task 据此把探测响应分流到本任务专属 mailbox,
 * 不与用户手动命令的回显互相干扰。
 * ============================================================ */
#define IPMB_DISC_SLOT_MIN               0
#define IPMB_DISC_SLOT_MAX               31
#define IPMB_DISC_PER_ADDR_TIMEOUT_MS     150   /* 覆盖 I2C1/I2C2_SendRequest 内部约100ms超时 */
#define IPMB_DISC_RESCAN_INTERVAL_MS      15000 /* 后台周期重扫,应对换槽位/从机重启 */
#define IPMB_DISC_BUS_BACKOFF_MISS        3     /* 连续几轮整轮无应答后判定该总线"已知失联" */
#define IPMB_DISC_BUS_BACKOFF_SKIP        4     /* 失联退避期间,每隔几轮才抽查探测1次(而非每轮全扫) */

/* 把本轮发现到的从机地址去重追加进临时表(满 IPMB_MAX_SLAVES 则丢弃多余) */
static void disc_add_addr(uint8_t *tbl, uint8_t *cnt, uint8_t addr)
{
	uint8_t i;
	for (i = 0; i < *cnt; i++) {
		if (tbl[i] == addr) return;   /* 已存在, 去重 */
	}
	if (*cnt < IPMB_MAX_SLAVES) {
		tbl[(*cnt)++] = addr;
	}
}

void IPMB_Discovery_Task(void* parameter)
{
	uint8_t disc_a_miss = 0, disc_b_miss = 0;   /* 连续几轮整轮无应答, 用于对已知失联总线退避 */
	uint32_t disc_cycle = 0;

	(void)parameter;
	vTaskDelay(pdMS_TO_TICKS(2000));   /* 等待其它外设/队列初始化完成 */

	for (;;)
	{
		uint8_t slot;
		uint8_t a_seen = 0, b_seen = 0;   /* 本轮是否收到过该总线的有效应答 */
		uint8_t tmp_addrs[IPMB_MAX_SLAVES]; /* 本轮临时表, 扫完整轮后一次性提交 */
		uint8_t tmp_count = 0;
		/* 已判定失联的总线, 退避期间只每 N 轮抽查 1 次探测, 避免每 15 秒都对
		 * 死总线跑满 32 个候选地址、白白占用 I2C1/2_Task 和命令队列 */
		uint8_t probe_a = (disc_a_miss < IPMB_DISC_BUS_BACKOFF_MISS) ||
		                   (disc_cycle % IPMB_DISC_BUS_BACKOFF_SKIP == 0);
		uint8_t probe_b = (disc_b_miss < IPMB_DISC_BUS_BACKOFF_MISS) ||
		                   (disc_cycle % IPMB_DISC_BUS_BACKOFF_SKIP == 0);

		/* 整轮扫描 0x30..0x6E, 记录所有应答的从机(多从机场景不再扫到第一个就停) */
		for (slot = IPMB_DISC_SLOT_MIN; slot <= IPMB_DISC_SLOT_MAX; slot++)
		{
			uint8_t candidate = (uint8_t)(0x30 + (slot << 1));
			ipmb_pkt_t pkt, rsp;

			pkt.len = 0;
			pkt.buf[pkt.len++] = candidate;                 /* 目标: 候选业务板地址 */
			pkt.buf[pkt.len++] = 0x18;                       /* netFn=App(6)<<2 */
			pkt.buf[pkt.len++] = ipmi_calc_chk(pkt.buf, 0, 2);
			pkt.buf[pkt.len++] = IPMB_DISC_HOST_ADDR_7BIT;   /* rqSA 探测标记 */
			pkt.buf[pkt.len++] = 0x00;                       /* rqSeq=0, lun=0 */
			pkt.buf[pkt.len++] = 0x01;                       /* cmd Get Device ID */
			pkt.buf[pkt.len] = ipmi_calc_chk(pkt.buf, 3, pkt.len);
			pkt.len++;

			if (probe_a && xIPMB_CmdQueue != NULL)  xQueueSend(xIPMB_CmdQueue, &pkt, 0);
			if (probe_b && xIPMB_CmdQueue2 != NULL) xQueueSend(xIPMB_CmdQueue2, &pkt, 0);

			/* 用响应帧里回显的 rsSA(resp.buf[3], 即真正应答的从机自己的地址)作为
			 * 发现结果, 而不是直接用本轮探测的 candidate —— 如果上一个候选的响应
			 * 因排队/重试延迟晚到、跨过了它自己的超时窗口, 才会在检查"下一个候选"
			 * 时才被取出, 此时 candidate 已经是下一个值, 用回显地址可避免记录错误 */
			if (probe_a && xIPMB_DiscRspQueue != NULL &&
			    xQueueReceive(xIPMB_DiscRspQueue, &rsp, pdMS_TO_TICKS(IPMB_DISC_PER_ADDR_TIMEOUT_MS)) == pdTRUE &&
			    ipmi_verify_chk(rsp.buf, rsp.len))
			{
				disc_add_addr(tmp_addrs, &tmp_count, rsp.buf[3]);
				g_host_active_bus = HOST_BUS_A;   /* A 可达: A 优先/恢复自动切回 */
				a_seen = 1;
				printf("[IPMB-DISC] found slave at 0x%02X via A\r\n", rsp.buf[3]);
			}
			else if (probe_b && xIPMB_DiscRspQueue2 != NULL &&
			         xQueueReceive(xIPMB_DiscRspQueue2, &rsp,
			                       pdMS_TO_TICKS(IPMB_DISC_PER_ADDR_TIMEOUT_MS)) == pdTRUE &&
			         ipmi_verify_chk(rsp.buf, rsp.len))
			{
				disc_add_addr(tmp_addrs, &tmp_count, rsp.buf[3]);
				if (!a_seen) g_host_active_bus = HOST_BUS_B;   /* 本轮 A 无应答才切 B */
				b_seen = 1;
				printf("[IPMB-DISC] found slave at 0x%02X via B\r\n", rsp.buf[3]);
			}
		}

		/* 提交本轮发现结果: 先填地址, 最后写 g_slave_count, 保证 PEM 轮询任务
		 * 不会读到半张表。只在本轮真正探测过(至少一路)时才更新, 避免纯退避跳过
		 * 的空轮把上次的发现表误清空。g_slave_ipmb_addr 取表首, 供 ipmboem 沿用。 */
		if (probe_a || probe_b) {
			uint8_t i;
			for (i = 0; i < tmp_count; i++) g_slave_addrs[i] = tmp_addrs[i];
			g_slave_count = tmp_count;
			if (tmp_count > 0) g_slave_ipmb_addr = tmp_addrs[0];
		}

		/* 本轮探测过却完全没收到有效应答才计一次失联; 一旦收到应答立即清零退避计数 */
		if (probe_a) disc_a_miss = a_seen ? 0 : (uint8_t)(disc_a_miss + (disc_a_miss < 255));
		if (probe_b) disc_b_miss = b_seen ? 0 : (uint8_t)(disc_b_miss + (disc_b_miss < 255));
		disc_cycle++;

		vTaskDelay(pdMS_TO_TICKS(IPMB_DISC_RESCAN_INTERVAL_MS));
	}
}
void Temp_Task(void* parameter)
{
    float temp1, temp2, temp3;
    uint16_t duty1, duty2, duty3, duty4;
    uint16_t rpm1, rpm2, rpm3, rpm4;
    char output_buf[512] = {0};

    while (1)
    {
        /* 独占 stPrintf_Buf,直到本轮拼接+发送+清空全部完成才释放,避免和
         * USART5_Task "?" 查询互相清空/混内容(不能连 vTaskDelay 也锁住) */
        xSemaphoreTake(xPrintfBufMutex, portMAX_DELAY);

        ADC_Filter();
        ADC_Calculation(&stADC_Data);
        print_rack_addr();
        print_slot_addr();

        Board_ADDR90_temp();
        Board_ADDR92_temp();
        Board_ADDR94_temp();

        /* BPD20550 电源模块:读 MFR_ID / Vin / Iout / Temp,写入 s_bpd_* */
        {
            char bpd_buf[256] = {0};
            Board_BPD20550_current(bpd_buf);
            (void)sscanf(bpd_buf, "BPD20550 MFR_ID:%*x,%*x\r\nVin: %f V\r\nIout: %f A\r\nTemp: %f C",
                         &s_bpd_vin, &s_bpd_iout, &s_bpd_temp);
            /* sscanf 失败时 s_bpd_* 保持原值(默认 0.0f),便于上层识别"读失败" */
        }

        temp1 = board_temp0[0] + (board_temp0[1] / 100.0f);
        temp2 = board_temp1[0] + (board_temp1[1] / 100.0f);
        temp3 = board_temp2[0] + (board_temp2[1] / 100.0f);

        /* 风扇占空比改由串口 fan <0-100> 命令设置(g_fan_duty),不再按温度自动调速。
         * 4 路共用同一个占空比,这里每周期重新下发一次,保证 4 路一致、异常后恢复到设定值。
         * 温度仍照常采集并打印,只是不再参与风扇转速控制。 */
        duty1 = duty2 = duty3 = duty4 = g_fan_duty;

        set_pwm_duty(OCPWM1_CHANNEL, duty1);
        set_pwm_duty(OCPWM2_CHANNEL, duty2);
        set_pwm_duty(OCPWM3_CHANNEL, 100-duty3);
        set_pwm_duty(OCPWM4_CHANNEL, duty4);

        rpm1 = calculate_fan_rpm(&TIM_ICUserValueStructure_TACH1);
        rpm2 = calculate_fan_rpm(&TIM_ICUserValueStructure_TACH2);
        rpm3 = calculate_fan_rpm(&TIM_ICUserValueStructure_TACH3);
        rpm4 = calculate_fan_rpm(&TIM_ICUserValueStructure_TACH4);

        sprintf(output_buf,
                "\r\n"
                "[PWM OUT] (serial-set duty via 'fan 0-100', all 4 = %3d%%)\r\n"
                "FAN_1: %3d%%  PE6(TIM9_CH1)\r\n"
                "FAN_2: %3d%%  PE5(TIM9_CH2)\r\n"
                "FAN_3: %3d%%  PC8(TIM8_CH3)\r\n"
                "FAN_4: %3d%%  PI6(TIM8_CH2)\r\n"
                "[TEMP]\r\n"
                "TEMP_1: %.1f C  (0x90)\r\n"
                "TEMP_2: %.1f C  (0x92)\r\n"
                "[TACH IN]\r\n"
                "RPM_1: %5d  PB9(TIM4_CH4)\r\n"
                "RPM_2: %5d  PI2(TIM8_CH4)\r\n"
                "RPM_3: %5d  PH11(TIM5_CH2)\r\n"
                "RPM_4: %5d  PH10(TIM5_CH1)\r\n"
                "[BPD20550]\r\n"
                "Vin:  %.2f V\r\n"
                "Iout: %.2f A\r\n"
                "Temp: %.1f C\r\n",
                g_fan_duty,
                duty1, duty2, duty3, duty4,
                temp1, temp2,
                rpm1, rpm2, rpm3, rpm4,
                s_bpd_vin, s_bpd_iout, s_bpd_temp);
        strcat((char *)stPrintf_Buf.buf, output_buf);

        stPrintf_Buf.size = strlen((char *)stPrintf_Buf.buf);
        Usart_SendString(COM_USART1, (char *)stPrintf_Buf.buf);
        memset(stPrintf_Buf.buf, 0, stPrintf_Buf.size);
        stPrintf_Buf.size = 0;

        xSemaphoreGive(xPrintfBufMutex);

vTaskDelay(4000);//延时时间最好长一点，不然会影响IPMB的多路从机通信
    }
}

#if (IPMB_A_ROLE == IPMB_A_MASTER)
void I2C1_StartBus(void)
{
	uint8_t i;
	for(i=0;i<8;i++)
		i2c1_send_data_struct.send_buf[i]=i;
	i2c1_send_data_struct.send_data_len=i;

	I2C_ITConfig(COM_I2C1,I2C_IT_BUF | I2C_IT_EVT | I2C_IT_ERR, ENABLE);
	if(I2C_GetFlagStatus(COM_I2C1, I2C_FLAG_BUSY) == 0)
		I2C_GenerateSTART(COM_I2C1, ENABLE);
}
#endif