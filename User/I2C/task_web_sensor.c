/*
*********************************************************************************************************
*	【2026-07-27停用】网页控制台改造后,本文件的单从机轮询已被 task_slot_poll.c 的
*	IPMB_Slot_Poll_Task(多槽位版,同时覆盖本文件这5个传感器)取代,bsp_init.c 不再创建
*	Web_Sensor_Poll_Task,bsp_httpd.c 也不再读 g_web_sensor_snapshot。本仓库没有版本控制,
*	为了不丢历史实现细节先保留文件本体、不物理删除,但不再加入编译/调用链——如确认不再
*	需要参考,可安全整体删除本文件+task_web_sensor.h。
*********************************************************************************************************
*/
/*
*********************************************************************************************************
*	模 块 名: task_web_sensor.c
*	功能说明: 网页只读监控——每3秒真实发一轮 IPMB Get Sensor Reading(cmd 0x2D)给从设备(FRU板),
*	          读回90Temp/92Temp/V12/V0.75/Iout共5个传感器,缓存进 g_web_sensor_snapshot 供
*	          bsp_httpd.c 渲染网页。构帧/发队列的写法照抄 task_usart.c 里 ipmboem 和
*	          IPMB_PEM_Poll_Task 已经验证过的模式,只是本文件自己复制一份 checksum/入队
*	          小函数(task_usart.c 里那两个是 static,故意不跨文件复用,维持各文件自带一份的现有约定)。
*	注    意: 响应走专属 mailbox(xIPMB_WebRspQueue/2),不进 xIPMB_RspQueue/2,避免跟
*	          IPMB_Test_Command_Task 抢同一条响应——具体机制见 task_i2c.h 里
*	          IPMB_WEB_HOST_ADDR_7BIT 的注释。
*********************************************************************************************************
*/
#include "task_web_sensor.h"
#include "task_i2c.h"
#include "queue.h"
#include <stdio.h>

volatile WebSensorSnapshot_t g_web_sensor_snapshot = {0};
TaskHandle_t Web_Sensor_Poll_Task_Handle = NULL;

/* IPMB 2's-complement 校验和,跟 task_usart.c/task_i2c.c 里各自那份逻辑一致 */
static uint8_t web_ipmb_cs(const uint8_t *buf, uint8_t start, uint8_t end)
{
	uint8_t s = 0, i;
	for (i = start; i < end; i++) s = (uint8_t)(s + buf[i]);
	return (uint8_t)(-s);
}

/* 把构造好的请求帧同时下发到两路主机命令队列,跟 ipmb_enqueue_both() 同样的做法 */
static void web_ipmb_enqueue_both(const ipmb_pkt_t *pkt)
{
	if (xIPMB_CmdQueue != NULL)  xQueueSend(xIPMB_CmdQueue,  pkt, (TickType_t)10);
	if (xIPMB_CmdQueue2 != NULL) xQueueSend(xIPMB_CmdQueue2, pkt, (TickType_t)10);
}

/* 5个传感器的 Get Sensor Reading sensor number(见 指令.txt 里⑤节的已验证示例值),
 * field_bit 对应 g_web_sensor_snapshot.valid_mask 里的 bit 位置 */
typedef struct {
	uint8_t sensor_num;
	uint8_t field_bit;
} WebSensorDef_t;

static const WebSensorDef_t s_sensor_defs[5] = {
	{0x04, 0},   /* 90Temp */
	{0x20, 1},   /* 92Temp */
	{0x03, 2},   /* V12    */
	{0x19, 3},   /* V0.75  */
	{0x07, 4},   /* Iout   */
};

void Web_Sensor_Poll_Task(void *parameter)
{
	static uint8_t s_web_rqseq = 0;
	uint8_t i;
	(void)parameter;

	for (;;)
	{
		vTaskDelay(3000);

		for (i = 0; i < 5; i++)
		{
			ipmb_pkt_t pkt, rsp;
			uint8_t got_it = 0;

			pkt.len = 0;
			pkt.buf[pkt.len++] = g_slave_ipmb_addr;               /* 目标:动态发现出来的从机地址 */
			pkt.buf[pkt.len++] = 0x10;                            /* netFn=04h(Sensor/Event)<<2 */
			pkt.buf[pkt.len] = web_ipmb_cs(pkt.buf, 0, 2); pkt.len++;  /* 头校验 */
			pkt.buf[pkt.len++] = IPMB_WEB_HOST_ADDR_7BIT;         /* rqSA标记,供响应分流识别 */
			pkt.buf[pkt.len++] = (uint8_t)(s_web_rqseq << 2);     /* rqSeq自增,lun=0 */
			pkt.buf[pkt.len++] = 0x2D;                            /* cmd Get Sensor Reading */
			pkt.buf[pkt.len++] = s_sensor_defs[i].sensor_num;     /* data[0]=传感器编号 */
			pkt.buf[pkt.len] = web_ipmb_cs(pkt.buf, 3, pkt.len); pkt.len++;  /* 尾校验 */
			s_web_rqseq = (uint8_t)((s_web_rqseq + 1) & 0x3F);

			web_ipmb_enqueue_both(&pkt);

			/* 只有当前活跃的那一路总线会真的发出去、真的有响应,两个 mailbox 都限时等一下 */
			if (xIPMB_WebRspQueue != NULL && xQueueReceive(xIPMB_WebRspQueue, &rsp, (TickType_t)150) == pdTRUE)
			{
				got_it = 1;
			}
			else if (xIPMB_WebRspQueue2 != NULL && xQueueReceive(xIPMB_WebRspQueue2, &rsp, (TickType_t)150) == pdTRUE)
			{
				got_it = 1;
			}

			if (got_it && rsp.len >= 8 && rsp.buf[6] == 0x00)
			{
				uint8_t val = rsp.buf[7];   /* 响应第7字节(0-based)=数值,见 指令.txt ⑤节说明 */
				switch (s_sensor_defs[i].field_bit)
				{
				case 0: g_web_sensor_snapshot.temp90 = val; break;
				case 1: g_web_sensor_snapshot.temp92 = val; break;
				case 2: g_web_sensor_snapshot.v12    = val; break;
				case 3: g_web_sensor_snapshot.v075   = val; break;
				case 4: g_web_sensor_snapshot.iout   = val; break;
				default: break;
				}
				g_web_sensor_snapshot.valid_mask |= (uint8_t)(1u << s_sensor_defs[i].field_bit);
			}
			else
			{
				g_web_sensor_snapshot.valid_mask &= (uint8_t)~(1u << s_sensor_defs[i].field_bit);
			}
		}

		printf(">>[WEB] 90T=%u 92T=%u V12=%u V0.75=%u Iout=%u mask=0x%02X\r\n",
			(unsigned)g_web_sensor_snapshot.temp90, (unsigned)g_web_sensor_snapshot.temp92,
			(unsigned)g_web_sensor_snapshot.v12, (unsigned)g_web_sensor_snapshot.v075,
			(unsigned)g_web_sensor_snapshot.iout, (unsigned)g_web_sensor_snapshot.valid_mask);
	}
}
