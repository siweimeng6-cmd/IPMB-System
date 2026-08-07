#ifndef __BSP_FLASH_IAP_H
#define __BSP_FLASH_IAP_H

#include "stm32f10x.h"

/* 512KB flash 三分区(见开发计划 M8):
 *   0x08000000~0x08008000  32KB   Bootloader 自己
 *   0x08008000~0x08048000  256KB  App 区
 *   0x08048000~0x08080000  224KB  固件暂存区(App 态收到的新固件先落在这)
 * STAGING_* 两个地址必须跟 User/usart/task_fw_update.c 里的同名宏完全一致 */
#define APP_FLASH_START        0x08008000UL
#define APP_FLASH_END          0x08048000UL
#define STAGING_FLASH_START    0x08048000UL
#define STAGING_FLASH_END      0x08080000UL
#define FLASH_PAGE_SIZE_BYTES  0x800UL   /* STM32F103 高密度型号(256~512KB) 2KB/页 */

void FlashIAP_Unlock(void);
void FlashIAP_Lock(void);

/* 擦除整个 App 区, 内部自己 unlock/lock。返回 1 成功 0 失败 */
uint8_t FlashIAP_EraseAppRegion(void);

/* 写一个字(4字节, 地址必须4字节对齐), 调用前必须先 FlashIAP_Unlock()。
 * 返回 1 成功 0 失败 */
uint8_t FlashIAP_ProgramWord(uint32_t addr, uint32_t data);

/* 校验暂存区里长度为 len 的镜像: 末4字节是 ipmudtool 那套 CRC-32/MPEG-2 尾巴,
 * 整段(含尾巴)按同样算法算出来应该等于0。返回 1 合法 0 不合法 */
uint8_t FlashIAP_VerifyStaging(uint32_t len);

/* 把暂存区前 len 个字节搬到 App 区(调用前 App 区必须已经擦干净)。
 * 返回 1 成功 0 失败 */
uint8_t FlashIAP_CopyStagingToApp(uint32_t len);

#endif
