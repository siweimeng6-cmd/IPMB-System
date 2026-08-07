#include "task_fw_update.h"
#include "bsp_basicmode.h"
#include "bsp_init.h"

/* OEM 固件升级命令族: netFn=0x34(OEM), cmd=0xF0, 子命令见 data[0]。
 * 字节值来自友商 ipmudtool 反汇编还原(H5K16M32004/ipmudtool), 详见开发计划文档 */
#define FWUPDATE_NETFN            0x34
#define FWUPDATE_CMD              0xF0
#define FWUPDATE_SUBCMD_PRE       0x01
#define FWUPDATE_SUBCMD_ERASE     0x02
#define FWUPDATE_SUBCMD_PROGRAM   0x03
#define FWUPDATE_SUBCMD_RESET     0x05

/* Program 分片包固定 300 字节, 内部偏移来自 UARTR_Program(0x403adc) 反汇编还原,
 * 见开发计划文档"已确认的协议常量"表 */
#define FWUPDATE_PROGRAM_PKT_LEN    300
#define FWUPDATE_CHUNK_PAYLOAD_MAX  256
#define FWUPDATE_OFF_LASTBYTE_MARK  10   /* seq 最高字节位置, 末包时被特殊编码覆盖 */
#define FWUPDATE_OFF_CHUNKLEN       11   /* 2B, 小端 */
#define FWUPDATE_OFF_PAYLOAD        13   /* 256B */
#define FWUPDATE_OFF_PAYLOAD_CS     269  /* 1B, checksum(pkt[7..268],262), 简单加法不取反 */

/* 跳 Bootloader 用的 BKP 寄存器约定: DR1=魔数(必须跟 Bootloader/main.c 的
 * BL_MAGIC_VALUE 完全一致, 两个工程是分开编译烧录的独立镜像, 改一边要记得
 * 同步改另一边), DR2/DR3=待接收固件总字节数的低/高16位 */
#define BOOTLOADER_MAGIC            ((uint16_t)0xA55A)

/* 固件暂存区: App 态收到的新固件先写这里, 复位后由 Bootloader 搬到 App 区。
 * 地址必须跟 Bootloader/bsp_flash_iap.h 里的 STAGING_FLASH_START/END 完全一致 */
#define STAGING_FLASH_START         0x08048000UL
#define STAGING_FLASH_END           0x08080000UL
#define FLASH_PAGE_SIZE_BYTES       0x800UL

/*
*********************************************************************************************************
*	函 数 名: send_ack
*	功能说明: 回一个标准 IPMB ack(cc), 通过 Basic Mode 成帧后从 UART5 发出
*	形    参: rqSA:回给谁(即收到的请求里的rqSA)  rqSeq:回显请求序列号  cc:完成码
*	返 回 值: 无
*********************************************************************************************************
*/
static void send_ack(uint8_t rqSA, uint8_t rqSeq, uint8_t cc)
{
	uint8_t resp[16];
	uint8_t resp_len;
	uint8_t wire[40];
	uint16_t wire_len;

	resp_len = IPMB_Build_Response(resp, rqSA, FWUPDATE_NETFN, 0,
	                                IPMB_ADDR_HOST, rqSeq, FWUPDATE_CMD,
	                                cc, NULL, 0);

	wire_len = BasicMode_Encode(resp, resp_len, wire, sizeof(wire));
	if (wire_len > 0) {
		Usart_SendArray(COM_USART5, wire, wire_len);
	}
}

/* 一次升级会话的运行状态, Pre 命令时清零, Program 分片逐包累加 */
static uint8_t  s_program_active      = 0;
static uint32_t s_program_bytes_total = 0;
static uint8_t  s_program_running_cs  = 0;  /* 简单加法校验和(不取反), 跟 Bootloader
                                              * 搬运完回读的结果比对用 */

/* 暂存区写入状态 */
static uint32_t s_staging_addr  = STAGING_FLASH_START;  /* 下一个字要写的地址 */
static uint32_t s_staging_erased = STAGING_FLASH_START; /* 已擦除到此地址(不含) */
static uint8_t  s_word_buf[4];
static uint8_t  s_word_idx = 0;

static void staging_reset(void)
{
	s_staging_addr   = STAGING_FLASH_START;
	s_staging_erased = STAGING_FLASH_START;
	s_word_idx       = 0;
}

/*
*********************************************************************************************************
*	函 数 名: staging_ensure_erased
*	功能说明: 保证 addr 所在的页已经擦过。按需一页一页擦(2KB/页, 约40ms), 而不是在
*	          Erase 命令里一次性擦完整个224KB——那要好几秒, 会超出 ipmudtool 的串口
*	          等待时间导致它判定升级失败
*	形    参: addr:即将写入的地址
*	返 回 值: 1 可以写  0 越界或擦除失败
*********************************************************************************************************
*/
static uint8_t staging_ensure_erased(uint32_t addr)
{
	while (addr >= s_staging_erased) {
		if (s_staging_erased >= STAGING_FLASH_END) {
			return 0;
		}
		if (FLASH_ErasePage(s_staging_erased) != FLASH_COMPLETE) {
			return 0;
		}
		s_staging_erased += FLASH_PAGE_SIZE_BYTES;
	}
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: staging_write_bytes
*	功能说明: 把 len 个字节顺序追加写进暂存区。flash 只能按字(4字节)写, 所以内部攒够
*	          4 字节才落盘, 不足的留在 s_word_buf 里等下一包(只有最后一包可能不是4的
*	          整数倍, 由 staging_flush 补 0xFF 收尾)
*	形    参: data/len
*	返 回 值: 1 成功  0 失败(越界/擦除失败/写失败)
*********************************************************************************************************
*/
static uint8_t staging_write_bytes(const uint8_t* data, uint16_t len)
{
	uint16_t i;
	uint32_t word;

	for (i = 0; i < len; i++) {
		s_word_buf[s_word_idx++] = data[i];
		if (s_word_idx < 4) {
			continue;
		}
		word = (uint32_t)s_word_buf[0] |
		       ((uint32_t)s_word_buf[1] << 8) |
		       ((uint32_t)s_word_buf[2] << 16) |
		       ((uint32_t)s_word_buf[3] << 24);
		if (!staging_ensure_erased(s_staging_addr)) {
			return 0;
		}
		if (FLASH_ProgramWord(s_staging_addr, word) != FLASH_COMPLETE) {
			return 0;
		}
		s_staging_addr += 4;
		s_word_idx = 0;
	}
	return 1;
}

/* 收尾: 把不足4字节的尾巴用 0xFF 补齐后写下去(0xFF 是 flash 擦除后的默认值) */
static uint8_t staging_flush(void)
{
	uint32_t word;

	if (s_word_idx == 0) {
		return 1;
	}
	while (s_word_idx < 4) {
		s_word_buf[s_word_idx++] = 0xFF;
	}
	word = (uint32_t)s_word_buf[0] |
	       ((uint32_t)s_word_buf[1] << 8) |
	       ((uint32_t)s_word_buf[2] << 16) |
	       ((uint32_t)s_word_buf[3] << 24);
	s_word_idx = 0;

	if (!staging_ensure_erased(s_staging_addr)) {
		return 0;
	}
	if (FLASH_ProgramWord(s_staging_addr, word) != FLASH_COMPLETE) {
		return 0;
	}
	s_staging_addr += 4;
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: simple_checksum
*	功能说明: 简单加法校验和(不取反), 对应友商代码里的 checksum(), 区别于 IPMB 用的
*	          两's补码校验和(IPMB_Calc_Checksum)
*	形    参: buf/len
*	返 回 值: 8-bit 累加和
*********************************************************************************************************
*/
static uint8_t simple_checksum(const uint8_t* buf, uint16_t len)
{
	uint16_t sum = 0;
	uint16_t i;
	for (i = 0; i < len; i++) {
		sum += buf[i];
	}
	return (uint8_t)sum;
}

/*
*********************************************************************************************************
*	函 数 名: handle_program
*	功能说明: 处理一个 Program 分片包(300字节, 见开发计划字节排布表): 校验负载校验和,
*	          累加进本次会话的运行校验和, 并把负载顺序写进暂存区
*	形    参: frame:已解码的300字节包  rqSA/rqSeq:用于回 ack
*	返 回 值: 无
*********************************************************************************************************
*/
static void handle_program(const uint8_t* frame, uint8_t rqSA, uint8_t rqSeq)
{
	uint16_t chunklen;
	uint8_t  payload_cs;
	uint8_t  is_last;
	uint8_t  ok;
	uint16_t i;

	if (!s_program_active) {
		printf("[FWUPDATE] Program 在 Pre 之前收到, 忽略\r\n");
		send_ack(rqSA, rqSeq, IPMB_CC_INVALID_COMMAND);
		return;
	}

	chunklen = (uint16_t)frame[FWUPDATE_OFF_CHUNKLEN] |
	           ((uint16_t)frame[FWUPDATE_OFF_CHUNKLEN + 1] << 8);
	if (chunklen > FWUPDATE_CHUNK_PAYLOAD_MAX) {
		printf("[FWUPDATE] Program chunklen=%u 非法(最大256), 丢弃\r\n", chunklen);
		send_ack(rqSA, rqSeq, IPMB_CC_DATA_OUT_OF_RANGE);
		return;
	}

	/* 附加负载校验和: checksum(pkt[7..268], 262), 简单加法不取反 */
	payload_cs = simple_checksum(&frame[7], 262);
	if (payload_cs != frame[FWUPDATE_OFF_PAYLOAD_CS]) {
		printf("[FWUPDATE] Program 负载校验和错误: 算得0x%02X, 帧里0x%02X\r\n",
		       payload_cs, frame[FWUPDATE_OFF_PAYLOAD_CS]);
		send_ack(rqSA, rqSeq, IPMB_CC_INVALID_DATA_FIELD);
		return;
	}

	for (i = 0; i < chunklen; i++) {
		s_program_running_cs = (uint8_t)(s_program_running_cs + frame[FWUPDATE_OFF_PAYLOAD + i]);
	}

	/* 末包判定: chunklen<256 是最直接的信号(文件大小不是256整数倍时必然出现);
	 * 反汇编里末包 seq 最高字节被覆盖成 0x80(bit7置1)或 0xAB, 作为文件大小刚好是
	 * 256整数倍这种边界情况的补充信号 */
	is_last = (chunklen < FWUPDATE_CHUNK_PAYLOAD_MAX) ||
	          (frame[FWUPDATE_OFF_LASTBYTE_MARK] == 0x80) ||
	          (frame[FWUPDATE_OFF_LASTBYTE_MARK] == 0xAB);

	/* 落盘到暂存区。复位后 Bootloader 从这里搬到 App 区——ipmudtool 的 /appflash
	 * 在 Reset 之后不再发任何字节(它只等30秒), 所以数据必须在 App 态就存下来 */
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
	ok = staging_write_bytes(&frame[FWUPDATE_OFF_PAYLOAD], chunklen);
	if (ok && is_last) {
		ok = staging_flush();
	}
	FLASH_Lock();

	if (!ok) {
		printf("[FWUPDATE] 暂存区写入失败(地址0x%08lX), 中止本次升级\r\n",
		       (unsigned long)s_staging_addr);
		s_program_active = 0;
		send_ack(rqSA, rqSeq, IPMB_CC_UNSPECIFIED);
		return;
	}

	s_program_bytes_total += chunklen;

	if (is_last) {
		printf("[FWUPDATE] Program 收完: 共 %lu 字节, 运行校验和=0x%02X, 已存入暂存区\r\n",
		       (unsigned long)s_program_bytes_total, s_program_running_cs);
	} else {
		printf("[FWUPDATE] Program 收到一包: %u 字节 (累计 %lu)\r\n",
		       chunklen, (unsigned long)s_program_bytes_total);
	}

	send_ack(rqSA, rqSeq, IPMB_CC_OK);
}

/*
*********************************************************************************************************
*	函 数 名: trigger_bootloader_reset
*	功能说明: 把待接收固件总字节数写进 BKP_DR2/DR3, 再写魔数进 BKP_DR1, 最后软复位。
*	          Bootloader 开机看到魔数就知道要进 BB 态, 见 Bootloader/main.c
*	形    参: total_len:本次升级收到的固件总字节数
*	返 回 值: 无(执行完会复位, 不会返回)
*********************************************************************************************************
*/
static void trigger_bootloader_reset(uint32_t total_len)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	PWR_BackupAccessCmd(ENABLE);
	BKP_WriteBackupRegister(BKP_DR2, (uint16_t)(total_len & 0xFFFF));
	BKP_WriteBackupRegister(BKP_DR3, (uint16_t)((total_len >> 16) & 0xFFFF));
	BKP_WriteBackupRegister(BKP_DR1, BOOTLOADER_MAGIC);

	NVIC_SystemReset();
}

void FwUpdate_HandleFrame(const uint8_t* raw, uint16_t raw_len)
{
	/* 300 字节够装最大的 Program 分片包, Pre/Erase/Reset 这种短命令也一起用这个 */
	uint8_t frame[FWUPDATE_PROGRAM_PKT_LEN];
	uint16_t frame_len;
	uint8_t rqSA, rqSeq, subcmd;

	frame_len = BasicMode_Decode(raw, raw_len, frame, sizeof(frame));
	if (frame_len < 8) {
		return;
	}
	if (IPMB_Verify_Checksum(frame, frame_len) != 0) {
		printf("[FWUPDATE] checksum error\r\n");
		return;
	}
	if (frame[0] != IPMB_ADDR_HOST ||
	    IPMB_EXTRACT_NETFN(frame[1]) != FWUPDATE_NETFN ||
	    frame[5] != FWUPDATE_CMD) {
		return;   /* 不是发给本命令族的帧 */
	}

	rqSA   = frame[3];
	rqSeq  = IPMB_Extract_RqSeq(frame);
	subcmd = frame[6];

	switch (subcmd) {
	case FWUPDATE_SUBCMD_PRE:
		printf("[FWUPDATE] Pre\r\n");
		s_program_active      = 1;
		s_program_bytes_total = 0;
		s_program_running_cs  = 0;
		staging_reset();
		send_ack(rqSA, rqSeq, IPMB_CC_OK);
		break;

	case FWUPDATE_SUBCMD_ERASE:
		/* 这里不真的擦 flash: 整片擦掉224KB暂存区要好几秒, ipmudtool 等不了那么久。
		 * 改成 Program 阶段"写到哪页才擦哪页"(staging_ensure_erased), 把擦除开销
		 * 摊到每8个包一次(约40ms), 所以本命令只需要复位写指针后立刻回 ack */
		printf("[FWUPDATE] Erase (复位暂存区写指针)\r\n");
		staging_reset();
		send_ack(rqSA, rqSeq, IPMB_CC_OK);
		break;

	case FWUPDATE_SUBCMD_PROGRAM:
		if (frame_len < FWUPDATE_PROGRAM_PKT_LEN) {
			printf("[FWUPDATE] Program 帧长度不对: %u(期望%u)\r\n",
			       frame_len, (unsigned)FWUPDATE_PROGRAM_PKT_LEN);
			send_ack(rqSA, rqSeq, IPMB_CC_REQUEST_DATA_LEN_INV);
			break;
		}
		handle_program(frame, rqSA, rqSeq);
		break;

	case FWUPDATE_SUBCMD_RESET:
		/* 必须先完整收完一轮 Program 才允许跳 Bootloader, 防止误触发/单独一条
		 * Reset 命令就把板子跳进"擦了App区却没数据可写"的坏状态 */
		if (!s_program_active || s_program_bytes_total == 0) {
			printf("[FWUPDATE] Reset 被拒绝: 还没收完 Program\r\n");
			send_ack(rqSA, rqSeq, IPMB_CC_INVALID_COMMAND);
			break;
		}
		printf("[FWUPDATE] Reset: 共%lu字节/运行校验和0x%02X, 跳转 Bootloader\r\n",
		       (unsigned long)s_program_bytes_total, s_program_running_cs);
		s_program_active = 0;
		send_ack(rqSA, rqSeq, IPMB_CC_OK);
		trigger_bootloader_reset(s_program_bytes_total);
		break;

	default:
		send_ack(rqSA, rqSeq, IPMB_CC_INVALID_DATA_FIELD);
		break;
	}
}
