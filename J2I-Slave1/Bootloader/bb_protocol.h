#ifndef __BB_PROTOCOL_H
#define __BB_PROTOCOL_H

#include "stm32f10x.h"

/**
 * @brief  BB(Bootloader)态裸协议主循环: 握手(0x10)->擦除(0x11)->接收固件写flash
 *         (0x12)->重启(0xAB)。协议字节表来自反汇编 H5K16M32004/ipmudtool 还原,
 *         见开发计划文档。任何一步失败或超时都会直接返回(不会卡死), 调用方
 *         (main.c)负责在返回后复位, 由下一次开机重新判断一次。
 * @param  total_len  待接收固件总字节数(App阶段已经知道, 通过 BKP 寄存器传过来)
 */
void BB_Protocol_Run(uint32_t total_len);

#endif
