#include "ipmb_threshold.h"
#include ".\i2c\bsp_eeprom.h"
#include <string.h>

/* EEPROM 布局:固定分配1页(64字节),256~1023 是板卡身份字段用的,这次从1024起,
 * 跟其它区域都不冲突(EEPROM 共32KB,见 ipmb_board_identity.c 同类布局注释)。
 * raw[0]=校验和(对raw[1..IPMB_THRESHOLD_CONFIG_LEN]做简单字节累加),raw[1..]=配置
 * 数据。校验和而不是单纯"看首字节是不是0xFF"是因为断电阈值一旦损坏后果比板卡身份
 * 字符串更严重,值得多一层保护,弥补这个项目里EEPROM记录一直没有CRC的既有薄弱点。 */
#define THRESHOLD_EE_ADDR       1024u
#define THRESHOLD_EE_SLOT_SIZE  64u

IPMB_ThresholdConfig_t g_threshold_config;

static uint8_t threshold_checksum(const uint8_t *data, uint8_t len)
{
	uint8_t sum = 0;
	uint8_t i;
	for (i = 0; i < len; i++)
	{
		sum = (uint8_t)(sum + data[i]);
	}
	return sum;
}

static void threshold_apply_default(void)
{
	g_threshold_config.temp_alarm_high      = 70;
	g_threshold_config.temp_alarm_low       = 0;
	g_threshold_config.temp_shutdown_high   = 80;
	g_threshold_config.temp_shutdown_low    = -10;
	g_threshold_config.volt_alarm_percent   = 10;
	g_threshold_config.volt_shutdown_percent = 20;
}

void ThresholdConfig_Init(void)
{
	uint8_t raw[THRESHOLD_EE_SLOT_SIZE];
	uint8_t ok = EEPROM_ReadBytes(THRESHOLD_EE_ADDR, raw, THRESHOLD_EE_SLOT_SIZE);

	if (ok && raw[0] != 0xFF && raw[0] == threshold_checksum(&raw[1], IPMB_THRESHOLD_CONFIG_LEN))
	{
		g_threshold_config.temp_alarm_high        = (int8_t)raw[1];
		g_threshold_config.temp_alarm_low         = (int8_t)raw[2];
		g_threshold_config.temp_shutdown_high     = (int8_t)raw[3];
		g_threshold_config.temp_shutdown_low      = (int8_t)raw[4];
		g_threshold_config.volt_alarm_percent     = raw[5];
		g_threshold_config.volt_shutdown_percent  = raw[6];
	}
	else
	{
		threshold_apply_default();
	}
}

void ThresholdConfig_Get(uint8_t *out_buf)
{
	out_buf[0] = (uint8_t)g_threshold_config.temp_alarm_high;
	out_buf[1] = (uint8_t)g_threshold_config.temp_alarm_low;
	out_buf[2] = (uint8_t)g_threshold_config.temp_shutdown_high;
	out_buf[3] = (uint8_t)g_threshold_config.temp_shutdown_low;
	out_buf[4] = g_threshold_config.volt_alarm_percent;
	out_buf[5] = g_threshold_config.volt_shutdown_percent;
}

uint8_t ThresholdConfig_Set(const uint8_t *value, uint8_t value_len)
{
	uint8_t raw[THRESHOLD_EE_SLOT_SIZE];
	int8_t  t_ah, t_al, t_sh, t_sl;
	uint8_t v_ap, v_sp;

	if (value_len != IPMB_THRESHOLD_CONFIG_LEN) return 1;

	t_ah = (int8_t)value[0];
	t_al = (int8_t)value[1];
	t_sh = (int8_t)value[2];
	t_sl = (int8_t)value[3];
	v_ap = value[4];
	v_sp = value[5];

	if (t_ah <= t_al || t_sh <= t_sl) return 1;
	if (v_ap > 100 || v_sp > 100) return 1;

	g_threshold_config.temp_alarm_high        = t_ah;
	g_threshold_config.temp_alarm_low         = t_al;
	g_threshold_config.temp_shutdown_high     = t_sh;
	g_threshold_config.temp_shutdown_low      = t_sl;
	g_threshold_config.volt_alarm_percent     = v_ap;
	g_threshold_config.volt_shutdown_percent  = v_sp;

	memset(raw, 0xFF, sizeof(raw));
	memcpy(&raw[1], value, IPMB_THRESHOLD_CONFIG_LEN);
	raw[0] = threshold_checksum(&raw[1], IPMB_THRESHOLD_CONFIG_LEN);
	if (!EEPROM_WriteBytes(THRESHOLD_EE_ADDR, raw, THRESHOLD_EE_SLOT_SIZE)) return 2;

	return 0;
}
