#include "bsp_init.h"
#include "task_i2c.h"

extern void vPortSetupTimerInterrupt( void );


/*																						
*********************************************************************************************************
*	?? ?? ??: task_create
*	???????: ????????????
*	??    ?Σ???
*	?? ?? ?: ??
*********************************************************************************************************
*/
void task_create(void)
{
		BaseType_t xReturn = pdPASS;
		xGpioCmdQueue = xQueueCreate(4, sizeof(GPIO_Cmd_t));
		if(xGpioCmdQueue == NULL)
			printf("[Err]:create xGpioCmdQueue fail\r\n");

		xReturn = xTaskCreate((TaskFunction_t )GPIO_Task,
													(const char*    )"GPIO_Task",
													(uint16_t       )128,
													(void*          )NULL,
													(UBaseType_t    )2,
													(TaskHandle_t*  )&GPIO_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create GPIO_Task success!\r\n");
		else
			printf("Create GPIO_Task failed! \r\n");/* ?????????????????????????pdPASS */	
		/* ????USART1_Task???? */
		xReturn = xTaskCreate((TaskFunction_t )USART1_Task, /* ?????????? */
													(const char*    )"USART1_Task",			/* ???????? */
													(uint16_t       )256,  			 		/* ???????С */
													(void*          )NULL,					/* ?????????????? */
													(UBaseType_t    )2,	    				/* ?????????? */
													(TaskHandle_t*  )&USART1_Task_Handle);	/* ??????????? */
		if(pdPASS == xReturn)
			printf("Create USART1_Task success!\r\n");
		else
			printf("Create USART1_Task failed! \r\n");
		
#if USART2_EN	
		xReturn = xTaskCreate((TaskFunction_t )USART2_Task, 								/* ?????????? */
													(const char*    )"USART2_Task",			/* ???????? */
													(uint16_t       )256,   				/* ???????С */
													(void*          )NULL,					/* ?????????????? */
													(UBaseType_t    )2,	   		 			/* ?????????? */
													(TaskHandle_t*  )&USART2_Task_Handle);	/* ??????????? */
		if(pdPASS == xReturn)
			printf("Create USART2_Task success!\r\n");
		else
			printf("Create USART2_Task failed! \r\n");
#endif
#if USART3_EN	
		xReturn = xTaskCreate((TaskFunction_t )USART3_Task, 								/* ?????????? */
													(const char*    )"USART3_Task",			/* ???????? */
													(uint16_t       )256,   				/* ???????С */
													(void*          )NULL,					/* ?????????????? */
													(UBaseType_t    )2,	    				/* ?????????? */
													(TaskHandle_t*  )&USART3_Task_Handle);	/* ??????????? */
		if(pdPASS == xReturn)
			printf("Create USART3_Task success!\r\n");
		else
			printf("Create USART3_Task failed! \r\n");
#endif		
#if USART4_EN	
		xReturn = xTaskCreate((TaskFunction_t )USART4_Task, /* ?????????? */
													(const char*    )"USART4_Task",/* ???????? */
													(uint16_t       )256,   /* ???????С */
													(void*          )NULL,	/* ?????????????? */
													(UBaseType_t    )2,	    /* ?????????? */
													(TaskHandle_t*  )&USART4_Task_Handle);/* ??????????? */
		if(pdPASS == xReturn)
			printf("Create USART4_Task success!\r\n");
		else
			printf("Create USART4_Task failed! \r\n");
#endif
		

		xReturn = xTaskCreate((TaskFunction_t )USART5_Task,
													(const char*    )"USART5_Task",
													(uint16_t       )768,   /* "?"查询要在栈上放 status_buf[900]+line_buf[400]+bpd_buf[256],
													                            原来 256 字(1024字节)不够,会栈溢出踩坏别的任务 */
													(void*          )NULL,
													(UBaseType_t    )2,
													(TaskHandle_t*  )&USART5_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create USART5_Task success!\r\n");
		else
			printf("Create USART5_Task failed! \r\n");

			

		xReturn = xTaskCreate((TaskFunction_t )USART6_Task, 
													(const char*    )"USART6_Task",
													(uint16_t       )256,   
													(void*          )NULL,	
													(UBaseType_t    )2,	    
													(TaskHandle_t*  )&USART6_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create USART6_Task success!\r\n");
		else
			printf("Create USART6_Task failed! \r\n");
		

		xReturn = xTaskCreate((TaskFunction_t )I2C1_Task, 
													(const char*    )"I2C1_Task",
													(uint16_t       )512,  
													(void*          )NULL,	
													(UBaseType_t    )2,	   
													(TaskHandle_t*  )&I2C1_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create I2C1_Task success!\r\n");
		else
			printf("Create I2C1_Task failed! \r\n");
		
	
		/* I2C1_Task 与 I2C2_Task 现在各用独立命令队列(xIPMB_CmdQueue /
		 * xIPMB_CmdQueue2),同一条 ipmb 命令双入队,两路主机(I2C1→F103,
		 * I2C2→第二块同类从机)同时下发。不再抢队列,故重新启用 I2C2_Task。 */
		xReturn = xTaskCreate((TaskFunction_t )I2C2_Task,
													(const char*    )"I2C2_Task",
													(uint16_t       )512,
													(void*          )NULL,
													(UBaseType_t    )2,
													(TaskHandle_t*  )&I2C2_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create I2C2_Task success!\r\n");
		else
			printf("Create I2C2_Task failed! \r\n");



		xReturn = xTaskCreate((TaskFunction_t )IPMB_Test_Command_Task,
													(const char*    )"IPMB_Test_Cmd",
													(uint16_t       )256,
													(void*          )NULL,
													(UBaseType_t    )2,
													(TaskHandle_t*  )&IPMB_Test_Cmd_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create IPMB_Test_Command_Task success!\r\n");
		else
			printf("Create IPMB_Test_Command_Task failed!\r\n");

		/* 从机地址由背板槽位动态决定, 后台周期扫描发现当前从机实际地址 */
		xReturn = xTaskCreate((TaskFunction_t )IPMB_Discovery_Task,
													(const char*    )"IPMB_Discovery",
													(uint16_t       )256,
													(void*          )NULL,
													(UBaseType_t    )2,
													(TaskHandle_t*  )&IPMB_Discovery_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create IPMB_Discovery_Task success!\r\n");
		else
			printf("Create IPMB_Discovery_Task failed!\r\n");

		/* 每5秒自动轮询一次 Platform Event Message,取走F103内部排队的PEM事件 */
		xReturn = xTaskCreate((TaskFunction_t )IPMB_PEM_Poll_Task,
													(const char*    )"IPMB_PEM_Poll",
													(uint16_t       )256,
													(void*          )NULL,
													(UBaseType_t    )2,
													(TaskHandle_t*  )&IPMB_PEM_Poll_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create IPMB_PEM_Poll_Task success!\r\n");
		else
			printf("Create IPMB_PEM_Poll_Task failed!\r\n");

		/* 网页控制台多槽位轮询(2026-07-27替换原单从机 Web_Sensor_Poll_Task):每1秒把
		 * 发现表里的槽位全查一遍(2026-08-20 从"每3秒轮转下一个"改成全扫描,让全槽位
		 * 刷新周期压进网页 3s 的拉取间隔),拉取 Device ID/Slot/Board Status/PEM/7个
		 * 传感器,结果存进 task_slot_cache.c 的 g_slot_cache[],供 bsp_httpd.c JSON API
		 * 直接读取 */
		xReturn = xTaskCreate((TaskFunction_t )IPMB_Slot_Poll_Task,
													(const char*    )"IPMB_Slot_Poll",
													(uint16_t       )256,
													(void*          )NULL,
													(UBaseType_t    )2,
													(TaskHandle_t*  )&IPMB_Slot_Poll_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create IPMB_Slot_Poll_Task success!\r\n");
		else
			printf("Create IPMB_Slot_Poll_Task failed!\r\n");

		/* 网页控制台异步控制通道(2026-07-27新增):消费 xIPMB_CtrlReqQueue,
		 * 完成 POST /control 的真正 IPMB 往返,结果供 GET /tickets/{n} 轮询 */
		xReturn = xTaskCreate((TaskFunction_t )IPMB_Ctrl_Dispatch_Task,
													(const char*    )"IPMB_Ctrl_Dispatch",
													(uint16_t       )256,
													(void*          )NULL,
													(UBaseType_t    )2,
													(TaskHandle_t*  )&IPMB_Ctrl_Dispatch_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create IPMB_Ctrl_Dispatch_Task success!\r\n");
		else
			printf("Create IPMB_Ctrl_Dispatch_Task failed!\r\n");

//		/* 风扇控制+状态打印任务:占空比改由串口 fan <0-100> 命令设置(不再按温度),
//		 * 4 路共用一个占空比;转速反馈([TACH IN] RPM_1..4)照常周期打印 */
		xReturn = xTaskCreate((TaskFunction_t )Temp_Task,
													(const char*    )"Temp_Task",
													(uint16_t       )512,
													(void*          )NULL,
													(UBaseType_t    )2,   
													(TaskHandle_t*  )&Temp_Task_Handle);
		if(pdPASS == xReturn)
			printf("Create Temp_Task success!\r\n");
		else
			printf("Create Temp_Task failed! \r\n");
//		
				/* ????I2C3_Task???? */
		xReturn = xTaskCreate((TaskFunction_t )PWM_Task, /* ?????????? */
													(const char*    )"PWM_Task",/* ???????? */
													(uint16_t       )256,   /* ???????С */
													(void*          )NULL,	/* ?????????????? */
													(UBaseType_t    )2,	    /* ?????????? */
													(TaskHandle_t*  )&PWM_Task_Handle);/* ??????????? */
		if(pdPASS == xReturn)
			printf("Create PWM_Task success!\r\n");
		else
			printf("Create PWM_Task failed! \r\n");
		
#if ENABLE_UDP_TEST_TRAFFIC
		xReturn =	xTaskCreate((TaskFunction_t )task_enternet_entry,
												(const char *    )"enternet",
												(uint16_t       )256 ,
												(void*          )NULL,
												(UBaseType_t    )2,
												(TaskHandle_t*  )&task_enternet_Handle );
		if(pdPASS == xReturn)
			printf("Create task_enternet_entry success!\r\n");
		else
			printf("Create task_enternet_entry failed! \r\n");


		xReturn = xTaskCreate((TaskFunction_t )Ethernet_receive_Task, /* ?????????? */
													(const char*    )"Ethernet_receive_Task",/* ???????? */
													(uint16_t       )512,   /* ???????С */
													(void*          )NULL,	/* ?????????????? */
													(UBaseType_t    )2,	    /* ?????????? */
													(TaskHandle_t*  )&Ethernet_receive_Task_Handle);/* ??????????? */
		if(pdPASS == xReturn)
			printf("Create Ethernet_receive_Task success!\r\n");
		else
			printf("Create Ethernet_receive_Task failed! \r\n");
#else
		printf(">>[MEM] ENABLE_UDP_TEST_TRAFFIC=0, skip task_enternet_entry/Ethernet_receive_Task (not related to IPMB/httpd)\r\n");
#endif

	  /* create the task that handles the ETH_link */
		xReturn = xTaskCreate((TaskFunction_t )Ethernet_LinkCheckTask, 
												(const char*    ) "E_link",
												(uint16_t       )ETH_LINK_TASK_STACK_SIZE, 
												(void *         )ETH_PHY_ADDRESS,
												(UBaseType_t    )ETH_LINK_TASK_PRIORITY,
												NULL);
		if(pdPASS == xReturn)
			printf("Create Ethernet_LinkCheckTask success!\r\n");
		else
			printf("Create Ethernet_LinkCheckTask failed! \r\n");
		
}


static void NVIC_Configuration(void)
{
  NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );			
  
  NVIC_InitStructure.NVIC_IRQChannel = COM_USART1_IRQ;  		
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; 
  NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;  			
  NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;  					
  NVIC_Init(&NVIC_InitStructure);  													
#if USART2_EN	
	NVIC_InitStructure.NVIC_IRQChannel = COM_USART2_IRQ;  		// ????USART??ж?? 
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; // ??????????1 
  NVIC_Init(&NVIC_InitStructure); 
#endif	
#if USART3_EN	
	NVIC_InitStructure.NVIC_IRQChannel = COM_USART3_IRQ;  		// ????USART??ж?? 
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; // ??????????1 
  NVIC_Init(&NVIC_InitStructure); 
#endif
#if USART4_EN	
	NVIC_InitStructure.NVIC_IRQChannel = COM_USART4_IRQ;  		// ????USART??ж?? 
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; // ??????????1 
  NVIC_Init(&NVIC_InitStructure); 
#endif	
	NVIC_InitStructure.NVIC_IRQChannel = COM_USART5_IRQ;  		// ????USART??ж?? 
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; // ??????????1 
  NVIC_Init(&NVIC_InitStructure);
	
  NVIC_InitStructure.NVIC_IRQChannel = COM_USART6_IRQ;  		// ????USART??ж?? 
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5; // ??????????1 
  NVIC_Init(&NVIC_InitStructure); 	
	
	NVIC_InitStructure.NVIC_IRQChannel = I2C1_EV_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
  NVIC_Init(&NVIC_InitStructure);

	NVIC_InitStructure.NVIC_IRQChannel = I2C1_ER_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
  NVIC_Init(&NVIC_InitStructure);

	/* I2C2 作为主机发请求 + 收响应,始终需要 EV + ER 中断 */
	NVIC_InitStructure.NVIC_IRQChannel = I2C2_EV_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
  NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = I2C2_ER_IRQn;
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
  NVIC_Init(&NVIC_InitStructure);


	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = TIM5_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = TIM8_CC_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
	NVIC_Init(&NVIC_InitStructure);
	NVIC_InitStructure.NVIC_IRQChannel = TIM8_UP_TIM13_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
	NVIC_Init(&NVIC_InitStructure);

	/* R15 PB15 / N11 PE13 共用 EXTI15_10_IRQn(PB15/PB12/PB13/PB14/PB15/PE13 全在 line10~15) */
	NVIC_InitStructure.NVIC_IRQChannel = EXTI15_10_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
	NVIC_Init(&NVIC_InitStructure);
	/* TIM6 1ms 节拍:用于电源按钮去抖 + 20ms 输出延时 */
	NVIC_InitStructure.NVIC_IRQChannel = TIM6_DAC_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;
	NVIC_Init(&NVIC_InitStructure);
	
#if CAN2_ENABLE	
	NVIC_InitStructure.NVIC_IRQChannel = CAN2_RX_IRQ;	   //CAN RX0?ж?
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;		
	NVIC_Init(&NVIC_InitStructure);	
	#endif
#if CAN1_ENABLE	
	NVIC_InitStructure.NVIC_IRQChannel = CAN1_RX_IRQ;	   //CAN RX0?ж?
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 5;		
	NVIC_Init(&NVIC_InitStructure);
#endif
		

  NVIC_InitStructure.NVIC_IRQChannel = ETH_IRQn;							// ????ETHERNET??ж?? 
  NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 12 ;
  NVIC_Init(&NVIC_InitStructure);
}


/*																						
*********************************************************************************************************
*	?? ?? ??: BSP_Init
*	???????: ?弶?????
*	??    ?Σ???
*	?? ?? ?: ??
*********************************************************************************************************
*/
void BSP_Init(void)
{
	NVIC_Configuration();													//?ж?????????
	delay_1ms(2000);
	
	USART_Config(COM_USART1 ,115200);							//????????
	USART_Config(COM_USART5 ,115200);
	USART_Config(COM_USART6 ,115200);
	printf("//***********************************************/\n");
	printf("/**             浩控信息科技有限公司           **/\n");
	printf("/**                2026-7-17                  **/\n");
	printf("/************************************************/\n");
  printf(">>>IPMB主机自动切换A总线和B总线-适配万邦公司...OK\n");
	
	IPMB_Bus_Enable_Init();	/* IPMB-A / IPMB-B 总线隔离器 NCA9511_DMSR 使能(PH3/PH6 拉高) */
	Bsp_I2C_Init();																		//IIC?????
	Init_MO_I2C();
	
	Bsp_ADC_Init();

	GPIO_INPUT_Config();													//GPIO??
	GPIO_OUTPUT_Config();													//GPIO??
	GPIO_Power_Config();													//R14/R15/N11 电源按钮 + EXTI + TIM6 1ms 节拍

	slave_addr_detect();													//
	print_slot_addr();														//

	Ethernet_init();
	delay_1ms(500);

	PWM_Configuration();													//PWM???????????????

	SemaphoreTakeCreate();												//?????????
	
	eeprom_test();
	GPIO_INPUT_Config();
}



static void SemaphoreTakeCreate(void)
{

		vSemaphoreCreateBinary(xUsart1Semaphore);												//????1
		xSemaphoreTake(xUsart1Semaphore,(portTickType)0);
		vSemaphoreCreateBinary(xUsart5Semaphore);												//????5
		xSemaphoreTake(xUsart5Semaphore,(portTickType)0);
		vSemaphoreCreateBinary(xUsart6Semaphore);												//????6
		xSemaphoreTake(xUsart6Semaphore,(portTickType)0);


		vSemaphoreCreateBinary(I2C1ReceiveSemaphore);										//IPMB???1
		xSemaphoreTake(I2C1ReceiveSemaphore,(portTickType)0);
		vSemaphoreCreateBinary(I2C2ReceiveSemaphore);										//IPMB???2
		xSemaphoreTake(I2C2ReceiveSemaphore,(portTickType)0);

	//IPMB 自测命令/响应队列
	//  队列 1 → I2C1_Task(IPMB-A);队列 2 → I2C2_Task(IPMB-B)。两路主机同发。
		xIPMB_CmdQueue = xQueueCreate(6, sizeof(ipmb_pkt_t));
		if (xIPMB_CmdQueue == NULL)
			printf("[Err]:create xIPMB_CmdQueue fail\r\n");
		xIPMB_RspQueue = xQueueCreate(5, sizeof(ipmb_pkt_t));   /* 深度5 FIFO: 突发多条响应排队打印,不再互相覆盖 */
		if (xIPMB_RspQueue == NULL)
			printf("[Err]:create xIPMB_RspQueue fail\r\n");
		xIPMB_CmdQueue2 = xQueueCreate(6, sizeof(ipmb_pkt_t));
		if (xIPMB_CmdQueue2 == NULL)
			printf("[Err]:create xIPMB_CmdQueue2 fail\r\n");
		xIPMB_RspQueue2 = xQueueCreate(5, sizeof(ipmb_pkt_t));   /* 深度5 FIFO: 突发多条响应排队打印,不再互相覆盖 */
		if (xIPMB_RspQueue2 == NULL)
			printf("[Err]:create xIPMB_RspQueue2 fail\r\n");

	//IPMB 地址发现任务专属响应 mailbox(与 xIPMB_RspQueue/2 分开,避免和手动命令回显抢队列)
		xIPMB_DiscRspQueue = xQueueCreate(1, sizeof(ipmb_pkt_t));
		if (xIPMB_DiscRspQueue == NULL)
			printf("[Err]:create xIPMB_DiscRspQueue fail\r\n");
		xIPMB_DiscRspQueue2 = xQueueCreate(1, sizeof(ipmb_pkt_t));
		if (xIPMB_DiscRspQueue2 == NULL)
			printf("[Err]:create xIPMB_DiscRspQueue2 fail\r\n");

	//网页监控轮询专属响应 mailbox(同样跟 xIPMB_RspQueue/2 分开,避免抢队列)
		xIPMB_WebRspQueue = xQueueCreate(1, sizeof(ipmb_pkt_t));
		if (xIPMB_WebRspQueue == NULL)
			printf("[Err]:create xIPMB_WebRspQueue fail\r\n");
		xIPMB_WebRspQueue2 = xQueueCreate(1, sizeof(ipmb_pkt_t));
		if (xIPMB_WebRspQueue2 == NULL)
			printf("[Err]:create xIPMB_WebRspQueue2 fail\r\n");

	//USART1 TX Mutex
		xUsart1TxMutex = xSemaphoreCreateMutex();
		if (xUsart1TxMutex == NULL)
			printf("[Err]:create xUsart1TxMutex fail\r\n");

	//stPrintf_Buf Mutex(Temp_Task 与 USART5_Task "?"查询 互斥访问共享打印缓冲区)
		xPrintfBufMutex = xSemaphoreCreateMutex();
		if (xPrintfBufMutex == NULL)
			printf("[Err]:create xPrintfBufMutex fail\r\n");

//		vSemaphoreCreateBinary(xCan1Semaphore);
//		vSemaphoreCreateBinary(xCan2Semaphore);									
//		xSemaphoreTake(xCan2Semaphore,(portTickType)0);
		
//		vSemaphoreCreateBinary(timechan1getSemaphore);									
//		xSemaphoreTake(timechan1getSemaphore,(portTickType)0);		
//		vSemaphoreCreateBinary(timechan2getSemaphore);									
//		xSemaphoreTake(timechan2getSemaphore,(portTickType)0);
		
#if ENABLE_UDP_TEST_TRAFFIC
#if DONT_USE_FREERTOS_QUEUE_RECEIVE_ETHERNET
#else
			queue_ethernet_Handle = xQueueCreate(UDP_RX_QUEUE_SIZE, (unsigned portBASE_TYPE)sizeof(stEVENT_t));
	if( queue_ethernet_Handle == NULL )
			printf("[Err]:create queue_ethernet fail");
	xQueueReset( queue_ethernet_Handle );
	#endif
#endif

	//PEM(Platform Event Message)主动上报专用队列:小结构(17字节)、深度2,堆足迹极小。
	//放在所有关键资源(命令/响应队列、信号量、以太网队列)之后创建,不占用它们的内存。
		xIPMB_PemQueue = xQueueCreate(2, sizeof(ipmb_pem_pkt_t));
		if (xIPMB_PemQueue == NULL)
			printf("[Err]:create xIPMB_PemQueue fail\r\n");
		xIPMB_PemQueue2 = xQueueCreate(2, sizeof(ipmb_pem_pkt_t));
		if (xIPMB_PemQueue2 == NULL)
			printf("[Err]:create xIPMB_PemQueue2 fail\r\n");

	//网页控制台异步控制通道(2026-07-27新增):POST /control 入队用,深度4;
	//响应走专属 mailbox(跟发现/网页轮询同样的分流机制),避免抢 xIPMB_RspQueue/2
		xIPMB_CtrlReqQueue = xQueueCreate(4, sizeof(IpmbCtrlReq_t));
		if (xIPMB_CtrlReqQueue == NULL)
			printf("[Err]:create xIPMB_CtrlReqQueue fail\r\n");
		xIPMB_CtrlRspQueue = xQueueCreate(1, sizeof(ipmb_pkt_t));
		if (xIPMB_CtrlRspQueue == NULL)
			printf("[Err]:create xIPMB_CtrlRspQueue fail\r\n");
		xIPMB_CtrlRspQueue2 = xQueueCreate(1, sizeof(ipmb_pkt_t));
		if (xIPMB_CtrlRspQueue2 == NULL)
			printf("[Err]:create xIPMB_CtrlRspQueue2 fail\r\n");
}

void delay_1ms(uint32_t count)
{
       u32 ticks;
       u32 told,tnow,reload,tcnt=0;
       if((0x0001&(SysTick->CTRL)) ==0)    
              vPortSetupTimerInterrupt();  
 
       reload = SysTick->LOAD;                     
       ticks = count * (SystemCoreClock / 1000);  
       
       vTaskSuspendAll();
       told=SysTick->VAL;  
       while(1)
       {
              tnow=SysTick->VAL; 
              if(tnow!=told)  
              {         
                     if(tnow<told)  
                          tcnt+=told-tnow; 
                     else     
                            tcnt+=reload-tnow+told;   
                                                     
 
                     told=tnow;   
                     if(tcnt>=ticks)break;  
              } 
       }  
       xTaskResumeAll();	

}




