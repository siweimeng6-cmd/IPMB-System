#ifndef __BSP_BASICMODE_H
#define __BSP_BASICMODE_H

#include "stm32f10x.h"

/* IPMI Serial/Modem "Basic Mode" 成帧特殊字节, 转义规则来自友商 ipmudtool 反汇编还原
 * (H5K16M32004/ipmudtool), 跟 ipmitool 开源的 serial-basic 传输层一致 */
#define BASICMODE_START            0xA0
#define BASICMODE_STOP             0xA5
#define BASICMODE_HANDSHAKE        0xA6
#define BASICMODE_ESCAPE           0xAA

/**
 * @brief  把一个原始 IPMB 帧编码成 Basic Mode 裸字节流(带起始/转义/结束)
 * @param  frame      原始 IPMB 帧
 * @param  frame_len  原始帧长度
 * @param  out        输出缓冲区
 * @param  out_cap    输出缓冲区容量
 * @return 编码后长度, 0 表示 out_cap 不够
 */
uint16_t BasicMode_Encode(const uint8_t* frame, uint16_t frame_len, uint8_t* out, uint16_t out_cap);

/**
 * @brief  从一段裸字节流(通常是一次 UART IDLE 间隔内收到的整包)里解出一个 Basic Mode 帧
 * @param  raw      裸字节流(含起始/转义/结束字节)
 * @param  raw_len  裸字节流长度
 * @param  out      输出缓冲区(还原后的原始 IPMB 帧)
 * @param  out_cap  输出缓冲区容量
 * @return 还原后帧长度, 0 表示没找到合法帧(没有起始/结束字节、转义序列非法、或 out_cap 不够)
 */
uint16_t BasicMode_Decode(const uint8_t* raw, uint16_t raw_len, uint8_t* out, uint16_t out_cap);

#endif
