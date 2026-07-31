#include "LY1210.h"
/*																						
*********************************************************************************************************
*	函 数 名: ETH_NRST_PIN_LOW
*	功能说明: 以太网复位低
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void ETH_NRST_PIN_LOW(void)
{
	 GPIO_ResetBits(GPIOD,GPIO_Pin_4);
}
/*																						
*********************************************************************************************************
*	函 数 名: ETH_NRST_PIN_HIGH
*	功能说明: 以太网复位高
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/

void ETH_NRST_PIN_HIGH(void)
{
	 GPIO_SetBits(GPIOD,GPIO_Pin_4);
}

/*																						
*********************************************************************************************************
*	函 数 名: ETH_GPIO_Config
*	功能说明: 以太网GPIO配置
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void ETH_GPIO_Config(void)
{
  GPIO_InitTypeDef GPIO_InitStructure;
  
  /* Enable GPIOs clocks */
  RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOA | RCC_AHB1Periph_GPIOB |
                         RCC_AHB1Periph_GPIOC | RCC_AHB1Periph_GPIOD |
                         RCC_AHB1Periph_GPIOG | RCC_AHB1Periph_GPIOH |
                         RCC_AHB1Periph_GPIOF | RCC_AHB1Periph_GPIOI, ENABLE);

  /* Enable SYSCFG clock */
  RCC_APB2PeriphClockCmd(RCC_APB2Periph_SYSCFG, ENABLE);  


  /* MII/RMII Media interface selection --------------------------------------*/
	//这个地方注意：如果是MII的接法，可以利用STM32的MCO脚把25MHz的时钟供给PHY，因为我们用的是LAN8742A，是RMII的接法，需要50MHZ的时钟，所以不能用这种接法
#ifdef MII_MODE /* Mode MII with STM324xx-EVAL  */

  SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_MII);
/* Ethernet pins configuration ************************************************/																									
//for MII
/*  	  ETH_MDIO -------------------------> PA2
        ETH_MDC --------------------------> PC1
				ETH_MII_CRS-----------------------> PA0
				ETH_MII_RX_CLK--------------------> PA1
				ETH_MII_COL-----------------------> PA3
				ETH_MII_RXDV----------------------> PA7
				ETH_MII_RXD2----------------------> PB0
				ETH_MII_RXD3----------------------> PB1
				ETH_MII_TXD3----------------------> PB8
				ETH_MII_RX_ER---------------------> PB10
				ETH_MII_TXD2----------------------> PC2
				ETH_MII_TX_CLK--------------------> PC3
				ETH_MII_RXD0----------------------> PC4
				ETH_MII_RXD1----------------------> PC5
				ETH_MII_TXD0----------------------> PG13
				ETH_MII_TXD1----------------------> PG14
				ETH_MII_TX_EN---------------------> PG11
*/

		GPIO_InitStructure.GPIO_Pin =  	GPIO_Pin_0|GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_3|GPIO_Pin_7;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
		GPIO_InitStructure.GPIO_Mode =  GPIO_Mode_AF;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
		GPIO_InitStructure.GPIO_PuPd =  GPIO_PuPd_NOPULL ;  
		GPIO_Init(GPIOA, &GPIO_InitStructure);
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource0, GPIO_AF_ETH); //引脚复用到网络接口上
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_ETH);
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_ETH);
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource3, GPIO_AF_ETH);
		GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_ETH);
		
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0 | GPIO_Pin_1 | GPIO_Pin_8 | GPIO_Pin_10;
		GPIO_Init(GPIOB, &GPIO_InitStructure);
		GPIO_PinAFConfig(GPIOB, GPIO_PinSource0, GPIO_AF_ETH); 
		GPIO_PinAFConfig(GPIOB, GPIO_PinSource1, GPIO_AF_ETH);
		GPIO_PinAFConfig(GPIOB, GPIO_PinSource8, GPIO_AF_ETH);
		GPIO_PinAFConfig(GPIOB, GPIO_PinSource10, GPIO_AF_ETH);
		
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_2 | GPIO_Pin_3 | GPIO_Pin_4 | GPIO_Pin_5;
		GPIO_Init(GPIOC, &GPIO_InitStructure);
		GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF_ETH); 
		GPIO_PinAFConfig(GPIOC, GPIO_PinSource2, GPIO_AF_ETH); //引脚复用到网络接口上
		GPIO_PinAFConfig(GPIOC, GPIO_PinSource3, GPIO_AF_ETH); 
		GPIO_PinAFConfig(GPIOC, GPIO_PinSource4, GPIO_AF_ETH);
		GPIO_PinAFConfig(GPIOC, GPIO_PinSource5, GPIO_AF_ETH);
		
		GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_14;
		GPIO_Init(GPIOG, &GPIO_InitStructure);
		GPIO_PinAFConfig(GPIOG, GPIO_PinSource11, GPIO_AF_ETH);
		GPIO_PinAFConfig(GPIOG, GPIO_PinSource13, GPIO_AF_ETH);
		GPIO_PinAFConfig(GPIOG, GPIO_PinSource14, GPIO_AF_ETH);
		
			//配置PD5为推完输出
		GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
		GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
		GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
		GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;	//推完输出
		GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;  
		GPIO_Init(GPIOD, &GPIO_InitStructure);
		
			//复位LY1210
		ETH_NRST_PIN_LOW();
		_eth_delay_(LY1210_RESET_DELAY);
		ETH_NRST_PIN_HIGH();
	
#elif defined RMII_MODE  /* Mode RMII with STM324xx-EVAL */

  SYSCFG_ETH_MediaInterfaceConfig(SYSCFG_ETH_MediaInterface_RMII);
/* Ethernet pins configuration ************************************************/																									
//for RMII
/*	  	ETH_MDIO -------------------------> PA2
        ETH_MDC --------------------------> PC1
        ETH_MII_RX_CLK/ETH_RMII_REF_CLK---> PA1
        ETH_MII_RX_DV/ETH_RMII_CRS_DV ----> PA7			
        ETH_MII_RXD0/ETH_RMII_RXD0 -------> PC4
        ETH_MII_RXD1/ETH_RMII_RXD1 -------> PC5			
        ETH_MII_TX_EN/ETH_RMII_TX_EN -----> PG11
        ETH_MII_TXD0/ETH_RMII_TXD0 -------> PG13
        ETH_MII_TXD1/ETH_RMII_TXD1 -------> PG14
				ETH_RESET                  -------> PD4		 */
																									
																									
 //配置PA1 PA2 PA7
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1|GPIO_Pin_2|GPIO_Pin_7;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;  
	GPIO_Init(GPIOA, &GPIO_InitStructure);
	
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource1, GPIO_AF_ETH); //引脚复用到网络接口上
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource2, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOA, GPIO_PinSource7, GPIO_AF_ETH);

	//配置PC1,PC4 and PC5
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1 | GPIO_Pin_4 | GPIO_Pin_5;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource1, GPIO_AF_ETH); //引脚复用到网络接口上
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource4, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOC, GPIO_PinSource5, GPIO_AF_ETH);
                                
	//配置PG11, PG14 and PG13 
	GPIO_InitStructure.GPIO_Pin =  GPIO_Pin_11 | GPIO_Pin_13 | GPIO_Pin_14;
	GPIO_Init(GPIOG, &GPIO_InitStructure);
	GPIO_PinAFConfig(GPIOG, GPIO_PinSource11, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOG, GPIO_PinSource13, GPIO_AF_ETH);
	GPIO_PinAFConfig(GPIOG, GPIO_PinSource14, GPIO_AF_ETH);
	
	//配置PD4为推完输出
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_100MHz;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_OUT;
	GPIO_InitStructure.GPIO_OType = GPIO_OType_PP;	//推完输出
	GPIO_InitStructure.GPIO_PuPd = GPIO_PuPd_NOPULL ;
	GPIO_Init(GPIOD, &GPIO_InitStructure);

	//复位LY1210
	ETH_NRST_PIN_LOW();
	_eth_delay_(LY1210_RESET_DELAY);
	ETH_NRST_PIN_HIGH();
#endif

	

}




