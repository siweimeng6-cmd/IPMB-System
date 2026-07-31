#ifndef __TASK_CTRL_DISPATCH_H
#define __TASK_CTRL_DISPATCH_H

#include "task_i2c.h"   /* ipmb_pkt_t */

/*
*********************************************************************************************************
*	模 块 名: task_ctrl_dispatch.c
*	功能说明: 网页控制台改造(2026-07-27)新增——异步控制命令通道。lwIP raw API 的
*	          http_recv 回调跑在 tcpip 线程里,不能阻塞等 IPMB 总线往返(双总线failover
*	          最坏情况要几百毫秒),所以 POST /api/v1/slots/{n}/control 只做"入队即回":
*	          Ipmb_Ctrl_Submit() 把请求丢进 xIPMB_CtrlReqQueue 立刻返回一个 ticket 号,
*	          真正的 IPMB 往返由本文件的 IPMB_Ctrl_Dispatch_Task 在自己的任务上下文里
*	          完成,结果写进 s_results[] 环形结果表,前端拿 ticket 号轮询
*	          GET /api/v1/tickets/{n} 直到 status 变成 done/timeout。
*	注    意: 单写者时序:Ipmb_Ctrl_Submit(tcpip线程)先把该 ticket 槽位置成 PENDING,
*	          再把请求送入队列——dispatch 任务只可能在出队(即 PENDING 状态已经落盘)之后
*	          才回写同一个槽位,两者不会真的同时写同一块内存,不需要加锁,跟项目里
*	          "单写者不加锁"的既有约定一致(见 task_web_sensor.h/task_slot_cache.h 注释)。
*	          目标是远端从机 FRU 板,跟主控本机的电源按钮/本地GPIO逻辑(bsp_gpio.c)无关。
*********************************************************************************************************
*/

#define IPMB_CTRL_HOST_ADDR_7BIT   0x05   /* 新的奇数标记:0x00=发现探测,0x01=网页轮询,已占用 */
#define IPMB_CTRL_MAX_TICKETS      8
#define IPMB_CTRL_DATA_MAX         4

/* tcpip线程(Ipmb_Ctrl_Submit) -> IPMB_Ctrl_Dispatch_Task 的请求描述,定义放头文件里
 * (而不是藏进 task_ctrl_dispatch.c 内部),方便 bsp_init.c 用 sizeof() 创建
 * xIPMB_CtrlReqQueue,跟 ipmb_pkt_t/ipmb_pem_pkt_t 都是公开结构体的既有风格一致 */
typedef struct {
	uint8_t  target_addr;
	uint8_t  control_req;
	uint8_t  data[IPMB_CTRL_DATA_MAX];
	uint8_t  data_len;
	uint16_t ticket;
} IpmbCtrlReq_t;

typedef enum {
	IPMB_CTRL_PENDING = 0,
	IPMB_CTRL_DONE,
	IPMB_CTRL_TIMEOUT
} IpmbCtrlStatus_t;

typedef struct {
	uint16_t ticket;
	uint8_t  in_use;
	uint8_t  control_req;
	uint8_t  completion_code;   /* 只在 status==IPMB_CTRL_DONE 时有意义,00h=成功 */
	IpmbCtrlStatus_t status;
	uint32_t submitted_tick;
	uint32_t completed_tick;
} IpmbCtrlResult_t;

extern xQueueHandle xIPMB_CtrlReqQueue;    /* tcpip线程 -> dispatch任务,深度4 */
extern xQueueHandle xIPMB_CtrlRspQueue;    /* dispatch任务专属响应mailbox,I2C1侧,深度1 */
extern xQueueHandle xIPMB_CtrlRspQueue2;   /* 同上,I2C2侧 */

/* 提交一条 Set FRU Activation 控制命令(control_req 见 指令.txt⑧节/419-434行);
 * data/data_len 只有 control_req=0x07(1字节档位)/0x08(2字节rpm,低字节在前)/
 * 0x09(1字节BCD版本)需要,其余传 NULL/0。非阻塞,tcpip线程直接调用。
 * 返回值: >0 = ticket号(供 Ipmb_Ctrl_GetResult 轮询), 0 = 提交失败(队列满/参数非法) */
uint16_t Ipmb_Ctrl_Submit(uint8_t target_addr, uint8_t control_req, const uint8_t *data, uint8_t data_len);

/* 查询某个 ticket 的当前结果;ticket 不存在/已被环形表覆盖返回 NULL */
const IpmbCtrlResult_t* Ipmb_Ctrl_GetResult(uint16_t ticket);

extern TaskHandle_t IPMB_Ctrl_Dispatch_Task_Handle;
void IPMB_Ctrl_Dispatch_Task(void *parameter);

#endif
