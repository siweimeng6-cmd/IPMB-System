#ifndef __BSP_BL_USART_H
#define __BSP_BL_USART_H

#include "stm32f10x.h"

/* UART5(功能串口), TX=PC12/RX=PD2, 跟 App 侧 bsp_usart.h 的 COM_USART5 配置一致。
 * Bootloader 是裸机程序, 没有 FreeRTOS 信号量, 改用查询方式收发, 不需要中断 */
void BL_UART5_Init(uint32_t baudrate);
void BL_UART5_SendByte(uint8_t b);
void BL_UART5_SendArray(const uint8_t* buf, uint16_t len);

/**
 * @brief  带超时的查询收字节
 * @param  out         收到的字节存到这里
 * @param  timeout_ms  超时时间(毫秒)
 * @return 1=收到  0=超时
 */
uint8_t BL_UART5_RecvByte(uint8_t* out, uint32_t timeout_ms);

/* UART4(调试口), TX=PC10/RX=PC11, 跟 App 侧 DEBUG_USARTx 是同一个口。只发不收,
 * 让 Bootloader 阶段的动作在调试串口上可见(以前全程静默, 出问题很难定位) */
void BL_UART4_Init(uint32_t baudrate);
void BL_Debug_Print(const char* s);
void BL_Debug_PrintHex32(const char* prefix, uint32_t value);

#endif
