#include "lwip/opt.h"
#include "stm32f4x7_eth.h"
#include "bsp_ethernet.h"
#include "bsp_init.h"
#include "bsp_httpd.h"

ETH_InitTypeDef ETH_InitStructure;
__IO uint32_t  EthStatus = 0;
#ifdef USE_DHCP
extern __IO uint8_t DHCP_state;
#endif /* LWIP_DHCP */
xSemaphoreHandle ETH_link_xSemaphore = NULL;

void ETH_GPIO_Config(void);
static void ETH_NVIC_Config(void);
static void ETH_MACDMA_Config(void);

/*																						
*********************************************************************************************************
*	函 数 名: Ethernet_init
*	功能说明: 网络初始化，包含配置GPIO,配置DMA寄存器，初始化LWIP，创建UDP连接
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void 	Ethernet_init(void)
{
	  /* configure ethernet (GPIOs, clocks, MAC, DMA) */ 
	printf(">>开始以太网模块配置......\n");
  ETH_BSP_Config();
	if((EthStatus&(0x01)) == 0)
	{
		printf(">>配置失败，请检查网线是否插好---ERR\n");
	}
	else
	{
		printf(">>配置以太网成功...OK\n");
	}
  
  /* Initilaize the LwIP stack */
  LwIP_Init();
	delay_1ms(50);
	printf(">>初始化LWIP协议栈成功...OK\n");
	  //IP地址和端口可在netconf.h文件修改，或者使用DHCP服务自动获取IP(需要路由器支持)
  printf("本地 IP: %d.%d.%d.%d    本地端口:%d\n",IP_ADDR0,IP_ADDR1,IP_ADDR2,IP_ADDR3,UDP_SERVER_PORT);
	printf("远程 IP: %d.%d.%d.%d    远程端口:%d\n",DEST_IP_ADDR0,DEST_IP_ADDR1,DEST_IP_ADDR2,DEST_IP_ADDR3,DEST_PORT);
	
#if ENABLE_UDP_TEST_TRAFFIC
	CreateUDPConnect(DEST_IP_ADDR0,DEST_IP_ADDR1,DEST_IP_ADDR2,DEST_IP_ADDR3,DEST_PORT);//远端的IP和端口号
#endif

	httpd_init();//启动网页控制台的HTTP服务(80端口)
}

/*																						
*********************************************************************************************************
*	函 数 名: ETH_BSP_Config
*	功能说明: 网络初始化
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void ETH_BSP_Config(void)
{
  
  ETH_GPIO_Config();																										//网络GPIO配置

  ETH_MACDMA_Config();  																								// 配置 Ethernet MAC/DMA

  if(ETH_ReadPHYRegister(ETH_PHY_ADDRESS, PHY_BSR) & 0x00000004)   //获取网络链接LINK状态
  {
    EthStatus |= ETH_LINK_FLAG;
  }


}

/*																						
*********************************************************************************************************
*	函 数 名: ETH_MACDMA_Config
*	功能说明: 以太网DMA配置
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
static void ETH_MACDMA_Config(void)
{ 
  /* Enable ETHERNET clock  */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_ETH_MAC | RCC_AHB1Periph_ETH_MAC_Tx |
                         RCC_AHB1Periph_ETH_MAC_Rx, ENABLE);

  /* Reset ETHERNET on AHB Bus */
  ETH_DeInit();

  /* Software reset */
  ETH_SoftwareReset();

  /* Wait for software reset —— 加超时，避免 PHY 无时钟时整机死循环 */
  {
    __IO uint32_t sw_reset_to = 0;
    while ((ETH_GetSoftwareResetStatus() == SET) && (sw_reset_to < (uint32_t)0x0004FFFF))
    {
      sw_reset_to++;
    }
    if (sw_reset_to >= (uint32_t)0x0004FFFF)
    {
      printf(">>[ETH]DMA软复位超时: MAC未收到PHY的RX_CLK/TX_CLK, 查PHY供电/复位(PD4)/25M晶振---ERR\r\n");
    }
    else
    {
      printf(">>[ETH]DMA软复位完成(说明PHY时钟已到MAC)...OK\r\n");
    }
  }

  /* ETHERNET Configuration --------------------------------------------------*/
  /* Call ETH_StructInit if you don't like to configure all ETH_InitStructure parameter */
  ETH_StructInit(&ETH_InitStructure);

  /* Fill ETH_InitStructure parametrs */
  /*------------------------   MAC   -----------------------------------*/
  ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Enable;
//  ETH_InitStructure.ETH_AutoNegotiation = ETH_AutoNegotiation_Disable; 
//  ETH_InitStructure.ETH_Speed = ETH_Speed_10M;
//  ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;

  ETH_InitStructure.ETH_LoopbackMode = ETH_LoopbackMode_Disable;
  ETH_InitStructure.ETH_RetryTransmission = ETH_RetryTransmission_Disable;
  ETH_InitStructure.ETH_AutomaticPadCRCStrip = ETH_AutomaticPadCRCStrip_Disable;
  ETH_InitStructure.ETH_ReceiveAll = ETH_ReceiveAll_Disable;
  ETH_InitStructure.ETH_BroadcastFramesReception = ETH_BroadcastFramesReception_Enable;
  ETH_InitStructure.ETH_PromiscuousMode = ETH_PromiscuousMode_Disable;
  ETH_InitStructure.ETH_MulticastFramesFilter = ETH_MulticastFramesFilter_Perfect;
  ETH_InitStructure.ETH_UnicastFramesFilter = ETH_UnicastFramesFilter_Perfect;
#ifdef CHECKSUM_BY_HARDWARE
  ETH_InitStructure.ETH_ChecksumOffload = ETH_ChecksumOffload_Enable;
#endif

  /*------------------------   DMA   -----------------------------------*/  
  
  /* When we use the Checksum offload feature, we need to enable the Store and Forward mode: 
  the store and forward guarantee that a whole frame is stored in the FIFO, so the MAC can insert/verify the checksum, 
  if the checksum is OK the DMA can handle the frame otherwise the frame is dropped */
  ETH_InitStructure.ETH_DropTCPIPChecksumErrorFrame = ETH_DropTCPIPChecksumErrorFrame_Enable;
  ETH_InitStructure.ETH_ReceiveStoreForward = ETH_ReceiveStoreForward_Enable;
  ETH_InitStructure.ETH_TransmitStoreForward = ETH_TransmitStoreForward_Enable;
 
  ETH_InitStructure.ETH_ForwardErrorFrames = ETH_ForwardErrorFrames_Disable;
  ETH_InitStructure.ETH_ForwardUndersizedGoodFrames = ETH_ForwardUndersizedGoodFrames_Disable;
  ETH_InitStructure.ETH_SecondFrameOperate = ETH_SecondFrameOperate_Enable;
  ETH_InitStructure.ETH_AddressAlignedBeats = ETH_AddressAlignedBeats_Enable;
  ETH_InitStructure.ETH_FixedBurst = ETH_FixedBurst_Enable;
  ETH_InitStructure.ETH_RxDMABurstLength = ETH_RxDMABurstLength_32Beat;
  ETH_InitStructure.ETH_TxDMABurstLength = ETH_TxDMABurstLength_32Beat;
  ETH_InitStructure.ETH_DMAArbitration = ETH_DMAArbitration_RoundRobin_RxTx_2_1;

  /* Configure Ethernet */
  EthStatus = ETH_Init(&ETH_InitStructure, ETH_PHY_ADDRESS);

  /* 诊断: 扫描 0~31 全部 PHY 地址, 读寄存器2(PHY ID1).
     MDIO只依赖MAC的MDC, 不依赖25M时钟. 只要PHY上电+复位释放+MDIO/MDC接线正常, 就该应答.
       - 某地址读到非0/非FFFF => 那就是真实PHY地址, 改 ETH_PHY_ADDRESS 即可;
       - 全部0xFFFF/0x0000  => 纯硬件: PHY没供电/复位(PD4)没释放/MDIO(PA2)-MDC(PC1)断线. */
  {
    uint8_t  addr;
    uint16_t id1, id2;
    int      found = -1;
    printf(">>[ETH]ETH_Init返回:0x%08X, 开始扫描PHY地址0~31...\r\n", (unsigned int)EthStatus);
    for (addr = 0; addr < 32; addr++)
    {
      id1 = ETH_ReadPHYRegister(addr, 2);
      if (id1 != 0x0000 && id1 != 0xFFFF)
      {
        id2 = ETH_ReadPHYRegister(addr, 3);
        printf(">>[ETH]  发现PHY @addr=%d  ID1=0x%04X ID2=0x%04X\r\n", addr, id1, id2);
        if (found < 0) found = addr;
      }
    }
    if (found < 0)
    {
      printf(">>[ETH]全部32个地址均无应答: 纯硬件问题, 查PHY供电/复位脚PD4是否拉高/MDIO(PA2)-MDC(PC1)走线---ERR\r\n");
    }
    else if (found != ETH_PHY_ADDRESS)
    {
      printf(">>[ETH]!! PHY真实地址=%d, 但代码里ETH_PHY_ADDRESS=%d, 请修改该宏 !!\r\n", found, ETH_PHY_ADDRESS);
    }
    else
    {
      printf(">>[ETH]PHY地址正确=%d, MDIO通信OK...OK\r\n", found);
    }
  }

  /* Enable the Ethernet Rx Interrupt */
  ETH_DMAITConfig(ETH_DMA_IT_NIS | ETH_DMA_IT_R, ENABLE);
}



/*																						
*********************************************************************************************************
*	函 数 名: Eth_Link_PHYITConfig
*	功能说明: 
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
uint32_t Eth_Link_PHYITConfig(uint16_t PHYAddress)
{
  uint16_t tmpreg = 0;

  /* Read MICR register */
  tmpreg = ETH_ReadPHYRegister(PHYAddress, PHY_MICR);

  /* Enable output interrupt events to signal via the INT pin */
  tmpreg |= (uint32_t)PHY_MICR_INT_EN | PHY_MICR_INT_OE;
  if(!(ETH_WritePHYRegister(PHYAddress, PHY_MICR, tmpreg)))
  {
    /* Return ERROR in case of write timeout */
    return ETH_ERROR;
  }

  /* Read MISR register */
  tmpreg = ETH_ReadPHYRegister(PHYAddress, PHY_MISR);

  /* Enable Interrupt on change of link status */
  tmpreg |= (uint32_t)PHY_MISR_LINK_INT_EN;
  if(!(ETH_WritePHYRegister(PHYAddress, PHY_MISR, tmpreg)))
  {
    /* Return ERROR in case of write timeout */
    return ETH_ERROR;
  }
  /* Return SUCCESS */
  return ETH_SUCCESS;
}




/*																						
*********************************************************************************************************
*	函 数 名: ETH_link_callback
*	功能说明: 
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void ETH_link_callback(struct netif *netif)
{
  __IO uint32_t timeout = 0;
 uint32_t tmpreg,RegValue;

  struct ip_addr ipaddr;
  struct ip_addr netmask;
  struct ip_addr gw;
#ifndef USE_DHCP
  uint8_t iptab[4] = {0};
  uint8_t iptxt[20];
#endif /* USE_DHCP */



  if(netif_is_link_up(netif))
  {
    /* Restart the autonegotiation */
    if(ETH_InitStructure.ETH_AutoNegotiation != ETH_AutoNegotiation_Disable)
    {
      /* Reset Timeout counter */
      timeout = 0;

      /* Enable Auto-Negotiation */
      ETH_WritePHYRegister(ETH_PHY_ADDRESS, PHY_BCR, PHY_AutoNegotiation);

      /* Wait until the auto-negotiation will be completed */
      do
      {
        timeout++;
      } while (!(ETH_ReadPHYRegister(ETH_PHY_ADDRESS, PHY_BSR) & PHY_AutoNego_Complete) && (timeout < (uint32_t)PHY_READ_TO));

      /* Reset Timeout counter */
      timeout = 0;

      /* Read the result of the auto-negotiation */
      RegValue = ETH_ReadPHYRegister(ETH_PHY_ADDRESS, PHY_SR);
    
      /* Configure the MAC with the Duplex Mode fixed by the auto-negotiation process */
      if((RegValue & PHY_DUPLEX_STATUS) != (uint32_t)RESET)
      {
        /* Set Ethernet duplex mode to Full-duplex following the auto-negotiation */
        ETH_InitStructure.ETH_Mode = ETH_Mode_FullDuplex;  
      }
      else
      {
        /* Set Ethernet duplex mode to Half-duplex following the auto-negotiation */
        ETH_InitStructure.ETH_Mode = ETH_Mode_HalfDuplex;
      }
      /* Configure the MAC with the speed fixed by the auto-negotiation process */
      if(RegValue & PHY_SPEED_STATUS)
      {
        /* Set Ethernet speed to 10M following the auto-negotiation */
        ETH_InitStructure.ETH_Speed = ETH_Speed_10M; 
      }
      else
      {
        /* Set Ethernet speed to 100M following the auto-negotiation */
        ETH_InitStructure.ETH_Speed = ETH_Speed_100M;      
      }

      /*------------------------ ETHERNET MACCR Re-Configuration --------------------*/
      /* Get the ETHERNET MACCR value */  
      tmpreg = ETH->MACCR;

      /* Set the FES bit according to ETH_Speed value */ 
      /* Set the DM bit according to ETH_Mode value */ 
      tmpreg |= (uint32_t)(ETH_InitStructure.ETH_Speed | ETH_InitStructure.ETH_Mode);

      /* Write to ETHERNET MACCR */
      ETH->MACCR = (uint32_t)tmpreg;

      _eth_delay_(ETH_REG_WRITE_DELAY);
      tmpreg = ETH->MACCR;
      ETH->MACCR = tmpreg;
    }

    /* Restart MAC interface */
    ETH_Start();

#ifdef USE_DHCP
    ipaddr.addr = 0;
    netmask.addr = 0;
    gw.addr = 0;


    DHCP_state = DHCP_START;
#else
    IP4_ADDR(&ipaddr, IP_ADDR0, IP_ADDR1, IP_ADDR2, IP_ADDR3);
    IP4_ADDR(&netmask, NETMASK_ADDR0, NETMASK_ADDR1 , NETMASK_ADDR2, NETMASK_ADDR3);
    IP4_ADDR(&gw, GW_ADDR0, GW_ADDR1, GW_ADDR2, GW_ADDR3);
#endif /* USE_DHCP */

    netif_set_addr(&xnetif, &ipaddr , &netmask, &gw);
    
    /* When the netif is fully configured this function must be called.*/
    netif_set_up(&xnetif);    


  }
  else
  {
    ETH_Stop();
#ifdef USE_DHCP
    DHCP_state = DHCP_LINK_DOWN;
    dhcp_stop(netif);

#endif /* USE_DHCP */

    /*  When the netif link is down this function must be called.*/
    netif_set_down(&xnetif);

  }
}

