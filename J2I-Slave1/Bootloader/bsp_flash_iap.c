#include "bsp_flash_iap.h"

void FlashIAP_Unlock(void)
{
	FLASH_Unlock();
	FLASH_ClearFlag(FLASH_FLAG_BSY | FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPRTERR);
}

void FlashIAP_Lock(void)
{
	FLASH_Lock();
}

uint8_t FlashIAP_EraseAppRegion(void)
{
	uint32_t addr;
	FLASH_Status status;

	FlashIAP_Unlock();

	for (addr = APP_FLASH_START; addr < APP_FLASH_END; addr += FLASH_PAGE_SIZE_BYTES) {
		status = FLASH_ErasePage(addr);
		if (status != FLASH_COMPLETE) {
			FlashIAP_Lock();
			return 0;
		}
	}

	FlashIAP_Lock();
	return 1;
}

uint8_t FlashIAP_ProgramWord(uint32_t addr, uint32_t data)
{
	return (FLASH_ProgramWord(addr, data) == FLASH_COMPLETE) ? 1 : 0;
}

/*
*********************************************************************************************************
*	函 数 名: crc32_mpeg2_step
*	功能说明: CRC-32/MPEG-2 单字节推进(多项式0x04C11DB7, 高位在前, 不反转输入输出,
*	          无最终异或)。用逐位算法而不是256项查表, 省下 Bootloader 里 1KB 的常量表
*	          ——47KB 镜像全算完约40ms, 完全够快
*	形    参: crc:当前值  byte:新字节
*	返 回 值: 推进后的 crc
*********************************************************************************************************
*/
static uint32_t crc32_mpeg2_step(uint32_t crc, uint8_t byte)
{
	uint8_t i;

	crc ^= ((uint32_t)byte) << 24;
	for (i = 0; i < 8; i++) {
		if (crc & 0x80000000UL) {
			crc = (crc << 1) ^ 0x04C11DB7UL;
		} else {
			crc <<= 1;
		}
	}
	return crc;
}

uint8_t FlashIAP_VerifyStaging(uint32_t len)
{
	uint32_t crc = 0xFFFFFFFFUL;
	uint32_t addr;
	uint32_t end;
	uint32_t word;

	/* 带 CRC 尾巴的镜像至少要有尾巴本身, 且必然是4字节整数倍(Tools/Append-Crc.ps1
	 * 补尾巴前会先把文件补齐到4的倍数)。装不进暂存区或装不进 App 区的也直接拒绝 */
	if (len < 8 || (len % 4) != 0) {
		return 0;
	}
	if (len > (STAGING_FLASH_END - STAGING_FLASH_START) ||
	    len > (APP_FLASH_END - APP_FLASH_START)) {
		return 0;
	}

	/* 算法跟 ipmudtool 的 CalculateCRC32 一致: 先把每4字节一组组内字节反转再算 CRC。
	 * flash 是小端存的, 所以直接按字读出来、再从最高字节往最低字节喂, 就等价于
	 * "组内反转后逐字节喂", 不用真的在内存里翻转一遍 */
	end = STAGING_FLASH_START + len;
	for (addr = STAGING_FLASH_START; addr < end; addr += 4) {
		word = *(volatile uint32_t*)addr;
		crc = crc32_mpeg2_step(crc, (uint8_t)(word >> 24));
		crc = crc32_mpeg2_step(crc, (uint8_t)(word >> 16));
		crc = crc32_mpeg2_step(crc, (uint8_t)(word >> 8));
		crc = crc32_mpeg2_step(crc, (uint8_t)word);
	}

	return (crc == 0) ? 1 : 0;
}

uint8_t FlashIAP_CopyStagingToApp(uint32_t len)
{
	uint32_t i;
	uint32_t word;
	uint8_t  ok = 1;

	if (len > (APP_FLASH_END - APP_FLASH_START)) {
		return 0;
	}

	FlashIAP_Unlock();
	for (i = 0; i < len; i += 4) {
		word = *(volatile uint32_t*)(STAGING_FLASH_START + i);
		if (FLASH_ProgramWord(APP_FLASH_START + i, word) != FLASH_COMPLETE) {
			ok = 0;
			break;
		}
	}
	FlashIAP_Lock();

	return ok;
}
