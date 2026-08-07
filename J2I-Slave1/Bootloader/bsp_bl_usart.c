#include "bsp_bl_usart.h"
#include "bsp_bl_tick.h"

#define BL_UART5_TX_PORT   GPIOC
#define BL_UART5_TX_PIN    GPIO_Pin_12
#define BL_UART5_RX_PORT   GPIOD
#define BL_UART5_RX_PIN    GPIO_Pin_2

#define BL_UART4_TX_PORT   GPIOC
#define BL_UART4_TX_PIN    GPIO_Pin_10

void BL_UART5_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef usart;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART5, ENABLE);

	gpio.GPIO_Pin   = BL_UART5_TX_PIN;
	gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BL_UART5_TX_PORT, &gpio);

	gpio.GPIO_Pin  = BL_UART5_RX_PIN;
	gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
	GPIO_Init(BL_UART5_RX_PORT, &gpio);

	usart.USART_BaudRate            = baudrate;
	usart.USART_WordLength          = USART_WordLength_8b;
	usart.USART_StopBits            = USART_StopBits_1;
	usart.USART_Parity              = USART_Parity_No;
	usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart.USART_Mode                = USART_Mode_Rx | USART_Mode_Tx;
	USART_Init(UART5, &usart);

	USART_Cmd(UART5, ENABLE);
}

void BL_UART5_SendByte(uint8_t b)
{
	USART_SendData(UART5, b);
	while (USART_GetFlagStatus(UART5, USART_FLAG_TXE) == RESET) {
	}
}

void BL_UART5_SendArray(const uint8_t* buf, uint16_t len)
{
	uint16_t i;
	for (i = 0; i < len; i++) {
		BL_UART5_SendByte(buf[i]);
	}
	while (USART_GetFlagStatus(UART5, USART_FLAG_TC) == RESET) {
	}
}

uint8_t BL_UART5_RecvByte(uint8_t* out, uint32_t timeout_ms)
{
	uint32_t start = BL_GetTick();

	while (USART_GetFlagStatus(UART5, USART_FLAG_RXNE) == RESET) {
		if ((BL_GetTick() - start) >= timeout_ms) {
			return 0;
		}
	}
	*out = (uint8_t)USART_ReceiveData(UART5);
	return 1;
}

void BL_UART4_Init(uint32_t baudrate)
{
	GPIO_InitTypeDef gpio;
	USART_InitTypeDef usart;

	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);

	gpio.GPIO_Pin   = BL_UART4_TX_PIN;
	gpio.GPIO_Mode  = GPIO_Mode_AF_PP;
	gpio.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(BL_UART4_TX_PORT, &gpio);

	usart.USART_BaudRate            = baudrate;
	usart.USART_WordLength          = USART_WordLength_8b;
	usart.USART_StopBits            = USART_StopBits_1;
	usart.USART_Parity              = USART_Parity_No;
	usart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
	usart.USART_Mode                = USART_Mode_Tx;
	USART_Init(UART4, &usart);

	USART_Cmd(UART4, ENABLE);
}

void BL_Debug_Print(const char* s)
{
	while (*s) {
		USART_SendData(UART4, (uint8_t)*s++);
		while (USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET) {
		}
	}
	while (USART_GetFlagStatus(UART4, USART_FLAG_TC) == RESET) {
	}
}

void BL_Debug_PrintHex32(const char* prefix, uint32_t value)
{
	static const char hex[] = "0123456789ABCDEF";
	char buf[11];
	uint8_t i;

	buf[0] = '0';
	buf[1] = 'x';
	for (i = 0; i < 8; i++) {
		buf[2 + i] = hex[(value >> ((7 - i) * 4)) & 0x0F];
	}
	buf[10] = '\0';

	BL_Debug_Print(prefix);
	BL_Debug_Print(buf);
	BL_Debug_Print("\r\n");
}
