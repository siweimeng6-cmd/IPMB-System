#include "task_slot_cache.h"
#include "task.h"   /* xTaskGetTickCount */
#include <string.h>

IpmbSlotCache_t g_slot_cache[IPMB_MAX_SLAVES] = {0};

/* 7个目前接了真实硬件的传感器编号(见 指令.txt⑤节),新分配槽位时预填进
 * sensors[].sensor_num,Ipmb_SlotCache_StoreSensor 按编号线性查表匹配 */
static const uint8_t s_known_sensor_nums[IPMB_SLOT_SENSOR_COUNT] = {
	0x04, 0x20, 0x03, 0x17, 0x19, 0x41, 0x07
};
/* 2026-07-31:中文名字改用 \x十六进制转义写原始UTF-8字节,不直接在源码里放中文字符——
 * 实测 Keil 是按 GBK 码页读这个 UTF-8 源文件的,直接写中文字符会被拆成乱码,严重时
 * (比如"风扇占空比"5字15字节是奇数,GBK两两配对到最后剩一个字节吞掉收尾引号)
 * 直接编译报错(见 task_i2c.c 同类问题的历史记录)。\x逐字节转义不管编译器按哪种
 * 码页读源文件都是原样按字节值抄进目标文件,100%绕开这个问题,网页那边收到的还是
 * 正确的UTF-8,显示不受影响。依次是"温度1"/"温度2"/"风扇占空比"/"输出电流"。 */
static const char * const s_known_sensor_names[IPMB_SLOT_SENSOR_COUNT] = {
	"\xe6\xb8\xa9\xe5\xba\xa6\x31", "\xe6\xb8\xa9\xe5\xba\xa6\x32", "V12", "V5", "V0.75",
	"\xe9\xa3\x8e\xe6\x89\x87\xe5\x8d\xa0\xe7\xa9\xba\xe6\xaf\x94",
	"\xe8\xbe\x93\xe5\x87\xba\xe7\x94\xb5\xe6\xb5\x81"
};
static const char * const s_known_sensor_units[IPMB_SLOT_SENSOR_COUNT] = {
	"C", "C", "V", "V", "V", "%", "A"
};

IpmbSlotCache_t* Ipmb_SlotCache_FindOrAlloc(uint8_t addr)
{
	uint8_t i, free_idx = IPMB_MAX_SLAVES;

	for (i = 0; i < IPMB_MAX_SLAVES; i++) {
		if (g_slot_cache[i].in_use && g_slot_cache[i].addr == addr)
			return &g_slot_cache[i];
		if (!g_slot_cache[i].in_use && free_idx == IPMB_MAX_SLAVES)
			free_idx = i;
	}
	if (free_idx == IPMB_MAX_SLAVES)
		return NULL;   /* 8个槽位全占用,和 IPMB_MAX_SLAVES 上限一致 */

	memset(&g_slot_cache[free_idx], 0, sizeof(IpmbSlotCache_t));
	g_slot_cache[free_idx].in_use = 1;
	g_slot_cache[free_idx].addr = addr;
	g_slot_cache[free_idx].slot_id = Ipmb_AddrToSlot(addr);
	for (i = 0; i < IPMB_SLOT_SENSOR_COUNT; i++)
		g_slot_cache[free_idx].sensors[i].sensor_num = s_known_sensor_nums[i];
	return &g_slot_cache[free_idx];
}

IpmbSlotCache_t* Ipmb_SlotCache_FindBySlotId(uint8_t slot_id)
{
	uint8_t i;
	for (i = 0; i < IPMB_MAX_SLAVES; i++) {
		if (g_slot_cache[i].in_use && g_slot_cache[i].slot_id == slot_id)
			return &g_slot_cache[i];
	}
	return NULL;
}

/* 响应帧字节偏移量见 指令.txt①节(19字节:buf[6]=完成码,buf[7..17]=数据,buf[18]=尾校验) */
void Ipmb_SlotCache_StoreDeviceID(IpmbSlotCache_t *slot, const ipmb_pkt_t *rsp)
{
	if (slot == NULL || rsp == NULL) return;
	if (rsp->len < 18 || rsp->buf[6] != 0x00) { slot->devid_valid = 0; return; }

	slot->device_id       = rsp->buf[7];
	slot->hw_revision     = rsp->buf[8];
	slot->fw_major_rev    = rsp->buf[9];
	slot->ipmc_sw_version = rsp->buf[10];
	slot->ipmi_version    = rsp->buf[11];
	slot->device_support  = rsp->buf[12];
	slot->mfg_id[0] = rsp->buf[13]; slot->mfg_id[1] = rsp->buf[14]; slot->mfg_id[2] = rsp->buf[15];
	slot->product_id[0] = rsp->buf[16]; slot->product_id[1] = rsp->buf[17];
	slot->devid_valid = 1;
	slot->online = 1;
	slot->last_seen_tick = xTaskGetTickCount();
}

/* 响应帧字节偏移量见 指令.txt②节(22字节:buf[6]=完成码,buf[7..20]=数据,buf[21]=尾校验) */
void Ipmb_SlotCache_StoreBoardStatus(IpmbSlotCache_t *slot, const ipmb_pkt_t *rsp)
{
	if (slot == NULL || rsp == NULL) return;
	if (rsp->len < 21 || rsp->buf[6] != 0x00) { slot->boardstat_valid = 0; return; }

	slot->cpu_present  = rsp->buf[7];
	slot->self_test    = rsp->buf[8];
	slot->cpu_util     = (uint16_t)(rsp->buf[9]  | (rsp->buf[10] << 8));
	slot->mem_util     = (uint16_t)(rsp->buf[11] | (rsp->buf[12] << 8));
	slot->bw1          = (uint16_t)(rsp->buf[13] | (rsp->buf[14] << 8));
	slot->bw2          = (uint16_t)(rsp->buf[15] | (rsp->buf[16] << 8));
	slot->cpu_temp_raw = rsp->buf[17];
	slot->os_version   = rsp->buf[18];
	slot->board_fw_version = rsp->buf[19];
	slot->power_state  = rsp->buf[20];
	slot->boardstat_valid = 1;
	slot->online = 1;
	slot->last_seen_tick = xTaskGetTickCount();
}

/* 响应帧字节偏移量见 指令.txt③节(9字节:buf[6]=完成码,buf[7]=槽位号,buf[8]=尾校验) */
void Ipmb_SlotCache_StoreSlotReadback(IpmbSlotCache_t *slot, const ipmb_pkt_t *rsp)
{
	if (slot == NULL || rsp == NULL) return;
	if (rsp->len < 8 || rsp->buf[6] != 0x00) { slot->slot_valid = 0; return; }

	slot->slot_readback = rsp->buf[7];
	slot->slot_valid = 1;
	slot->online = 1;
	slot->last_seen_tick = xTaskGetTickCount();
}

/* 【2026-07-28更正】响应帧仍是11字节(buf[6]=完成码),但数值字段已从单字节改成
 * buf[7]=LSB/buf[8]=MSB 小端拼出的有符号int16(×100定点),buf[9]=事件上报标志位
 * (原来的位置/含义变了,不用管)。已用真实抓包+从机源码核实,见 task_slot_cache.h
 * 里 IpmbSensorCache_t.value 的注释。最小长度判定从 len>=8 改成 len>=9(要读到buf[8])。
 * raw==0x7FFF 视为"数据不可用"哨兵,同样标记 valid=0。 */
void Ipmb_SlotCache_StoreSensor(IpmbSlotCache_t *slot, uint8_t sensor_num, const ipmb_pkt_t *rsp)
{
	uint8_t i;
	if (slot == NULL) return;

	for (i = 0; i < IPMB_SLOT_SENSOR_COUNT; i++) {
		if (slot->sensors[i].sensor_num != sensor_num) continue;
		if (rsp != NULL && rsp->len >= 9 && rsp->buf[6] == 0x00) {
			int16_t raw = (int16_t)((uint16_t)rsp->buf[7] | ((uint16_t)rsp->buf[8] << 8));
			if (raw == (int16_t)0x7FFF) {
				slot->sensors[i].valid = 0;
			} else {
				slot->sensors[i].value = raw;
				slot->sensors[i].valid = 1;
				slot->online = 1;
				slot->last_seen_tick = xTaskGetTickCount();
			}
		} else {
			slot->sensors[i].valid = 0;
		}
		return;
	}
}

/* 响应帧布局(Get Board Identity Field 0x17,见 J2I-Slave1 ipmb_slave.c
 * IPMB_Slave_Handle_GetBoardIdentity):buf[6]=完成码,buf[7]=field_id(回显),
 * buf[8]=value_len,buf[9..9+value_len-1]=文本内容(不含结尾'\0')。
 * 只处理 field_id 7~11(自定义文本字段),0~6 走标准 Get Device ID,归
 * Ipmb_SlotCache_StoreDeviceID 管,不受这个函数影响。 */
void Ipmb_SlotCache_StoreCustomField(IpmbSlotCache_t *slot, uint8_t field_id, const ipmb_pkt_t *rsp)
{
	char *dst;
	uint8_t value_len;

	if (slot == NULL) return;

	switch (field_id) {
	case 7:  dst = slot->cpu_model;     break;
	case 8:  dst = slot->mem_size;      break;
	case 9:  dst = slot->board_model;   break;
	case 10: dst = slot->serial_number; break;
	case 11: dst = slot->os_version_text; break;
	default: return;
	}

	if (rsp == NULL || rsp->len < 9 || rsp->buf[6] != 0x00 || rsp->buf[7] != field_id) return;

	value_len = rsp->buf[8];
	if (value_len > 63 || rsp->len < 9 + value_len) return;

	memcpy(dst, &rsp->buf[9], value_len);
	dst[value_len] = '\0';
	slot->online = 1;
	slot->last_seen_tick = xTaskGetTickCount();
}

/* PEM 帧布局见 task_i2c.c IPMB_Test_Command_Task 已验证过的解析(pem->buf[3]=源地址,
 * buf[9]=eventType,buf[10]=eventData1,buf[11]=eventData2),照抄同一套位运算 */
void Ipmb_SlotCache_StorePem(uint8_t addr, const ipmb_pem_pkt_t *pem)
{
	IpmbSlotCache_t *slot;
	uint8_t evt;

	if (pem == NULL || pem->len < 12) return;
	slot = Ipmb_SlotCache_FindOrAlloc(addr);
	if (slot == NULL) return;

	evt = pem->buf[9];
	slot->pem_threshold_exceeded = (evt & 0x80) ? 0 : 1;   /* bit7=1 正常,0 超限 */
	slot->pem_sys_state = pem->buf[10] & 0x0F;
	slot->pem_cause = (pem->buf[11] >> 4) & 0x0F;
	slot->pem_valid = 1;
	slot->pem_last_tick = xTaskGetTickCount();
}

const char* Ipmb_SensorName(uint8_t sensor_num)
{
	uint8_t i;
	for (i = 0; i < IPMB_SLOT_SENSOR_COUNT; i++)
		if (s_known_sensor_nums[i] == sensor_num) return s_known_sensor_names[i];
	return "";
}

const char* Ipmb_SensorUnit(uint8_t sensor_num)
{
	uint8_t i;
	for (i = 0; i < IPMB_SLOT_SENSOR_COUNT; i++)
		if (s_known_sensor_nums[i] == sensor_num) return s_known_sensor_units[i];
	return "";
}
