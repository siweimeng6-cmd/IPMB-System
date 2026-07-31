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
			if (xIPMB_WebRspQueue != NULL &&
			    xQueueReceive(xIPMB_WebRspQueue, rsp_out, (TickType_t)20) == pdTRUE) {
				if (rsp_out->len >= 5 && rsp_out->buf[4] == expect_byte4) return 1;
				continue;   /* 陈旧/不匹配的响应,丢弃,用剩余时间预算继续等 */
			}
			if (xIPMB_WebRspQueue2 != NULL &&
			    xQueueReceive(xIPMB_WebRspQueue2, rsp_out, (TickType_t)20) == pdTRUE) {
				if (rsp_out->len >= 5 && rsp_out->buf[4] == expect_byte4) return 1;
				continue;
			}
		}
	}
	return 0;
}

void IPMB_Slot_Poll_Task(void *parameter)
{
	static uint8_t s_rqseq = 0;
	static uint8_t s_rotation_idx = 0;
	(void)parameter;

	for (;;)
	{
		uint8_t n, target_addr, i, rqseq;
		IpmbSlotCache_t *slot;
		ipmb_pkt_t pkt, rsp;

		vTaskDelay(3000);

		n = g_slave_count;
		if (n == 0) continue;                       /* 尚未发现任何从机,本轮跳过 */
		if (n > IPMB_MAX_SLAVES) n = IPMB_MAX_SLAVES;   /* 防御:count 越界保护,同 task_usart.c 里的写法 */
		if (s_rotation_idx >= n) s_rotation_idx = 0;

		target_addr = g_slave_addrs[s_rotation_idx];
		s_rotation_idx = (uint8_t)((s_rotation_idx + 1) % n);

		slot = Ipmb_SlotCache_FindOrAlloc(target_addr);
		if (slot == NULL) continue;   /* 8个缓存槽位已满,理论上和 g_slave_addrs 上限同步,不会发生 */

		/* Get Device ID / Get Slot 只在这个槽位第一次被发现(还没有有效数据)时各拉一次:
		 * 两者都是几乎静态的字段(槽位号只有物理热插拔到别的槽位才会变,罕见场景),
		 * 没必要每轮重复查询,省下的总线时间留给传感器/状态轮询 */
		if (!slot->devid_valid) {
			rqseq = s_rqseq;
			IPMB_Build_GetDeviceID(&pkt, target_addr, IPMB_WEB_HOST_ADDR_7BIT, rqseq);
			s_rqseq = (uint8_t)((s_rqseq + 1) & 0x3F);
			if (slot_poll_send_wait(&pkt, &rsp, rqseq)) Ipmb_SlotCache_StoreDeviceID(slot, &rsp);
		}

		if (!slot->slot_valid) {
			rqseq = s_rqseq;
			IPMB_Build_GetSlot(&pkt, target_addr, IPMB_WEB_HOST_ADDR_7BIT, rqseq);
			s_rqseq = (uint8_t)((s_rqseq + 1) & 0x3F);
			if (slot_poll_send_wait(&pkt, &rsp, rqseq)) Ipmb_SlotCache_StoreSlotReadback(slot, &rsp);
		}

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

			rqseq = s_rqseq;
			IPMB_Build_GetSensorReading(&pkt, target_addr, IPMB_WEB_HOST_ADDR_7BIT, rqseq, sensor_num);
			s_rqseq = (uint8_t)((s_rqseq + 1) & 0x3F);
			if (slot_poll_send_wait(&pkt, &rsp, rqseq))
				Ipmb_SlotCache_StoreSensor(slot, sensor_num, &rsp);
			else
				Ipmb_SlotCache_StoreSensor(slot, sensor_num, NULL);
		}

		printf(">>[SLOT-POLL] addr=0x%02X slot=%u devid=%u board=%u online=%u\r\n",
			(unsigned)target_addr, (unsigned)slot->slot_id,
			(unsigned)slot->devid_valid, (unsigned)slot->boardstat_valid, (unsigned)slot->online);
	}
}
