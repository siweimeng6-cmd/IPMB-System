#include "task_ctrl_dispatch.h"
#include "task_i2c.h"
#include "task_slot_cache.h"   /* 2026-07-29新增:设置IPMC版本成功后清devid_valid,见下方完成分支 */
#include "ipmb_request_builder.h"
#include "queue.h"

xQueueHandle xIPMB_CtrlReqQueue = NULL;
xQueueHandle xIPMB_CtrlRspQueue = NULL;
xQueueHandle xIPMB_CtrlRspQueue2 = NULL;

TaskHandle_t IPMB_Ctrl_Dispatch_Task_Handle = NULL;

static IpmbCtrlResult_t s_results[IPMB_CTRL_MAX_TICKETS] = {0};
static uint16_t s_next_ticket = 1;   /* 0 保留,表示"无效/提交失败",不分配给真实请求 */
static uint8_t s_rqseq = 0;          /* 2026-07-29:从 IPMB_Ctrl_Dispatch_Task 局部变量提到文件作用域,
                                       * 供下面新抽出的 ctrl_dispatch_send_one() 也能共用同一个递增序号 */

uint16_t Ipmb_Ctrl_Submit(uint8_t target_addr, uint8_t control_req, const uint8_t *data, uint8_t data_len)
{
	IpmbCtrlReq_t req;
	uint16_t ticket;
	uint8_t slot_idx, i;

	if (data_len > IPMB_CTRL_DATA_MAX) return 0;
	if (xIPMB_CtrlReqQueue == NULL) return 0;

	ticket = s_next_ticket;
	slot_idx = (uint8_t)(ticket % IPMB_CTRL_MAX_TICKETS);

	/* 先落 PENDING 状态,再入队——保证 dispatch 任务(哪怕入队后立刻被调度处理完)
	 * 不会在这条记录还没初始化好之前就抢先回写同一个槽位,见本文件头部注释 */
	s_results[slot_idx].ticket = ticket;
	s_results[slot_idx].in_use = 1;
	s_results[slot_idx].control_req = control_req;
	s_results[slot_idx].completion_code = 0xFF;
	s_results[slot_idx].status = IPMB_CTRL_PENDING;
	s_results[slot_idx].submitted_tick = xTaskGetTickCount();
	s_results[slot_idx].completed_tick = 0;

	req.target_addr = target_addr;
	req.req_kind = IPMB_CTRL_KIND_FRU;
	req.control_req = control_req;
	req.data_len = data_len;
	for (i = 0; i < data_len; i++) req.data[i] = data[i];
	req.ticket = ticket;

	/* 2026-07-30改:等待时间从10tick改成0——这里是lwIP的http_recv回调(tcpip线程
	 * 上下文),tcpip线程不能阻塞在这个队列上,即使只有10tick也不行,见排查
	 * "提交失败: {}"问题的计划文档 */
	if (xQueueSend(xIPMB_CtrlReqQueue, &req, (TickType_t)0) != pdTRUE) {
		s_results[slot_idx].status = IPMB_CTRL_TIMEOUT;   /* 队列满,提交失败,轮询能看到 */
		return 0;
	}

	s_next_ticket++;
	if (s_next_ticket == 0) s_next_ticket = 1;
	return ticket;
}

uint16_t Ipmb_BoardIdentity_Submit(uint8_t target_addr, uint8_t field_id, const uint8_t *value, uint8_t value_len)
{
	IpmbCtrlReq_t req;
	uint16_t ticket;
	uint8_t slot_idx, i;

	if (value_len > IPMB_CTRL_DATA_MAX) return 0;
	if (xIPMB_CtrlReqQueue == NULL) return 0;

	ticket = s_next_ticket;
	slot_idx = (uint8_t)(ticket % IPMB_CTRL_MAX_TICKETS);

	s_results[slot_idx].ticket = ticket;
	s_results[slot_idx].in_use = 1;
	s_results[slot_idx].control_req = field_id;
	s_results[slot_idx].completion_code = 0xFF;
	s_results[slot_idx].status = IPMB_CTRL_PENDING;
	s_results[slot_idx].submitted_tick = xTaskGetTickCount();
	s_results[slot_idx].completed_tick = 0;

	req.target_addr = target_addr;
	req.req_kind = IPMB_CTRL_KIND_BOARD_IDENTITY;
	req.control_req = field_id;
	req.data_len = value_len;
	for (i = 0; i < value_len; i++) req.data[i] = value[i];
	req.ticket = ticket;

	if (xQueueSend(xIPMB_CtrlReqQueue, &req, (TickType_t)0) != pdTRUE) {
		s_results[slot_idx].status = IPMB_CTRL_TIMEOUT;
		return 0;
	}

	s_next_ticket++;
	if (s_next_ticket == 0) s_next_ticket = 1;
	return ticket;
}

const IpmbCtrlResult_t* Ipmb_Ctrl_GetResult(uint16_t ticket)
{
	uint8_t slot_idx;
	if (ticket == 0) return NULL;
	slot_idx = (uint8_t)(ticket % IPMB_CTRL_MAX_TICKETS);
	if (!s_results[slot_idx].in_use || s_results[slot_idx].ticket != ticket) return NULL;
	return &s_results[slot_idx];
}

/*
*********************************************************************************************************
*	函 数 名: ctrl_dispatch_send_one
*	功能说明: 对单个目标地址发一条 Set FRU Activation 命令并等响应,原来是
*	          IPMB_Ctrl_Dispatch_Task 里唯一的一条路径,2026-07-29抽成独立函数——
*	          广播场景(control_req 0x10/0x11)需要对多个目标各调一次,单目标场景
*	          也复用同一份逻辑,避免维护两份构帧+等待+rqSeq校验代码。
*	          截止时间循环轮流查两路 mailbox,并校验 buf[4](rqSeq回显)跟这次请求
*	          匹配才接受,不匹配当陈旧响应丢弃继续等——同 task_slot_poll.c 里
*	          slot_poll_send_wait 一类竞态考虑,外层重试1次的预算结构不变。
*	形    参: req_kind: IpmbCtrlKind_t,决定 control_req 的含义和构帧函数
*	          target_addr/control_req/data/data_len: 同 Ipmb_Ctrl_Submit/Ipmb_BoardIdentity_Submit
*	          out_cc: 成功时写入完成码(rsp.buf[6]),失败时不修改
*	返 回 值: 1=拿到匹配的响应(*out_cc有效)  0=两次重试后仍未拿到
*********************************************************************************************************
*/
static uint8_t ctrl_dispatch_send_one(uint8_t req_kind, uint8_t target_addr, uint8_t control_req,
                                        const uint8_t *data, uint8_t data_len, uint8_t *out_cc)
{
	ipmb_pkt_t pkt, rsp;
	uint8_t got_it = 0;
	uint8_t attempt, rqseq, expect_byte4;

	rqseq = s_rqseq;
	if (req_kind == IPMB_CTRL_KIND_BOARD_IDENTITY) {
		IPMB_Build_SetBoardIdentityField(&pkt, target_addr, IPMB_CTRL_HOST_ADDR_7BIT, rqseq,
		                                  control_req, data, data_len);
	} else {
		IPMB_Build_SetFruActivation(&pkt, target_addr, IPMB_CTRL_HOST_ADDR_7BIT, rqseq,
		                             control_req, data, data_len);
	}
	s_rqseq = (uint8_t)((s_rqseq + 1) & 0x3F);
	expect_byte4 = (uint8_t)(rqseq << 2);   /* lun固定0 */

	for (attempt = 0; attempt < 2 && !got_it; attempt++) {
		TickType_t deadline;
		BaseType_t queued_a = pdFALSE;
		BaseType_t queued_b = pdFALSE;

		/* 控制命令不能排在后台发现/状态/传感器轮询之后，否则深度 6 的 FIFO
		 * 最坏会积压数百毫秒，控制层先判超时而命令稍后才真正执行。
		 * dispatch 运行在独立任务中，可以等待队列腾位；确认至少一路入队成功后
		 * 才启动响应计时，避免原来忽略 xQueueSend 失败造成的必然超时。 */
		if (xIPMB_CmdQueue != NULL)
			queued_a = xQueueSendToFront(xIPMB_CmdQueue, &pkt, pdMS_TO_TICKS(200));
		if (xIPMB_CmdQueue2 != NULL)
			queued_b = xQueueSendToFront(xIPMB_CmdQueue2, &pkt, pdMS_TO_TICKS(200));

		if (queued_a != pdTRUE && queued_b != pdTRUE)
			continue;

		/* 底层单次回包预算为 300ms；这里额外覆盖任务切换和响应 mailbox 转发。 */
		deadline = xTaskGetTickCount() + pdMS_TO_TICKS(700);
		while (!got_it && (int32_t)(deadline - xTaskGetTickCount()) > 0) {
			if (xIPMB_CtrlRspQueue != NULL &&
			    xQueueReceive(xIPMB_CtrlRspQueue, &rsp, (TickType_t)20) == pdTRUE) {
				if (rsp.len >= 5 && rsp.buf[4] == expect_byte4) { got_it = 1; break; }
				continue;
			}
			if (xIPMB_CtrlRspQueue2 != NULL &&
			    xQueueReceive(xIPMB_CtrlRspQueue2, &rsp, (TickType_t)20) == pdTRUE) {
				if (rsp.len >= 5 && rsp.buf[4] == expect_byte4) { got_it = 1; break; }
				continue;
			}
		}
	}

	if (got_it && rsp.len >= 8) {
		*out_cc = rsp.buf[6];
		return 1;
	}
	return 0;
}

void IPMB_Ctrl_Dispatch_Task(void *parameter)
{
	IpmbCtrlReq_t req;
	(void)parameter;

	for (;;)
	{
		if (xQueueReceive(xIPMB_CtrlReqQueue, &req, portMAX_DELAY) == pdTRUE)
		{
			uint8_t slot_idx = (uint8_t)(req.ticket % IPMB_CTRL_MAX_TICKETS);

			/* 【2026-08-20】广播分支只对 FRU 控制命令有意义(切总线速率);board identity
			 * 写入的 control_req 字节被借用来装 field_id,数值上可能撞到 0x10/0x11,
			 * 必须先判 req_kind 排除,否则一次板卡身份写入会被误当成总线速率广播。 */
			if (req.req_kind == IPMB_CTRL_KIND_FRU &&
			    (req.control_req == 0x10 || req.control_req == 0x11)) {
				/* 广播:见 bsp_i2c.c Ipmb_SetBusSpeed 注释——I2C_Speed 是整条总线共用的,
				 * 只切一个从机会把没切到的从机直接带崩(接收方时序容差跟总线实际速率
				 * 不匹配,不是降级是失联)。这条命令要发给当前发现表里的每一个从机,
				 * 全部确认成功才提交切换;只要有一个没确认成功就整体判失败、不切换,
				 * 避免"部分从机新速率、部分从机+主控还是旧速率"的半切换状态。 */
				uint8_t n = g_slave_count;
				uint8_t i, all_ok = 1;
				uint8_t any_slave = (n > 0);
				if (n > IPMB_MAX_SLAVES) n = IPMB_MAX_SLAVES;

				for (i = 0; i < n; i++) {
					uint8_t cc = 0xFF;
					if (!ctrl_dispatch_send_one(IPMB_CTRL_KIND_FRU, g_slave_addrs[i], req.control_req,
					                             req.data, req.data_len, &cc) ||
					    cc != 0x00) {
						all_ok = 0;   /* 不提前退出,继续把剩下的从机也发一遍,方便串口日志排查是谁没响应 */
					}
				}

				if (s_results[slot_idx].ticket == req.ticket) {
					if (any_slave && all_ok) {
						s_results[slot_idx].completion_code = 0x00;
						s_results[slot_idx].status = IPMB_CTRL_DONE;
						if (req.control_req == 0x10) Ipmb_SetBusSpeed(100000);
						else Ipmb_SetBusSpeed(400000);
					} else {
						s_results[slot_idx].completion_code = 0xFF;
						s_results[slot_idx].status = IPMB_CTRL_TIMEOUT;
					}
					s_results[slot_idx].completed_tick = xTaskGetTickCount();
				}
			} else {
				uint8_t cc = 0xFF;
				uint8_t ok = ctrl_dispatch_send_one(req.req_kind, req.target_addr, req.control_req,
				                                     req.data, req.data_len, &cc);

				if (s_results[slot_idx].ticket == req.ticket) {   /* 防御:环形表理论上不会在此期间被复用,仍加个校验 */
					if (ok) {
						s_results[slot_idx].completion_code = cc;
						s_results[slot_idx].status = IPMB_CTRL_DONE;
						if (req.req_kind == IPMB_CTRL_KIND_BOARD_IDENTITY) {
							/* 【2026-08-20】板卡身份字段写入成功——control_req 这里借用的是
							 * field_id,不能跟下面 FRU 分支的 0x09(数值上会跟 field_id=9
							 * 即"主板型号"撞车)混在一起判断,必须先分 req_kind。写成功后清
							 * 对应的缓存有效位,让下一轮轮询(task_slot_poll.c)重新拉取新值,
							 * field_id 0~6 走 Get Device ID(清 devid_valid),7~10 走新增的
							 * Get Board Identity Field(清 custom_valid)。 */
							if (cc == 0x00) {
								IpmbSlotCache_t *slot = Ipmb_SlotCache_FindOrAlloc(req.target_addr);
								if (slot != NULL) {
									if (req.control_req <= 6) slot->devid_valid = 0;
									else slot->custom_valid = 0;
								}
							}
						} else if (req.control_req == 0x09 && cc == 0x00) {
							/* 设置IPMC版本成功——Get Device ID现在跟Board Status共用
							 * 同一个变量了(2026-07-29),但常规轮询(task_slot_poll.c)
							 * 只在槽位首次发现、devid_valid==0 时才拉一次 Get Device ID,
							 * 平时不会重新问。这里主动清掉devid_valid,让下一轮轮询
							 * 重新拉一次,网页Device ID卡片才能跟着刷新,不用等主控
							 * 重启重新发现这个槽位 */
							IpmbSlotCache_t *slot = Ipmb_SlotCache_FindOrAlloc(req.target_addr);
							if (slot != NULL) slot->devid_valid = 0;
						}
					} else {
						s_results[slot_idx].status = IPMB_CTRL_TIMEOUT;
					}
					s_results[slot_idx].completed_tick = xTaskGetTickCount();
				}
			}
		}
	}
}
