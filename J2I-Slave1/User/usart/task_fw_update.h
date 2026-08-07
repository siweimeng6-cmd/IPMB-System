#ifndef __TASK_FW_UPDATE_H
#define __TASK_FW_UPDATE_H

#include "stm32f10x.h"

/**
 * @brief  处理一段疑似 IPMI Basic Mode 成帧数据(调用方已确认首字节是 0xA0)。
 *         解出 IPMB 帧、校验、按 OEM 固件升级子命令(Pre/Erase/Program/Reset)
 *         分发处理并通过 UART5 回 ack。不是发给本命令族的帧(netFn/cmd 不匹配)
 *         或校验失败则直接忽略、不回应答。
 * @param  raw      一次 UART5 IDLE 间隔内收到的原始裸字节流
 * @param  raw_len  长度
 */
void FwUpdate_HandleFrame(const uint8_t* raw, uint16_t raw_len);

#endif
