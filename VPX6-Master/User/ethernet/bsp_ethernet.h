#ifndef __BSP_ETHERNET_H
#define __BSP_ETHERNET_H

#ifdef __cplusplus
 extern "C" {
#endif
#include "stdint.h"
#include "netif.h"
#include "netconf.h"
#include "lwip/dhcp.h"

/* Scheduler includes */
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"


/* The time to block waiting for input. */
#define ETH_LINK_TASK_STACK_SIZE		( configMINIMAL_STACK_SIZE * 2 )
#define ETH_LINK_TASK_PRIORITY		        ( tskIDLE_PRIORITY + 3 )
#define emacBLOCK_TIME_WAITING_ETH_LINK_IT	( ( portTickType ) 100 )

/* PHY芯片选择总开关: 1 = 量产板LY1210A-IR-QFN32(MII), 0 = 测试板LAN8720A-CP-TR-ABC(RMII) */
#define BOARD_USE_LY1210A      0

#if BOARD_USE_LY1210A
  #define ETH_PHY_ADDRESS   0x03    /* LY1210A板载地址, 已验证 */
  #define MII_MODE
#else
  #define ETH_PHY_ADDRESS   0x00    /* LAN8720A占位地址, 以启动时串口打印的PHY扫描结果为准 */
  #define RMII_MODE  // LAN8720A仅支持RMII; 板上Y6是25MHz晶振接在LAN8720A的XTAL1/XTAL2, 芯片内部倍频后由REFCLKO脚(NINT/REFCLKO)把50MHz参考时钟送给MCU的PA1, 不需要MCU侧另外提供时钟
#endif


/* Ethernet Flags for EthStatus variable */   
#define ETH_INIT_FLAG           0x01 /* Ethernet Init Flag */
#define ETH_LINK_FLAG           0x10 /* Ethernet Link Flag */


extern struct netif xnetif;

void 	Ethernet_init(void);
void  ETH_BSP_Config(void);
void Eth_Link_EXTIConfig(void);
void Eth_Link_ITHandler(uint16_t PHYAddress);
void Eth_Link_IT_task( void * pvParameters );
void vEthLinkChkTask( void * pvParameters );
void ETH_link_callback(struct netif *netif);
//�޲���ϵͳ��ʱ��ѭ������
void Eth_Link_ITLoop(uint16_t PHYAddress);
void vEthLinkChkTask( void * pvParameters);

#ifdef __cplusplus
}
#endif

#endif /* __STM32F4x7_ETH_BSP_H */


/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
