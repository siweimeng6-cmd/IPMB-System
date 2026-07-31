#include "bsp_init.h"
#include "task_ethernet.h"

TaskHandle_t task_enternet_Handle;
TaskHandle_t Ethernet_receive_Task_Handle;

uint8_t send_eth_data[256]={0};
uint16_t read_udp_length=0;
uint8_t link_success = 0;
uint8_t link_down_cnt=0;

/*																						
*********************************************************************************************************
*	函 数 名: task_enternet_entry
*	功能说明: 网络任务，数据处理
*	形    参：void * pvParameters
*	返 回 值: 无
*********************************************************************************************************
*/
void task_enternet_entry(void * pvParameters)
{
	uint8_t* p_data;
	uint32_t sendNum=256;
	uint16_t i,len,cnt=0;
	uint8_t read_buf[256]={0};
	char ethernet_send_char[20]="ETHERNET SEND DATA\r\n";
	
	for(i=0;i<256;i++)
		send_eth_data[i]=i;
	/* loop */
	while(1)
	{
		if(link_success == 1)
		{
				if(s_structUDPDev.state == ES_UDP_CONNECTED){	//	UDP链接成功
				cnt++;
					if(cnt == 20){
						p_data = (uint8_t*) ethernet_send_char;
						WriteUDP(p_data,20);
						p_data = (uint8_t*) send_eth_data;
						len= WriteUDP(p_data,sendNum);
						printf("send udp,length=%d\r\n",len);
						cnt = 0;
					}
			}//end if(s_structUDPDev.state == ES_UDP_CONNECTED){
		}

		vTaskDelay(200);
	}
}
/*																						
*********************************************************************************************************
*	函 数 名: Ethernet_receive_Task
*	功能说明: 网络接收数据处理
						从队列里接收的数据uint16的长度和uint32的地址
						需要从地址中再取出数据
*	形    参：void * pvParameters
*	返 回 值: 无
*********************************************************************************************************
*/
void Ethernet_receive_Task(void * pvParameters)
{	
	uint16_t i,len,cnt=0;
	uint8_t * p=0;
	stEVENT_t ethernet_rx_evt;
	while(1)
	{
		if(link_success == 1)
		{
				#if DONT_USE_FREERTOS_QUEUE_RECEIVE_ETHERNET			
								if(read_udp_length!=0){	//udp回调函数有数据接收
									ReadUDP(read_buf, read_udp_length);
									printf("\r\n UDP receive data: ");
									for(i=0;i<read_udp_length;i++)
									printf("%d  ",read_buf[i]);
									printf("\r\n UDP receive length: %d \r\n",read_udp_length);
									read_udp_length = 0;//接收长度清0，避免重复进入
								}
			#else
							if( xQueueReceive( queue_ethernet_Handle, &ethernet_rx_evt, portMAX_DELAY ) == pdPASS )
							{
								printf("\r\n UDP receive data： ");
								p = (uint8_t *)ethernet_rx_evt.data;	//将uint32位地址强制转换为指针类
								for(i=0;i<ethernet_rx_evt.data_len;i++)
									printf("%c  ",*(p+i));
								printf("\r\n UDP receive length：%d \r\n",ethernet_rx_evt.data_len);
							}
							
			#endif
		}
	 }
}
/*																						
*********************************************************************************************************
*	函 数 名: Ethernet_LinkCheckTask
*	功能说明: 网络link状态轮询查询
*	形    参：void * pvParameters
*	返 回 值: 无
*********************************************************************************************************
*/
void Ethernet_LinkCheckTask( void * pvParameters )
{
	uint32_t  link_state;
	while(1)
	{
		//读出链接状态
		link_state = (ETH_ReadPHYRegister(ETH_PHY_ADDRESS, PHY_BSR) & 0x00000004);

		if(link_state && (!link_success))				//发现网线插上，并且前面没有初始化好MAC，就需要重新配置MAC
		{
			 printf(">>网线连接正常，正在配置网卡...\n");
			 netif_set_link_up(&xnetif);
			 printf(">>网卡配置成功...OK\n");
			 link_success = 1;
				link_down_cnt=0;

		}
		else if((!link_state) && link_success)	//如果网线被拔掉，并且已经配置好了，就需要删除网卡
		{
			 printf(">>网线被拔出，请检查...\n");
			 netif_set_link_down(&xnetif); 
			 link_success = 0;
		}
		else if((!link_state) && (!link_success))
		{
			 link_down_cnt++;
			 if(link_down_cnt==5)
			 {
				 printf(">>网线被拔出，请检查...\n");
				 link_down_cnt=0;
			 }
		}
		//延时1000ms
		//vTaskDelay(1000/ portTICK_RATE_MS);
		vTaskDelay(2000);

	}
}