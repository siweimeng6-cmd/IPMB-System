#include "bsp_basicmode.h"

/* 特殊字节 <-> 转义替代字节 对照表, 跟 ipmudtool 反汇编还原的一致 */
static const struct {
	uint8_t character;
	uint8_t escape;
} s_escape_table[] = {
	{ BASICMODE_START,     0xB0 },
	{ BASICMODE_STOP,      0xB5 },
	{ BASICMODE_HANDSHAKE, 0xB6 },
	{ BASICMODE_ESCAPE,    0xBA },
	{ 0x1B,                0x3B },
};
#define ESCAPE_TABLE_SIZE   (sizeof(s_escape_table) / sizeof(s_escape_table[0]))

static uint8_t get_escaped_char(uint8_t c)
{
	uint8_t i;
	for (i = 0; i < ESCAPE_TABLE_SIZE; i++) {
		if (s_escape_table[i].character == c) {
			return s_escape_table[i].escape;
		}
	}
	return c;
}

static uint8_t get_unescaped_char(uint8_t c, uint8_t* is_special)
{
	uint8_t i;
	for (i = 0; i < ESCAPE_TABLE_SIZE; i++) {
		if (s_escape_table[i].escape == c) {
			*is_special = 1;
			return s_escape_table[i].character;
		}
	}
	*is_special = 0;
	return c;
}

uint16_t BasicMode_Encode(const uint8_t* frame, uint16_t frame_len, uint8_t* out, uint16_t out_cap)
{
	uint16_t i;
	uint16_t out_len = 0;
	uint8_t escaped;

	if (out_cap < 1) {
		return 0;
	}
	out[out_len++] = BASICMODE_START;

	for (i = 0; i < frame_len; i++) {
		escaped = get_escaped_char(frame[i]);
		if (escaped != frame[i]) {
			if (out_len + 2 > out_cap) {
				return 0;
			}
			out[out_len++] = BASICMODE_ESCAPE;
			out[out_len++] = escaped;
		} else {
			if (out_len + 1 > out_cap) {
				return 0;
			}
			out[out_len++] = frame[i];
		}
	}

	if (out_len + 1 > out_cap) {
		return 0;
	}
	out[out_len++] = BASICMODE_STOP;
	return out_len;
}

uint16_t BasicMode_Decode(const uint8_t* raw, uint16_t raw_len, uint8_t* out, uint16_t out_cap)
{
	uint16_t i;
	uint16_t out_len = 0;
	uint8_t in_frame = 0;
	uint8_t escape_pending = 0;
	uint8_t is_special;
	uint8_t unescaped;

	for (i = 0; i < raw_len; i++) {
		uint8_t c = raw[i];

		if (c == BASICMODE_START) {
			in_frame = 1;
			escape_pending = 0;
			out_len = 0;
			continue;
		}
		if (!in_frame) {
			continue;
		}
		if (escape_pending) {
			unescaped = get_unescaped_char(c, &is_special);
			if (!is_special) {
				/* 非法转义序列, 丢弃当前帧重新等待下一个起始字节 */
				in_frame = 0;
				continue;
			}
			if (out_len >= out_cap) {
				return 0;
			}
			out[out_len++] = unescaped;
			escape_pending = 0;
		} else if (c == BASICMODE_ESCAPE) {
			escape_pending = 1;
		} else if (c == BASICMODE_STOP) {
			return out_len;
		} else if (c == BASICMODE_HANDSHAKE) {
			/* 包握手字节, 跳过 */
			continue;
		} else {
			if (out_len >= out_cap) {
				return 0;
			}
			out[out_len++] = c;
		}
	}

	/* 没等到结束字节, 帧不完整 */
	return 0;
}
