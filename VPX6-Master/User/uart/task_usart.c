#include "task_usart.h"
#include "bsp_init.h"
#include "bsp_adc.h"
#include "bsp_mo_i2c.h"
#include "task_i2c.h"
#include <string.h>

/* USART5 "?"查询要用,跟 task_i2c.c 里同样的 ad-hoc extern 写法保持一致 */
extern uint16_t calculate_fan_rpm(TIM_ICUserValueTypeDef *tach_struct);

/* IPMB 2's-complement 校验和: cs = -(sum(buf[start..end-1])) */
static uint8_t ipmb_cs(const uint8_t *buf, uint8_t start, uint8_t end)
{
	uint8_t s = 0, i;
	for (i = start; i < end; i++) s = (uint8_t)(s + buf[i]);
	return (uint8_t)(-s);
}

/* IPMB 请求序列号(rqSeq,帧内 byte4 高6位):供 ipmboem 自动构帧时自增用(6bit 循环)。
 * 只在 ipmboem 里使用 —— ipmb <hex> 是手动透传命令,不能被这个计数器插手,
 * 否则用户手算的校验和会被覆盖后失效,这个命令就没法再用来手动验证协议了。 */
static uint8_t s_ipmb_rqseq = 0;

/* 把一条已构造好的 IPMB 帧同时下发到两路主机(I2C1 + I2C2)。
 * 两路各有独立命令队列,实现"同一命令同时发给两块同类从机"。
 * 纯转发,不修改帧内任何字节 —— rqSeq/校验和由调用方(ipmb 用户手填,
 * ipmboem 固件自算)各自负责。 */
static void ipmb_enqueue_both(const ipmb_pkt_t *pkt)
{
	if (pkt->len == 0) {
		Usart_SendString(COM_USART1, "<ERR 1 badlen>\r\n");
		return;
	}
	if (xIPMB_CmdQueue != NULL) {
		if (xQueueSend(xIPMB_CmdQueue, pkt, (TickType_t)10) != pdTRUE)
			Usart_SendString(COM_USART1, "<ERR 3 queue full A>\r\n");
	}
	if (xIPMB_CmdQueue2 != NULL) {
		if (xQueueSend(xIPMB_CmdQueue2, pkt, (TickType_t)10) != pdTRUE)
			Usart_SendString(COM_USART1, "<ERR 3 queue full B>\r\n");
	}
}

/* 每5秒自动轮询一次 Platform Event Message(cmd=0x02),取走 F103 内部排队的
 * PEM 事件,只打印到串口日志(复用 IPMB_Test_Command_Task 已有的 <RSP-A/B> 打印)。
 * 用户明确要求:只有 PEM 需要这种周期性轮询,其他命令不做自动轮询,
 * 保持纯手动/透明(不能影响 ipmb <hex> 的手动验证用途)。
 * rqSeq 用独立计数器,和 ipmboem 那个不共用,互不影响。 */
TaskHandle_t IPMB_PEM_Poll_Task_Handle = NULL;

void IPMB_PEM_Poll_Task(void* parameter)
{
	static uint8_t s_pem_poll_rqseq = 0;
	(void)parameter;
	(void)s_pem_poll_rqseq;   /* IPMB_PEM_AUTO_POLL_ENABLE=0 时不使用, 避免告警 */

	for (;;)
	{
		vTaskDelay(5000);
#if IPMB_PEM_AUTO_POLL_ENABLE
		{
		uint8_t n, i;
		/* 遍历发现表, 给每一块从机各发一条 0x02 轮询 —— 多从机场景下必须逐个
		 * 轮询, 否则只有发现表第一个从机会上报 PEM。发现表为空(尚未发现)时兜底
		 * 用历史默认地址 g_slave_ipmb_addr(0x8E)发一条, 保持改动前的行为。 */
		n = g_slave_count;
		if (n == 0) {
			uint8_t fallback = g_slave_ipmb_addr;
			ipmb_pkt_t pkt;
			pkt.len = 0;
			pkt.buf[pkt.len++] = fallback;   /* 目标(兜底默认地址) */
			pkt.buf[pkt.len++] = 0x10;   /* netFn=04h<<2 */
			pkt.buf[pkt.len] = ipmb_cs(pkt.buf, 0, 2); pkt.len++;  /* 头校验 */
			pkt.buf[pkt.len++] = 0x20;   /* rqSA 主控板 */
			pkt.buf[pkt.len++] = (uint8_t)(s_pem_poll_rqseq << 2);  /* rqSeq自增,lun=0 */
			pkt.buf[pkt.len++] = 0x02;   /* cmd Platform Event Message */
			pkt.buf[pkt.len] = ipmb_cs(pkt.buf, 3, pkt.len);        /* 尾校验 */
			pkt.len++;
			s_pem_poll_rqseq = (uint8_t)((s_pem_poll_rqseq + 1) & 0x3F);
			ipmb_enqueue_both(&pkt);
			continue;
		}
		if (n > IPMB_MAX_SLAVES) n = IPMB_MAX_SLAVES;   /* 防御: count 越界保护 */
		for (i = 0; i < n; i++)
		{
			ipmb_pkt_t pkt;
			pkt.len = 0;
			pkt.buf[pkt.len++] = g_slave_addrs[i];   /* 目标(发现表第 i 个从机) */
			pkt.buf[pkt.len++] = 0x10;   /* netFn=04h<<2 */
			pkt.buf[pkt.len] = ipmb_cs(pkt.buf, 0, 2); pkt.len++;  /* 头校验,按各自地址算 */
			pkt.buf[pkt.len++] = 0x20;   /* rqSA 主控板 */
			pkt.buf[pkt.len++] = (uint8_t)(s_pem_poll_rqseq << 2);  /* rqSeq自增,lun=0 */
			pkt.buf[pkt.len++] = 0x02;   /* cmd Platform Event Message */
			pkt.buf[pkt.len] = ipmb_cs(pkt.buf, 3, pkt.len);        /* 尾校验 */
			pkt.len++;
			s_pem_poll_rqseq = (uint8_t)((s_pem_poll_rqseq + 1) & 0x3F);
			ipmb_enqueue_both(&pkt);
		}
		}
#endif  /* IPMB_PEM_AUTO_POLL_ENABLE */
	}
}

/***************************������**********************************/
TaskHandle_t USART1_Task_Handle = NULL;
TaskHandle_t USART2_Task_Handle = NULL;
TaskHandle_t USART3_Task_Handle = NULL;
TaskHandle_t USART4_Task_Handle = NULL;
TaskHandle_t USART5_Task_Handle = NULL;
TaskHandle_t USART6_Task_Handle = NULL;
/*************************�ź������**********************************/
xTaskHandle xUsart1Semaphore    = NULL; 
xTaskHandle xUsart2Semaphore    = NULL; 
xTaskHandle xUsart3Semaphore    = NULL; 
xTaskHandle xUsart4Semaphore    = NULL; 
xTaskHandle xUsart5Semaphore    = NULL; 
xTaskHandle xUsart6Semaphore    = NULL; 

stPRINTF_BUF_t stPrintf_Buf;
xSemaphoreHandle xPrintfBufMutex = NULL;   /* 保护 stPrintf_Buf 在 Temp_Task / USART5_Task 之间互斥访问 */

/*																						
*********************************************************************************************************
*	��������: USART1_Task
*	����˵��: USART1_Task������
*	��    ��:void* parameter
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void USART1_Task(void* parameter)
{
  while (1)
  {
		if(xSemaphoreTake(xUsart1Semaphore,(TickType_t)0) == pdTRUE)
		{
			if(stUsart1_recv_Data.recv_data_len > 0)
			{
				if(memcmp((char *)stUsart1_recv_Data.recv_buf, "power on" , strlen("power on")) == 0)
				{
					GPIO_Cmd_t cmd = GPIO_CMD_POWER_ON_BMC_CPLD_PWR_BTN;
					if(xGpioCmdQueue != NULL)
						xQueueSend(xGpioCmdQueue, &cmd, (TickType_t)10);
				}
				else if(memcmp((char *)stUsart1_recv_Data.recv_buf, "power off" , strlen("power off")) == 0)
				{
					GPIO_Cmd_t cmd = GPIO_CMD_POWER_OFF_BMC_CPLD_PWR_BTN;
					if(xGpioCmdQueue != NULL)
						xQueueSend(xGpioCmdQueue, &cmd, (TickType_t)10);
				}
				else if(memcmp((char *)stUsart1_recv_Data.recv_buf, "reset" , strlen("reset")) == 0)
				{
					GPIO_Cmd_t cmd = GPIO_CMD_RESET_BMC_OUT_SYS_RST;
					if(xGpioCmdQueue != NULL)
						xQueueSend(xGpioCmdQueue, &cmd, (TickType_t)10);
				}
				else if(memcmp((char *)stUsart1_recv_Data.recv_buf, "fan ", 4) == 0)
				{
					/* 串口手动设风扇占空比:fan <0-100>,一个值同时控制 4 路风扇。
					 * 立即下发生效(不用等 Temp_Task 的 2 秒周期),转速反馈仍由
					 * Temp_Task 周期打印([TACH IN] RPM_1..4),能看到转速跟着变。 */
					uint8_t in_len  = stUsart1_recv_Data.recv_data_len;
					const char *p   = (const char *)stUsart1_recv_Data.recv_buf + 4;
					const char *end = (const char *)stUsart1_recv_Data.recv_buf + in_len;
					uint16_t duty = 0;
					uint8_t got = 0;
					while (p < end && (*p == ' ' || *p == '\t')) p++;
					while (p < end && *p >= '0' && *p <= '9') {
						duty = (uint16_t)(duty * 10 + (uint16_t)(*p - '0'));
						p++; got = 1;
					}
					if (!got) {
						Usart_SendString(COM_USART1, "<ERR fan: usage 'fan 0-100'>\r\n");
					} else {
						char msg[48];
						if (duty > 100) duty = 100;
						Fan_Set_All_Duty(duty);
						sprintf(msg, "<FAN all4 = %u%%>\r\n", (unsigned)duty);
						Usart_SendString(COM_USART1, msg);
					}
				}
				else if(memcmp((char *)stUsart1_recv_Data.recv_buf, "ipmboem ", 8) == 0)
				{
					/* OEM Get Module Info(补充协议,非标 netFn):
					 *   输入只给 itemID 列表,如  ipmboem 12   或  ipmboem 11 12
					 *   自动构帧(含非标 netFn=0x2E 不左移 + 两个校验和):
					 *   [0]=g_slave_ipmb_addr 目标(动态发现)  [1]=0x2E  [2]=cs1  [3]=0x20 rqSA
					 *   [4]=rqSeq(自增)/lun  [5]=0x12 cmd  [6]=item_count
					 *   [7..]=itemID  [末]=cs2 */
					ipmb_pkt_t pkt;
					uint8_t in_len = stUsart1_recv_Data.recv_data_len;
					const char *p   = (const char *)stUsart1_recv_Data.recv_buf + 8;
					const char *end = (const char *)stUsart1_recv_Data.recv_buf + in_len;
					uint8_t ids[16];
					uint8_t id_cnt = 0;

					/* 解析 itemID 列表(十六进制,空白分隔) */
					while (p < end && id_cnt < sizeof(ids)) {
						while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
						if (p >= end) break;
						if (!p[0] || !p[1]) break;
						{
							uint8_t hi, lo; char c;
							c = p[0]; hi = (c <= '9') ? (c - '0') : (c | 0x20) - 'a' + 10;
							c = p[1]; lo = (c <= '9') ? (c - '0') : (c | 0x20) - 'a' + 10;
							ids[id_cnt++] = (uint8_t)((hi << 4) | lo);
							p += 2;
						}
					}

					if (id_cnt == 0) {
						Usart_SendString(COM_USART1, "<ERR 1 badlen>\r\n");
					} else {
						uint8_t i;
						pkt.len = 0;
						pkt.buf[pkt.len++] = g_slave_ipmb_addr;    /* 目标(业务板 IPMB 地址,动态发现) */
						pkt.buf[pkt.len++] = 0x2E;                 /* 非标 netFn,不左移 */
						pkt.buf[pkt.len++] = ipmb_cs(pkt.buf, 0, 2);   /* cs1 覆盖 [0..1] */
						pkt.buf[pkt.len++] = 0x20;                 /* rqSA 主控板 */
						pkt.buf[pkt.len++] = (uint8_t)(s_ipmb_rqseq << 2); /* rqSeq自增,lun=0 */
						s_ipmb_rqseq = (uint8_t)((s_ipmb_rqseq + 1) & 0x3F);
						pkt.buf[pkt.len++] = 0x12;                 /* cmd Get Module Info */
						pkt.buf[pkt.len++] = id_cnt;               /* item_count */
						for (i = 0; i < id_cnt; i++) pkt.buf[pkt.len++] = ids[i];
						pkt.buf[pkt.len] = ipmb_cs(pkt.buf, 3, pkt.len);  /* cs2 覆盖 [3..末-1] */
						pkt.len++;
						ipmb_enqueue_both(&pkt);
					}
				}
				else if(memcmp((char *)stUsart1_recv_Data.recv_buf, "oemipmb ", 8) == 0)
				{
					/* OEM协议专用透传命令:和"ipmb "完全同一套逐字节解析+转发逻辑
					 * (用户需自带完整帧,含两个校验和,固件不做任何修改),只是单独
					 * 换个命令名方便区分测试OEM(netFn=2Eh)帧,不影响"ipmb "命令
					 * 本身的行为 —— 遵循"手动验证类命令禁止自动纠正"的原则 */
					ipmb_pkt_t pkt;
					uint8_t in_len = stUsart1_recv_Data.recv_data_len;
					const char *p = (const char *)stUsart1_recv_Data.recv_buf + 8;
					const char *end = (const char *)stUsart1_recv_Data.recv_buf + in_len;
					pkt.len = 0;
					while (p < end && pkt.len < IPMB_TEST_BUF_SIZE) {
						while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
						if (p >= end) break;
						if (!p[0] || !p[1]) break;
						{
							uint8_t hi, lo;
							char c;
							c = p[0]; hi = (c <= '9') ? (c - '0') : (c | 0x20) - 'a' + 10;
							c = p[1]; lo = (c <= '9') ? (c - '0') : (c | 0x20) - 'a' + 10;
							pkt.buf[pkt.len++] = (uint8_t)((hi << 4) | lo);
							p += 2;
						}
					}
					ipmb_enqueue_both(&pkt);
				}
				else if(memcmp((char *)stUsart1_recv_Data.recv_buf, "ipmb ", 5) == 0)
				{
					/* 通用:逐字节透传(用户需自带完整帧含两个校验和,固件不做任何修改) */
					ipmb_pkt_t pkt;
					uint8_t in_len = stUsart1_recv_Data.recv_data_len;
					const char *p = (const char *)stUsart1_recv_Data.recv_buf + 5;
					const char *end = (const char *)stUsart1_recv_Data.recv_buf + in_len;
					pkt.len = 0;
					while (p < end && pkt.len < IPMB_TEST_BUF_SIZE) {
						while (p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')) p++;
						if (p >= end) break;
						if (!p[0] || !p[1]) break;
						{
							uint8_t hi, lo;
							char c;
							c = p[0]; hi = (c <= '9') ? (c - '0') : (c | 0x20) - 'a' + 10;
							c = p[1]; lo = (c <= '9') ? (c - '0') : (c | 0x20) - 'a' + 10;
							pkt.buf[pkt.len++] = (uint8_t)((hi << 4) | lo);
							p += 2;
						}
					}
					ipmb_enqueue_both(&pkt);
				}
			}
			stUsart1_recv_Data.recv_data_len = 0;	//��ս���
		}
  }
}


/*																						
*********************************************************************************************************
*	��������: USART2_Task
*	����˵��: USART2_Task������
*	��    ��:void* parameter
*	�� �� ֵ: ��
*********************************************************************************************************
*/
#if USART2_EN	
void USART2_Task(void* parameter)
{	
	uint8_t i;
	
  while (1)
  {
		if(xSemaphoreTake(xUsart2Semaphore,(TickType_t)0) == pdTRUE)
		{
			printf("usart2 receive code\r\n");
			for(i=0;i<stUsart2_recv_Data.recv_data_len;i++){
				printf(" %c ",stUsart2_recv_Data.recv_buf[i]);
			}
			Usart_SendString( COM_USART2, (char *)stUsart2_recv_Data.recv_buf);
			stUsart2_recv_Data.recv_data_len = 0;	//��ս���
		}

  }
}
#endif
/*																						
*********************************************************************************************************
*	��������: USART3_Task
*	����˵��: USART3_Task������
*	��    ��:void* parameter
*	�� �� ֵ: ��
*********************************************************************************************************
*/
#if USART3_EN	
void USART3_Task(void* parameter)
{	
	uint8_t i;

  while (1)
  {
		if(xSemaphoreTake(xUsart3Semaphore,(TickType_t)0) == pdTRUE)
		{
			printf("usart3 receive code\r\n");
			for(i=0;i<stUsart3_recv_Data.recv_data_len;i++){
				printf(" %c ",stUsart3_recv_Data.recv_buf[i]);
			}
			Usart_SendString( COM_USART3, (char *)stUsart3_recv_Data.recv_buf);
			stUsart3_recv_Data.recv_data_len = 0;	//��ս���
		}
  }
}
#endif
/*																						
*********************************************************************************************************
*	��������: USART4_Task
*	����˵��: USART4_Task������
*	��    ��:void* parameter
*	�� �� ֵ: ��
*********************************************************************************************************
*/
#if USART4_EN	
void USART4_Task(void* parameter)
{	
	uint8_t i;
	
  while (1)
  {
		if(xSemaphoreTake(xUsart4Semaphore,(TickType_t)0) == pdTRUE)
		{
			printf("usart4 receive code\r\n");
			for(i=0;i<stUsart4_recv_Data.recv_data_len;i++){
				printf(" %c ",stUsart4_recv_Data.recv_buf[i]);
			}
			Usart_SendString( COM_USART4, (char *)stUsart4_recv_Data.recv_buf);
			stUsart4_recv_Data.recv_data_len = 0;	//��ս���
		}
  }
}
#endif
#if USART5_EN	
/*																						
*********************************************************************************************************
*	��������: USART5_Task
*	����˵��: USART5_Task������
*	��    ��:void* parameter
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void USART5_Task(void* parameter)
{	
	uint8_t i;
	
  while (1)
  {
		if(xSemaphoreTake(xUsart5Semaphore,(TickType_t)0) == pdTRUE)
		{
			printf("usart5 receive code\r\n");
			for(i=0;i<stUsart5_recv_Data.recv_data_len;i++){
				printf(" %c ",stUsart5_recv_Data.recv_buf[i]);
			}
			
			if(stUsart5_recv_Data.recv_data_len > 0)
			{
				if(memcmp((char *)stUsart5_recv_Data.recv_buf, "?" , strlen("?")) == 0)
				{
					/* 参照 Temp_Task 的采集顺序,给 USART5 拼一份完整快照,
					 * 但发送内容都攒在自己的 status_buf 里,不依赖 stPrintf_Buf
					 * 什么时候被 Temp_Task 清空。 */
					char status_buf[900];
					char line_buf[400];
					uint16_t rpm1, rpm2, rpm3, rpm4;
					float bpd_vin = 0.0f, bpd_iout = 0.0f, bpd_temp = 0.0f;

					status_buf[0] = '\0';

					/* RACK/GA:跟 Temp_Task 一样直接调用,内部会顺带 printf 一次到
					 * USART1(不进 stPrintf_Buf,无害,Temp_Task 本来也这么打)。
					 * 同时自己再读一遍同一批 GPIO 拼进 status_buf,这样 USART5
					 * 这边也能拿到,不用改 print_rack_addr/print_slot_addr 本身
					 * (改了会导致 Temp_Task 的 USART1 banner 里这两行重复打印)。 */
					print_rack_addr();
					print_slot_addr();
					{
						uint8_t rid0 = GPIO_ReadInputDataBit(RACK_ID0_PORT, RACK_ID0_PIN);
						uint8_t rid1 = GPIO_ReadInputDataBit(RACK_ID1_PORT, RACK_ID1_PIN);
						uint8_t rid2 = GPIO_ReadInputDataBit(RACK_ID2_PORT, RACK_ID2_PIN);
						uint8_t rid3 = GPIO_ReadInputDataBit(RACK_ID3_PORT, RACK_ID3_PIN);
						uint8_t rid4 = GPIO_ReadInputDataBit(RACK_ID4_PORT, RACK_ID4_PIN);
						uint8_t rid5 = GPIO_ReadInputDataBit(RACK_ID5_PORT, RACK_ID5_PIN);
						uint8_t ga0  = GPIO_ReadInputDataBit(GPIO_GA0_PORT, GPIO_GA0_PIN);
						uint8_t ga1  = GPIO_ReadInputDataBit(GPIO_GA1_PORT, GPIO_GA1_PIN);
						uint8_t ga2  = GPIO_ReadInputDataBit(GPIO_GA2_PORT, GPIO_GA2_PIN);
						uint8_t ga3  = GPIO_ReadInputDataBit(GPIO_GA3_PORT, GPIO_GA3_PIN);
						uint8_t ga4  = GPIO_ReadInputDataBit(GPIO_GA4_PORT, GPIO_GA4_PIN);
						uint8_t gap  = GPIO_ReadInputDataBit(GPIO_GAP_PORT, GPIO_GAP_PIN);

						snprintf(line_buf, sizeof(line_buf),
								"RACK : RID0=%d RID1=%d RID2=%d RID3=%d RID4=%d RID5=%d\r\n"
								"GA   : GA0=%d GA1=%d GA2=%d GA3=%d GA4=%d GAP=%d\r\n",
								rid0, rid1, rid2, rid3, rid4, rid5,
								ga0, ga1, ga2, ga3, ga4, gap);
						strncat(status_buf, line_buf, sizeof(status_buf) - strlen(status_buf) - 1);
					}

					/* ADC电压+两路温度:复用现有采集函数,内部仍会顺带 strcat 进
					 * 共享的 stPrintf_Buf,用互斥锁保护那段共享区不被 Temp_Task
					 * 同时写乱;拿到结果立刻拷出来、清空共享区、放锁,后面全用
					 * 自己的缓冲区,不再受 Temp_Task 清空节奏影响 */
					if (xSemaphoreTake(xPrintfBufMutex, pdMS_TO_TICKS(300)) == pdTRUE)
					{
						memset(stPrintf_Buf.buf, 0, sizeof(stPrintf_Buf.buf));
						stPrintf_Buf.size = 0;

						ADC_Filter();
						ADC_Calculation( &stADC_Data );
						Board_ADDR90_temp();
						Board_ADDR92_temp();

						strncat(status_buf, (char *)stPrintf_Buf.buf, sizeof(status_buf) - strlen(status_buf) - 1);

						memset(stPrintf_Buf.buf, 0, sizeof(stPrintf_Buf.buf));
						stPrintf_Buf.size = 0;
						xSemaphoreGive(xPrintfBufMutex);
					}

					/* 风扇占空比+转速+BPD20550:跟 Temp_Task 一样直接读,不经过
					 * 共享区;BPD20550 用局部变量接,不碰 task_i2c.c 里 Temp_Task
					 * 自己缓存的 s_bpd_* */
					rpm1 = calculate_fan_rpm(&TIM_ICUserValueStructure_TACH1);
					rpm2 = calculate_fan_rpm(&TIM_ICUserValueStructure_TACH2);
					rpm3 = calculate_fan_rpm(&TIM_ICUserValueStructure_TACH3);
					rpm4 = calculate_fan_rpm(&TIM_ICUserValueStructure_TACH4);
					{
						char bpd_buf[256] = {0};
						Board_BPD20550_current(bpd_buf);
						(void)sscanf(bpd_buf, "BPD20550 MFR_ID:%*x,%*x\r\nVin: %f V\r\nIout: %f A\r\nTemp: %f C",
									 &bpd_vin, &bpd_iout, &bpd_temp);
					}

					snprintf(line_buf, sizeof(line_buf),
							"[PWM OUT] (serial-set duty via 'fan 0-100', all 4 = %3d%%)\r\n"
							"FAN_1: %3d%%  PE6(TIM9_CH1)\r\n"
							"FAN_2: %3d%%  PE5(TIM9_CH2)\r\n"
							"FAN_3: %3d%%  PC8(TIM8_CH3)\r\n"
							"FAN_4: %3d%%  PI6(TIM8_CH2)\r\n"
							"[TACH IN]\r\n"
							"RPM_1: %5d  PB9(TIM4_CH4)\r\n"
							"RPM_2: %5d  PI2(TIM8_CH4)\r\n"
							"RPM_3: %5d  PH11(TIM5_CH2)\r\n"
							"RPM_4: %5d  PH10(TIM5_CH1)\r\n"
							"[BPD20550]\r\n"
							"Vin:  %.2f V\r\n"
							"Iout: %.2f A\r\n"
							"Temp: %.1f C\r\n",
							g_fan_duty,
							g_fan_duty, g_fan_duty, g_fan_duty, g_fan_duty,
							rpm1, rpm2, rpm3, rpm4,
							bpd_vin, bpd_iout, bpd_temp);
					strncat(status_buf, line_buf, sizeof(status_buf) - strlen(status_buf) - 1);

					Usart_SendString( COM_USART5, status_buf);
				}
				else
				{
					Usart_SendString( COM_USART5, (char *)stUsart5_recv_Data.recv_buf);
				}
			}
			stUsart5_recv_Data.recv_data_len = 0;	//��ս���
		}
  }
}
#endif
#if USART6_EN	
/*																						
*********************************************************************************************************
*	��������: USART6_Task
*	����˵��: USART6_Task������
*	��    ��:void* parameter
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void USART6_Task(void* parameter)
{	
	uint8_t i;

	while (1)
	{
	if(xSemaphoreTake(xUsart6Semaphore,(TickType_t)0) == pdTRUE)
	{
		printf("usart6 receive code\r\n");
		for(i=0;i<stUsart6_recv_Data.recv_data_len;i++){
		printf(" %c ",stUsart6_recv_Data.recv_buf[i]);
	}
	Usart_SendString( COM_USART6, (char *)stUsart6_recv_Data.recv_buf);
	stUsart6_recv_Data.recv_data_len = 0;	//��ս���
	}
	}
}
#endif
