#include "bb_protocol.h"
#include "bsp_bl_usart.h"
#include "bsp_bl_tick.h"
#include "bsp_flash_iap.h"

/* BB态裸令牌: 请求 55 20 <stage> 0D (4字节), 应答 80 55 20 <stage> 0D (5字节)。
 * 字节值来自反汇编还原, 见开发计划"已确认的协议常量"表 */
#define BB_TOKEN_M0        0x55
#define BB_TOKEN_M1        0x20
#define BB_TOKEN_TAIL      0x0D

#define BB_STAGE_HANDSHAKE 0x10
#define BB_STAGE_ERASE     0x11
#define BB_STAGE_PROGRAM   0x12
#define BB_STAGE_RESTART   0xAB

/*
*********************************************************************************************************
*	函 数 名: wait_for_token
*	功能说明: 等待一个 55 20 <stage> 0D 请求令牌, 允许前面夹杂垃圾字节(简单状态机
*	          逐字节匹配, 不对就退回状态0重新找起始字节)
*	形    参: stage:期望的阶段号  timeout_ms:总超时(收到合法令牌前一直重试, 拿不到就放弃)
*	返 回 值: 1 收到  0 超时
*********************************************************************************************************
*/
static uint8_t wait_for_token(uint8_t stage, uint32_t timeout_ms)
{
	uint8_t b;
	uint32_t start = BL_GetTick();
	uint8_t state = 0;

	while ((BL_GetTick() - start) < timeout_ms) {
		if (!BL_UART5_RecvByte(&b, 100)) {
			continue;
		}
		switch (state) {
		case 0: state = (b == BB_TOKEN_M0) ? 1 : 0; break;
		case 1: state = (b == BB_TOKEN_M1) ? 2 : 0; break;
		case 2: state = (b == stage) ? 3 : 0; break;
		case 3:
			if (b == BB_TOKEN_TAIL) {
				return 1;
			}
			state = 0;
			break;
		default: state = 0; break;
		}
	}
	return 0;
}

static void send_ack(uint8_t stage)
{
	uint8_t ack[5];
	ack[0] = 0x80;
	ack[1] = BB_TOKEN_M0;
	ack[2] = BB_TOKEN_M1;
	ack[3] = stage;
	ack[4] = BB_TOKEN_TAIL;
	BL_UART5_SendArray(ack, sizeof(ack));
}

/*
*********************************************************************************************************
*	函 数 名: receive_and_program
*	功能说明: 裸接收 total_len 字节固件数据(无成帧, 无每包应答), 每凑够4字节就写一个
*	          flash 字, 边收边写。收完不足4字节的尾巴用 0xFF 补齐(擦除后本来就是0xFF)
*	形    参: total_len
*	返 回 值: 1 成功  0 超时/写入失败
*********************************************************************************************************
*/
static uint8_t receive_and_program(uint32_t total_len)
{
	uint32_t addr = APP_FLASH_START;
	uint32_t received = 0;
	uint8_t word_buf[4];
	uint8_t word_idx = 0;
	uint8_t b;
	uint8_t ok = 1;

	FlashIAP_Unlock();

	while (received < total_len) {
		if (!BL_UART5_RecvByte(&b, 5000)) {
			ok = 0;
			break;
		}
		word_buf[word_idx++] = b;
		received++;

		if (word_idx == 4) {
			uint32_t data = (uint32_t)word_buf[0] |
			                 ((uint32_t)word_buf[1] << 8) |
			                 ((uint32_t)word_buf[2] << 16) |
			                 ((uint32_t)word_buf[3] << 24);
			if (!FlashIAP_ProgramWord(addr, data)) {
				ok = 0;
				break;
			}
			addr += 4;
			word_idx = 0;
		}
	}

	if (ok && word_idx > 0) {
		uint32_t data;
		while (word_idx < 4) {
			word_buf[word_idx++] = 0xFF;
		}
		data = (uint32_t)word_buf[0] |
		       ((uint32_t)word_buf[1] << 8) |
		       ((uint32_t)word_buf[2] << 16) |
		       ((uint32_t)word_buf[3] << 24);
		ok = FlashIAP_ProgramWord(addr, data);
	}

	FlashIAP_Lock();
	return ok;
}

void BB_Protocol_Run(uint32_t total_len)
{
	/* 握手: 已经通过 BKP 魔数进了 BB 态, host 随时可能连上来, 多等一会(5分钟)。
	 * 真等不到就放弃、让 main.c 复位回退到正常开机流程, 不会死等到宕机 */
	if (!wait_for_token(BB_STAGE_HANDSHAKE, 300000UL)) {
		return;
	}
	send_ack(BB_STAGE_HANDSHAKE);

	if (!wait_for_token(BB_STAGE_ERASE, 30000UL)) {
		return;
	}
	if (!FlashIAP_EraseAppRegion()) {
		return;
	}
	send_ack(BB_STAGE_ERASE);

	if (!receive_and_program(total_len)) {
		return;
	}
	send_ack(BB_STAGE_PROGRAM);

	/* 重启这一步手册/反汇编都没看到 Bootloader 要回应答, host 发完就等着掉电/
	 * 主板自动开机, 这里收到就直接返回, 交给 main.c 复位 */
	wait_for_token(BB_STAGE_RESTART, 30000UL);
}
