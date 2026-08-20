#ifndef __BSP_I2C_H
#define	__BSP_I2C_H

#include "stm32f4xx.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"

#include "task_i2c.h"

/* STM32 I2C ����ģʽ —— 2026-07-29改成运行时变量(原来是#define),
 * 供 IPMB 总线速率切换(control_req 0x10/0x11)在运行期改写,见 bsp_i2c.c */
extern volatile uint32_t I2C_Speed;
void Ipmb_SetBusSpeed(uint32_t speed_hz);
void Ipmb_I2C1_ApplyPendingSpeed(void);
void Ipmb_I2C2_ApplyPendingSpeed(void);
#define COM_I2C1_ADDRESS      						0x0A
#define COM_I2C2_ADDRESS      						0x0B
#define COM_I2C3_ADDRESS      						0x0C

/* OAR2 — 第二从机地址(主控板 IPMB 地址 0x10h,7bit 为 0x10) */
#define COM_I2C1_OAR2_ADDRESS						0x10

/* IPMB 主机在总线上的地址(STM32 OwnAddress1/OAR2 都要求预先左移好的 8bit 形式:
 * 真实 7bit 地址 << 1)。必须与 task_i2c.c 里 HOST_IPMB_ADDR_7BIT(0x10) << 1 = 0x20 保持一致——
 * 两处各自定义、无编译期联动,以后如改动 HOST_IPMB_ADDR_7BIT 需同步改这里。
 * I2C1/I2C2 主机模式下都用这个地址监听从机主动 write 回来的响应帧。 */
#define IPMB_HOST_I2C_ADDR							0x20

/* IPMB-A (I2C1) 角色定义 — 仅编译期切换,运行期不切换 */
#define IPMB_A_SLAVE          						0
#define IPMB_A_MASTER         						1

#ifndef IPMB_A_ROLE
#define IPMB_A_ROLE           						IPMB_A_MASTER
#endif

/* 自测串口选择(USART1,用于本地 IPMI 命令下发与响应回显) */
#define IPMB_TEST_COM         						COM_USART1
#define IPMB_TEST_TIMEOUT_MS  						50

#define BUFFER_TX_SIZE  256   //��Ӧ���صĳ���256
#define BUFFER_RX_SIZE  256  //������������ĳ���1256


/****************************I2C1_************************************/
//I2C1_SDA:PB7*******I2C1_SCL:PB6
#define COM_I2C1                          I2C1
#define COM_I2C1_CLK                      RCC_APB1Periph_I2C1

#define COM_I2C1_SCL_PIN                  GPIO_Pin_6                 
#define COM_I2C1_SCL_GPIO_PORT            GPIOB                       
#define COM_I2C1_SCL_GPIO_CLK             RCC_AHB1Periph_GPIOB
#define COM_I2C1_SCL_SOURCE               GPIO_PinSource6
#define COM_I2C1_SCL_AF                   GPIO_AF_I2C1

#define COM_I2C1_SDA_PIN                  GPIO_Pin_7                  
#define COM_I2C1_SDA_GPIO_PORT            GPIOB                       
#define COM_I2C1_SDA_GPIO_CLK             RCC_AHB1Periph_GPIOB
#define COM_I2C1_SDA_SOURCE               GPIO_PinSource7
#define COM_I2C1_SDA_AF                   GPIO_AF_I2C1

/****************************I2C2************************************/
//I2C2_SDA:PF0*******I2C2_SCL:PF1
#define COM_I2C2                          I2C2
#define COM_I2C2_CLK                      RCC_APB1Periph_I2C2

#define COM_I2C2_SCL_PIN                  GPIO_Pin_1                 
#define COM_I2C2_SCL_GPIO_PORT            GPIOF                       
#define COM_I2C2_SCL_GPIO_CLK             RCC_AHB1Periph_GPIOF
#define COM_I2C2_SCL_SOURCE               GPIO_PinSource1
#define COM_I2C2_SCL_AF                   GPIO_AF_I2C2

#define COM_I2C2_SDA_PIN                  GPIO_Pin_0                  
#define COM_I2C2_SDA_GPIO_PORT            GPIOF                       
#define COM_I2C2_SDA_GPIO_CLK             RCC_AHB1Periph_GPIOF
#define COM_I2C2_SDA_SOURCE               GPIO_PinSource0
#define COM_I2C2_SDA_AF                   GPIO_AF_I2C2

/****************************I2C3************************************/
//I2C3_SDA:PF0*******I2C2_SCL:PF1
#define COM_I2C3                          I2C3
#define COM_I2C3_CLK                      RCC_APB1Periph_I2C3

#define COM_I2C3_SCL_PIN                  GPIO_Pin_8                 
#define COM_I2C3_SCL_GPIO_PORT            GPIOA                       
#define COM_I2C3_SCL_GPIO_CLK             RCC_AHB1Periph_GPIOA
#define COM_I2C3_SCL_SOURCE               GPIO_PinSource8
#define COM_I2C3_SCL_AF                   GPIO_AF_I2C3

#define COM_I2C3_SDA_PIN                  GPIO_Pin_9                  
#define COM_I2C3_SDA_GPIO_PORT            GPIOC                       
#define COM_I2C3_SDA_GPIO_CLK             RCC_AHB1Periph_GPIOC
#define COM_I2C3_SDA_SOURCE               GPIO_PinSource9
#define COM_I2C3_SDA_AF                   GPIO_AF_I2C3


#pragma pack(1)
typedef struct
{
	unsigned char  recv_buf[BUFFER_RX_SIZE];
	unsigned short recv_data_len;

}stIIC_RECV_DATA_t,*pstIIC_RECV_DATA_t;

#pragma pack(1)
typedef struct
{
	unsigned char  send_buf[BUFFER_TX_SIZE];
	unsigned short send_data_len;
	unsigned short send_data_cnt;

}stIIC_SEND_DATA_t,*pstIIC_SEND_DATA_t;

extern stIIC_SEND_DATA_t i2c1_send_data_struct;
extern stIIC_RECV_DATA_t i2c1_recv_data_struct;
extern stIIC_SEND_DATA_t i2c2_send_data_struct;
extern stIIC_RECV_DATA_t i2c2_recv_data_struct;

void Bsp_I2C_Init(void);
void i2c_send_nvic(I2C_TypeDef* I2Cx,uint8_t slave_addr);
void i2c_recv_callback(I2C_TypeDef* I2Cx);
void Board_BPD20550_current(char *outbuf);

#if (IPMB_A_ROLE == IPMB_A_SLAVE)
/* 丛机模式:IPMB-A 作为从机响应 IPMI 请求 */
void I2C1_EV_IRQHandler_Slave(void);
void I2C1_SlaveSendResponse(const uint8_t *buf, uint8_t len);
#else
/* 主机模式:IPMB-A 作为主机发起 IPMI 请求 */
void I2C1_EV_IRQHandler_Master(void);
/* budget_ms: 等从机主动回写响应的总预算,由调用方按请求来源分级(见 task_i2c.h
 * 的 IPMB_BUDGET_*_MS)。传 0 沿用 300ms 默认值。 */
void I2C1_SendRequest(const uint8_t *buf, uint8_t len, uint8_t rx_expect_len, uint8_t target_addr, uint16_t budget_ms);
#endif
void I2C2_SendRequest(const uint8_t *buf, uint8_t len, uint8_t rx_expect_len, uint8_t target_addr, uint16_t budget_ms);
void I2C2_EV_IRQHandler_Master(void);



#endif
