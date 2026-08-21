#include "ipmb_board_identity.h"
#include ".\i2c\bsp_eeprom.h"
#include <string.h>
#include <stddef.h>   /* offsetof */

/* EEPROM 布局:每字段固定分配 64 字节槽位(2026-08-20 从 32 扩容,支持文本字段存中文),
 * 不管实际用了几字节,简化地址计算。256 起,12 个字段共占 256~1023,跟已用的 0~15(自检)、
 * 128~131(累计运行时长)不冲突,留了足够裕量(EEPROM 共 32KB)。64 字节正好是 24C256 的
 * 页写大小,每个字段槽位写入时不跨页,EEPROM_WriteBytes 单次页写即可完成。 */
#define BOARD_IDENTITY_EE_BASE       256u
#define BOARD_IDENTITY_EE_SLOT_SIZE  64u

IPMB_BoardIdentity_t g_board_identity;

/* 字段描述表:struct_offset 供泛化读写定位到 g_board_identity 里具体是哪个成员,
 * byte_len 对数值/原始字节字段是精确长度,对文本字段是最大长度(不含结尾'\0'),
 * is_text 决定校验/EEPROM存取走哪条路径。 */
typedef struct {
	uint16_t struct_offset;
	uint8_t  byte_len;
	uint8_t  is_text;
} BoardIdentityFieldDesc_t;

static const BoardIdentityFieldDesc_t s_field_desc[IPMB_BOARD_IDENTITY_FIELD_COUNT] = {
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, device_id),      1, 0 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, hw_revision),    1, 0 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, fw_major_rev),   1, 0 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, ipmi_version),   1, 0 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, device_support), 1, 0 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, mfg_id),         3, 0 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, product_id),     2, 0 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, cpu_model),      IPMB_BOARD_IDENTITY_TEXT_MAX, 1 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, mem_size),       IPMB_BOARD_IDENTITY_TEXT_MAX, 1 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, board_model),    IPMB_BOARD_IDENTITY_TEXT_MAX, 1 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, serial_number),  IPMB_BOARD_IDENTITY_TEXT_MAX, 1 },
	{ (uint16_t)offsetof(IPMB_BoardIdentity_t, os_version_text),IPMB_BOARD_IDENTITY_TEXT_MAX, 1 },
};

/* 今天硬编码的默认值(跟改动前 ipmb_slave.c 里 g_slave_device_id 的初值逐一对应),
 * field_id 0~6 在 EEPROM 未配置(擦除态)时回退到这里,保证行为跟改动前一致 */
static void board_identity_apply_default(uint8_t field_id, uint8_t *ram_ptr)
{
	switch (field_id) {
	case 0: *ram_ptr = 0x01; break;                          /* device_id */
	case 1: *ram_ptr = 0x81; break;                          /* hw_revision */
	case 2: *ram_ptr = 0x81; break;                          /* fw_major_rev */
	case 3: *ram_ptr = 0x51; break;                          /* ipmi_version */
	case 4: *ram_ptr = 0x23; break;                          /* device_support */
	case 5: ram_ptr[0] = 0x00; ram_ptr[1] = 0x00; ram_ptr[2] = 0x00; break;  /* mfg_id */
	case 6: ram_ptr[0] = 0x01; ram_ptr[1] = 0x00; break;      /* product_id,低字节在前=1 */
	default: break;
	}
}

void BoardIdentity_Init(void)
{
	uint8_t i;

	for (i = 0; i < IPMB_BOARD_IDENTITY_FIELD_COUNT; i++)
	{
		uint8_t *ram_ptr = (uint8_t *)&g_board_identity + s_field_desc[i].struct_offset;
		uint16_t ee_addr = (uint16_t)(BOARD_IDENTITY_EE_BASE + (uint16_t)i * BOARD_IDENTITY_EE_SLOT_SIZE);
		uint8_t raw[BOARD_IDENTITY_EE_SLOT_SIZE];
		uint8_t ok = EEPROM_ReadBytes(ee_addr, raw, BOARD_IDENTITY_EE_SLOT_SIZE);

		if (s_field_desc[i].is_text)
		{
			/* 文本槽位格式:raw[0]=实际字节数,raw[1..]=文本字节。raw[0]==0xFF(不可能是
			 * 合法长度,上限只有63)视为擦除态/从未配置过,回退空字符串。 */
			if (ok && raw[0] != 0xFF && raw[0] <= s_field_desc[i].byte_len)
			{
				memcpy(ram_ptr, &raw[1], raw[0]);
				ram_ptr[raw[0]] = '\0';
			}
			else
			{
				ram_ptr[0] = '\0';
			}
		}
		else
		{
			/* 数值/原始字节槽位:第一个字节 0xFF 视为擦除态(跟 Runtime_Init 同样的哨兵
			 * 约定),回退硬编码默认值 */
			if (ok && raw[0] != 0xFF)
			{
				memcpy(ram_ptr, raw, s_field_desc[i].byte_len);
			}
			else
			{
				board_identity_apply_default(i, ram_ptr);
			}
		}
	}
}

uint8_t BoardIdentity_GetField(uint8_t field_id, uint8_t *out_buf, uint8_t *out_len)
{
	uint8_t *ram_ptr;

	if (field_id >= IPMB_BOARD_IDENTITY_FIELD_COUNT) return 0;
	ram_ptr = (uint8_t *)&g_board_identity + s_field_desc[field_id].struct_offset;

	if (s_field_desc[field_id].is_text)
	{
		uint8_t len = (uint8_t)strlen((const char *)ram_ptr);
		memcpy(out_buf, ram_ptr, len);
		*out_len = len;
	}
	else
	{
		memcpy(out_buf, ram_ptr, s_field_desc[field_id].byte_len);
		*out_len = s_field_desc[field_id].byte_len;
	}
	return 1;
}

/* 轻量 UTF-8 结构校验:放行 ASCII 可打印字符 + 合法的 2/3/4 字节 UTF-8 序列(覆盖
 * 中文等常用字符),拒绝控制字符/DEL/续字节数不对/序列被截断的情况——延续原来
 * "防止不可打印字节写进EEPROM污染网页JSON输出"的思路,只是把允许范围从纯ASCII
 * 扩到合法UTF-8,不是简单地放行所有字节 */
static uint8_t board_identity_is_valid_utf8(const uint8_t *value, uint8_t value_len)
{
	uint8_t i = 0;
	while (i < value_len)
	{
		uint8_t c = value[i];
		uint8_t extra;
		if (c < 0x20 || c == 0x7F) return 0;
		if (c < 0x80) { i++; continue; }
		if ((c & 0xE0) == 0xC0) extra = 1;
		else if ((c & 0xF0) == 0xE0) extra = 2;
		else if ((c & 0xF8) == 0xF0) extra = 3;
		else return 0;
		if ((uint8_t)(i + extra) >= value_len) return 0;
		i++;
		while (extra-- > 0)
		{
			if ((value[i] & 0xC0) != 0x80) return 0;
			i++;
		}
	}
	return 1;
}

uint8_t BoardIdentity_SetField(uint8_t field_id, const uint8_t *value, uint8_t value_len)
{
	uint8_t *ram_ptr;
	uint16_t ee_addr;
	uint8_t raw[BOARD_IDENTITY_EE_SLOT_SIZE];

	if (field_id >= IPMB_BOARD_IDENTITY_FIELD_COUNT) return 1;
	ram_ptr = (uint8_t *)&g_board_identity + s_field_desc[field_id].struct_offset;
	ee_addr = (uint16_t)(BOARD_IDENTITY_EE_BASE + (uint16_t)field_id * BOARD_IDENTITY_EE_SLOT_SIZE);

	if (s_field_desc[field_id].is_text)
	{
		if (value_len > s_field_desc[field_id].byte_len) return 1;
		if (!board_identity_is_valid_utf8(value, value_len)) return 1;

		memcpy(ram_ptr, value, value_len);
		ram_ptr[value_len] = '\0';

		memset(raw, 0, sizeof(raw));
		raw[0] = value_len;
		memcpy(&raw[1], value, value_len);
		if (!EEPROM_WriteBytes(ee_addr, raw, BOARD_IDENTITY_EE_SLOT_SIZE)) return 2;
	}
	else
	{
		if (value_len != s_field_desc[field_id].byte_len) return 1;

		memcpy(ram_ptr, value, value_len);

		memset(raw, 0xFF, sizeof(raw));
		memcpy(raw, value, value_len);
		if (!EEPROM_WriteBytes(ee_addr, raw, BOARD_IDENTITY_EE_SLOT_SIZE)) return 2;
	}
	return 0;
}
