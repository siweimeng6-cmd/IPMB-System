#include "stm32f10x.h"
#include "bsp_bl_tick.h"
#include "bsp_bl_usart.h"
#include "bsp_flash_iap.h"

/* App 侧收到 Reset 命令(见 User/usart/task_fw_update.c)时会往这个寄存器写这个魔数
 * 再软复位, Bootloader 开机看到魔数就知道暂存区里有待安装的新固件 */
#define BL_MAGIC_VALUE      ((uint16_t)0xA55A)
#define BL_BKP_MAGIC        BKP_DR1
#define BL_BKP_LEN_LO       BKP_DR2
#define BL_BKP_LEN_HI       BKP_DR3
#define BL_BKP_RETRY        BKP_DR4

/* 搬运失败(比如擦到一半掉电)时下次上电会拿暂存区重试, 但不能无限重试下去 */
#define BL_MAX_RETRY        3

#define APP_VECTOR_TABLE    APP_FLASH_START   /* 0x08008000, 见 bsp_flash_iap.h */

typedef void (*pFunction)(void);

static uint16_t read_bkp_magic(void)
{
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	return BKP_ReadBackupRegister(BL_BKP_MAGIC);
}

static uint32_t read_bkp_total_len(void)
{
	uint32_t lo = BKP_ReadBackupRegister(BL_BKP_LEN_LO);
	uint32_t hi = BKP_ReadBackupRegister(BL_BKP_LEN_HI);
	return lo | (hi << 16);
}

static void clear_bkp_magic(void)
{
	PWR_BackupAccessCmd(ENABLE);
	BKP_WriteBackupRegister(BL_BKP_MAGIC, 0x0000);
	BKP_WriteBackupRegister(BL_BKP_RETRY, 0x0000);
}

/*
*********************************************************************************************************
*	函 数 名: install_staged_image
*	功能说明: 把暂存区里的新固件装到 App 区。ipmudtool 的 /appflash 在发完 Reset 命令
*	          之后就不再发任何字节了(只打印"bootloader is upgrading, wait 30 seconds"
*	          干等30秒), 所以这一步必须完全自主完成, 不能等主机握手
*	形    参: total_len:App 态收到的固件总字节数(含末尾4字节CRC尾巴)
*	返 回 值: 1 装好了  0 失败(此时 App 区可能已被擦, 交给调用方按重试次数处理)
*********************************************************************************************************
*/
static uint8_t install_staged_image(uint32_t total_len)
{
	BL_Debug_Print("[BL] 正在擦除 App 区...\r\n");
	if (!FlashIAP_EraseAppRegion()) {
		BL_Debug_Print("[BL] App 区擦除失败!\r\n");
		return 0;
	}

	BL_Debug_Print("[BL] 正在写入新固件...\r\n");
	if (!FlashIAP_CopyStagingToApp(total_len)) {
		BL_Debug_Print("[BL] 新固件写入失败!\r\n");
		return 0;
	}

	BL_Debug_Print("[BL] 升级完成, 即将重启进入新固件\r\n");
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: handle_update_request
*	功能说明: 处理"有新固件待安装"这条路径。擦写中途掉电的话, 暂存区数据还是完好的,
*	          下次上电魔数仍在会自动重试一次, 所以魔数只在装成功后才清; 但重试次数有
*	          上限, 避免变成无限重启循环
*	形    参: 无
*	返 回 值: 无(总是以复位结束, 不返回)
*********************************************************************************************************
*/
static void handle_update_request(void)
{
	uint32_t total_len = read_bkp_total_len();
	uint16_t retry     = BKP_ReadBackupRegister(BL_BKP_RETRY);

	BL_Debug_Print("\r\n[BL] 检测到升级请求\r\n");
	BL_Debug_PrintHex32("[BL] 暂存区镜像长度 = ", total_len);

	/* 先验 CRC 再动 App 区: 校验不过就一个字节都不擦, 老固件毫发无损。尾巴由
	 * Tools/Append-Crc.ps1 补, ipmudtool 自己也用同一套算法做本地校验。
	 * 镜像本身是坏的, 重试多少次都是坏的, 所以这种情况直接清魔数放弃, 不占用重试次数 */
	if (!FlashIAP_VerifyStaging(total_len)) {
		BL_Debug_Print("[BL] 暂存区 CRC 校验失败, 放弃升级, 继续跑原固件\r\n");
		clear_bkp_magic();
		NVIC_SystemReset();
	}
	BL_Debug_Print("[BL] 暂存区 CRC 校验通过\r\n");

	if (retry >= BL_MAX_RETRY) {
		BL_Debug_Print("[BL] 擦写重试次数已达上限, 放弃本次升级\r\n");
		clear_bkp_magic();
		NVIC_SystemReset();
	}

	PWR_BackupAccessCmd(ENABLE);
	BKP_WriteBackupRegister(BL_BKP_RETRY, (uint16_t)(retry + 1));

	if (install_staged_image(total_len)) {
		clear_bkp_magic();
	}

	NVIC_SystemReset();
}

/*
*********************************************************************************************************
*	函 数 名: jump_to_app
*	功能说明: 标准 STM32 IAP 跳转套路: 校验App起始栈指针落在SRAM范围内(粗略判断
*	          App区是不是有效程序, 不是就停在这里等 SWD 重新烧录, 不瞎跳进垃圾
*	          指令), 然后重定位向量表、设置主栈指针、跳到 App 的 Reset_Handler
*	形    参: 无
*	返 回 值: 无(正常情况下不会返回)
*********************************************************************************************************
*/
static void jump_to_app(void)
{
	uint32_t app_sp    = *(volatile uint32_t*)(APP_VECTOR_TABLE);
	uint32_t app_reset = *(volatile uint32_t*)(APP_VECTOR_TABLE + 4);
	pFunction app_entry;

	if (app_sp < 0x20000000UL || app_sp > 0x20010000UL) {
		/* App 区看起来没有有效程序, 停在这里比瞎跳进垃圾指令安全,
		 * 用 SWD 重新烧一次 App 就能恢复 */
		while (1) {
		}
	}

	__disable_irq();
	SCB->VTOR = APP_VECTOR_TABLE;
	__set_MSP(app_sp);
	app_entry = (pFunction)app_reset;
	app_entry();
}

int main(void)
{
	BL_Tick_Init();
	BL_UART4_Init(115200);

	if (read_bkp_magic() == BL_MAGIC_VALUE) {
		handle_update_request();   /* 内部总是以复位结束, 不会返回 */
	}

	jump_to_app();

	while (1) {
	}
}
