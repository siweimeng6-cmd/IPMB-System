#include "task_slot_poll.h"
#include "task_i2c.h"
#include "task_slot_cache.h"
#include "ipmb_request_builder.h"
#include "queue.h"
#include <stdio.h>
#include <string.h>

TaskHandle_t IPMB_Slot_Poll_Task_Handle = NULL;

/* 【2026-07-28更正】把已构造好的请求帧同时下发到两路主机命令队列,再等专属 mailbox 应答。
 * 实测发现两个问题:
 * 1) IPMB_Discovery_Task 每15秒一轮扫描32个候选地址期间会持续占用 xIPMB_CmdQueue/2,
 *    轮询请求如果恰好排在 discovery 探测命令后面,单次等待窗口偶尔不够用。
 * 2) 更严重的:如果不校验响应到底是不是"这次请求"的答复,从机某一路读取异常慢
 *    (比如这块测试板没接的90Temp/92Temp,内部读一个不存在的I2C器件可能卡住/严重超时)时,
 *    轮询任务等不到就已经放弃、转去问下一个传感器了;那条迟到的旧响应后来才姗姗来迟地
 *    被塞进 mailbox,会被"下一个传感器"的等待逻辑不加分辨地捡走,导致网页上出现
 *    "数据错位"(比如V12显示了本该属于90Temp的值)——这是实测确认过的真实竞态条件。
 * 修复:IPMB 响应帧 buf[4](rqSeq<<2|lun)协议上要求原样回显请求里发的序号,这里改成
 * 只接受 buf[4] 跟这次请求所用 expect_rqseq 匹配的响应,不匹配就当陈旧数据丢弃、
 * 在同一个时间预算内继续等,而不是来者不拒。 */
static uint8_t slot_poll_send_wait(const ipmb_pkt_t *pkt, ipmb_pkt_t *rsp_out, uint8_t expect_rqseq)
{
	uint8_t attempt;
	uint8_t expect_byte4 = (uint8_t)(expect_rqseq << 2);   /* lun固定0,跟请求里的编码一致 */

	for (attempt = 0; attempt < 2; attempt++) {
		TickType_t deadline;

		if (xIPMB_CmdQueue != NULL)  xQueueSend(xIPMB_CmdQueue,  pkt, (TickType_t)10);
		if (xIPMB_CmdQueue2 != NULL) xQueueSend(xIPMB_CmdQueue2, pkt, (TickType_t)10);

		deadline = xTaskGetTickCount() + pdMS_TO_TICKS(400);
		while ((int32_t)(deadline - xTaskGetTickCount()) > 0) {
			/* 【2026-08-20】先查活跃总线那一路。原来固定先等 A 再等 B,活跃总线是 B 时
			 * 每条请求都要在 A 上白等满 20 tick;每轮 N×10 条请求累积起来是几百毫秒的
			 * 固定损耗。两个 mailbox 仍然都查——failover 切换过程中响应可能来自另一路。 */
			xQueueHandle first  = (g_host_active_bus == HOST_BUS_A) ? xIPMB_WebRspQueue  : xIPMB_WebRspQueue2;
			xQueueHandle second = (g_host_active_bus == HOST_BUS_A) ? xIPMB_WebRspQueue2 : xIPMB_WebRspQueue;

			if (first != NULL &&
			    xQueueReceive(first, rsp_out, (TickType_t)20) == pdTRUE) {
				if (rsp_out->len >= 5 && rsp_out->buf[4] == expect_byte4) return 1;
				continue;   /* 陈旧/不匹配的响应,丢弃,用剩余时间预算继续等 */
			}
			if (second != NULL &&
			    xQueueReceive(second, rsp_out, (TickType_t)20) == pdTRUE) {
				if (rsp_out->len >= 5 && rsp_out->buf[4] == expect_byte4) return 1;
				continue;
			}
		}
	}
	return 0;
}

/* 【2026-08-20】s_rqseq 从 IPMB_Slot_Poll_Task 的局部 static 提到文件作用域,
 * 供下面新抽出的 slot_poll_one() 共用同一个递增序号——同 task_ctrl_dispatch.c
 * 里 s_rqseq 因为同样的原因(要对多个目标各发一次)做的处理。
 * s_round 是轮次计数,供传感器指数退避和 [SLOT-POLL] 打印降频用。 */
static uint8_t  s_rqseq = 0;
static uint32_t s_round = 0;

/*
*********************************************************************************************************
*	函 数 名: slot_poll_one
*	功能说明: 对单个槽位跑完一轮采集(Device ID/Get Slot/Board Status/PEM/7个传感器),
*	          原来是 IPMB_Slot_Poll_Task 里直接内联的唯一一条路径,2026-08-20抽成独立
*	          函数——轮询从"每轮只查一个槽位(轮转)"改成"每轮把发现表里的槽位全查一遍",
*	          需要对多个地址各调一次。发的请求清单、顺序、构造函数与抽取前完全一致。
*	形    参: target_addr: 目标从机 7bit 地址
*********************************************************************************************************
*/
static void slot_poll_one(uint8_t target_addr)
{
	uint8_t i, rqseq;
	IpmbSlotCache_t *slot;
	ipmb_pkt_t pkt, rsp;

	slot = Ipmb_SlotCache_FindOrAlloc(target_addr);
	if (slot == NULL) return;   /* 8个缓存槽位已满,理论上和 g_slave_addrs 上限同步,不会发生 */

	/* Get Device ID 只在这个槽位第一次被发现(还没有有效数据)时拉一次:
	 * 这是真正静态的字段(设备型号等信息不会变),没必要每轮重复查询,
	 * 省下的总线时间留给传感器/状态轮询 */
	if (!slot->devid_valid) {
		rqseq = s_rqseq;
		IPMB_Build_GetDeviceID(&pkt, target_addr, IPMB_WEB_HOST_ADDR_7BIT, rqseq);
		s_rqseq = (uint8_t)((s_rqseq + 1) & 0x3F);
		if (slot_poll_send_wait(&pkt, &rsp, rqseq)) Ipmb_SlotCache_StoreDeviceID(slot, &rsp);
	}

	/* Get Slot 改成跟 Board Status 一样每轮都查,实时反映槽位号/GA 引脚状态
	 * (原来只在第一次发现时读一次,物理换槽位后网页会长期显示旧值) */
	rqseq = s_rqseq;
	IPMB_Build_GetSlot(&pkt, target_addr, IPMB_WEB_HOST_ADDR_7BIT, rqseq);
	s_rqseq = (uint8_t)((s_rqseq + 1) & 0x3F);
	if (slot_poll_send_wait(&pkt, &rsp, rqseq)) Ipmb_SlotCache_StoreSlotReadback(slot, &rsp);
	else slot->slot_valid = 0;

	rqseq = s_rqseq;
	IPMB_Build_GetBoardStatus(&pkt, target_addr, IPMB_WEB_HOST_ADDR_7BIT, rqseq);
	s_rqseq = (uint8_t)((s_rqseq + 1) & 0x3F);
	if (slot_poll_send_wait(&pkt, &rsp, rqseq)) Ipmb_SlotCache_StoreBoardStatus(slot, &rsp);
	else slot->boardstat_valid = 0;

	/* 【2026-07-28新增】修复:项目里原有的 IPMB_PEM_Poll_Task(task_usart.c,每5秒轮询)
	 * 用真实主控地址 0x20 发请求,响应会走进普通 xIPMB_RspQueue/2 只被打印,从来没有
	 * 调用过 Ipmb_SlotCache_StorePem——网页 PEM 缓存只有在从机真正"自主抢总线推送"
	 * (task_i2c.c 里 is_pem_push_frame 命中那两处)时才会被写入,如果从机开机以来
	 * 一直没有真正自主推送过,缓存就会一直停在 memset 清零的初始状态,网页上表现为
	 * "未刷新"+全部字段是0,不是解析错误。这里改成网页轮询自己主动定期问一次 PEM
	 * 状态,复用同一套专属 mailbox + rqSeq 校验机制,可靠刷新,不再依赖"是否恰好被
	 * 自主推送过"。响应帧结构跟 ipmb_pem_pkt_t 一致(buf[9]/[10]/[11] 同样的偏移量),
	 * 借一个局部 ipmb_pem_pkt_t 直接复用现成的 Ipmb_SlotCache_StorePem。 */
	rqseq = s_rqseq;
	IPMB_Build_PlatformEventPoll(&pkt, target_addr, IPMB_WEB_HOST_ADDR_7BIT, rqseq);
	s_rqseq = (uint8_t)((s_rqseq + 1) & 0x3F);
	if (slot_poll_send_wait(&pkt, &rsp, rqseq)) {
		ipmb_pem_pkt_t pem_view;
		uint8_t copy_len = (rsp.len > IPMB_PEM_BUF_SIZE) ? IPMB_PEM_BUF_SIZE : rsp.len;
		pem_view.len = copy_len;
		memcpy(pem_view.buf, rsp.buf, copy_len);
		Ipmb_SlotCache_StorePem(target_addr, &pem_view);
	}

	for (i = 0; i < IPMB_SLOT_SENSOR_COUNT; i++) {
		uint8_t sensor_num = slot->sensors[i].sensor_num;
		uint8_t streak = slot->sensors[i].fail_streak;

		/* 【2026-08-20】指数退避:连续"真超时"的那几路(多半是物理没接的传感器)
		 * 每轮都要吃满 slot_poll_send_wait 的 2×400ms,是单轮耗时里最大的一块。
		 * 连续失败后按 1<<min(streak,3) 退到每 2/4/8 轮才试一次;一旦拿到响应
		 * 立刻清零恢复每轮采集,所以传感器接回来最多 8 轮就能自动跟上。
		 * 退避期直接 continue、不调用 StoreSensor(...,NULL):保留上次的
		 * valid/value,网页照旧显示灰色"未刷新",语义跟改动前一致。
		 * 注意只有 slot_poll_send_wait 返回 0(一条响应都没等到)才算失败;
		 * raw==0x7FFF 哨兵走的是"成功且很快"的响应路径,由 StoreSensor 内部置
		 * valid=0,不是开销来源,不能让它触发退避。 */
		if (streak > 0 && (s_round % (1UL << (streak > 3 ? 3 : streak))) != 0)
			continue;

		rqseq = s_rqseq;
		IPMB_Build_GetSensorReading(&pkt, target_addr, IPMB_WEB_HOST_ADDR_7BIT, rqseq, sensor_num);
		s_rqseq = (uint8_t)((s_rqseq + 1) & 0x3F);
		if (slot_poll_send_wait(&pkt, &rsp, rqseq)) {
			Ipmb_SlotCache_StoreSensor(slot, sensor_num, &rsp);
			slot->sensors[i].fail_streak = 0;
		} else {
			Ipmb_SlotCache_StoreSensor(slot, sensor_num, NULL);
			if (streak < 255) slot->sensors[i].fail_streak = (uint8_t)(streak + 1);
		}
	}

	/* 【2026-08-20】打印降频到每 4 轮一次:轮询周期从 3s 缩到 1s、且改成每轮扫全部
	 * 槽位之后,这条 printf 的输出量会涨到 N×3 倍。串口被占会拖慢 IPMB 响应转发
	 * (task_i2c.c 里探测响应特意绕开打印任务直送 mailbox 就是这个原因),这里主动
	 * 降频,保留可观测性但不给串口加压。 */
	if ((s_round & 3u) == 0)
		printf(">>[SLOT-POLL] addr=0x%02X slot=%u devid=%u board=%u online=%u\r\n",
			(unsigned)target_addr, (unsigned)slot->slot_id,
			(unsigned)slot->devid_valid, (unsigned)slot->boardstat_valid, (unsigned)slot->online);
}

void IPMB_Slot_Poll_Task(void *parameter)
{
	(void)parameter;

	for (;;)
	{
		uint8_t n, k;

		vTaskDelay(1000);

		n = g_slave_count;
		if (n == 0) continue;                       /* 尚未发现任何从机,本轮跳过 */
		if (n > IPMB_MAX_SLAVES) n = IPMB_MAX_SLAVES;   /* 防御:count 越界保护,同 task_usart.c 里的写法 */

		s_round++;

		/* 【2026-08-20】原来每轮只查 g_slave_addrs[s_rotation_idx] 一个槽位(轮转),
		 * 单槽刷新周期是 N×(3s+总线耗时),多槽位时网页每 3s 拉一次基本都拿到旧快照。
		 * 改成每轮把发现表里的槽位全查一遍,刷新周期变成 1s+N×总线耗时。
		 * 并发语义不变:n 在循环外快照一次,discovery 是先填 g_slave_addrs 再最后写
		 * g_slave_count(见 task_i2c.h 注释),读不到半张表。 */
		for (k = 0; k < n; k++)
			slot_poll_one(g_slave_addrs[k]);
	}
}
