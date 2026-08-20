#ifndef __TASK_I2C_H
#define	__TASK_I2C_H

#include "stm32f4xx.h"
#include <stdio.h>
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "queue.h"

#include "bsp_i2c.h"
#include "bsp_adc.h"
#include "bsp_mo_i2c.h"



extern TaskHandle_t I2C1_Task_Handle ;
extern TaskHandle_t I2C2_Task_Handle ;
extern TaskHandle_t Temp_Task_Handle ;
extern TaskHandle_t IPMB_Test_Cmd_Task_Handle ;
extern xTaskHandle I2C1ReceiveSemaphore ;
extern xTaskHandle I2C2ReceiveSemaphore ;

/* 自测任务与主机任务之间的命令/响应队列
 * 队列 1 → I2C1_Task(IPMB-A 主机);队列 2 → I2C2_Task(IPMB-B 主机)
 * 同一条 ipmb/ipmboem 命令会同时入队两条,实现两路主机同发 */
extern xQueueHandle xIPMB_CmdQueue;
extern xQueueHandle xIPMB_RspQueue;
extern xQueueHandle xIPMB_CmdQueue2;
extern xQueueHandle xIPMB_RspQueue2;

/* 地址发现任务专属响应 mailbox(深度1), 由 IPMB_Test_Command_Task 从
 * xIPMB_RspQueue/2 中识别出探测响应后转发进来, 避免和用户手动命令的
 * 回显互相争抢同一个 mailbox */
extern xQueueHandle xIPMB_DiscRspQueue;
extern xQueueHandle xIPMB_DiscRspQueue2;

/* 网页监控轮询专用的 rqSA 标记,机制跟 IPMB_DISC_HOST_ADDR_7BIT 完全一样:
 * I2C1_Task/I2C2_Task 识别出响应的 rqSA(buf[0])等于这个值,就转发到下面
 * 这个专属 mailbox,不进 xIPMB_RspQueue/2(避免跟 IPMB_Test_Command_Task
 * 抢同一条响应)。取奇数 0x01——本系统里所有真实IPMB地址都是7bit左移1位
 * 得到,必为偶数,用奇数保证不会跟任何真实地址混淆。 */
#define IPMB_WEB_HOST_ADDR_7BIT   0x01
extern xQueueHandle xIPMB_WebRspQueue;
extern xQueueHandle xIPMB_WebRspQueue2;

/* I2C1/I2C2_SendRequest 等从机主动回写响应的预算,按请求来源(rqSA 标记,即
 * 帧的 buf[3])分级——同一个字节已经被用来做响应分流,这里复用同一套约定。
 * 分级的理由:预算只在"从机没按时回包"时才真正花掉,而这三类请求对慢响应的
 * 容忍度完全不同。控制命令(开关机/复位)从机侧要真的执行动作,回包慢属正常,
 * 必须给够,否则网页"最近操作"会显示超时;后台轮询和发现探测则是高频、
 * 可容忍单次丢失的,让它们也陪等 300ms 会把深度只有 6 的命令队列堵住,
 * 表现为网页传感器长时间"未刷新"。 */
#define IPMB_BUDGET_CTRL_MS      300   /* 控制命令(rqSA=IPMB_CTRL_HOST_ADDR_7BIT) */
#define IPMB_BUDGET_POLL_MS      150   /* 网页槽位轮询(rqSA=IPMB_WEB_HOST_ADDR_7BIT) */
#define IPMB_BUDGET_DISC_MS       80   /* 地址发现探测(rqSA=IPMB_DISC_HOST_ADDR_7BIT) */
#define IPMB_BUDGET_DEFAULT_MS   300   /* 串口手动命令等其余来源 */

/* PEM(Platform Event Message)主动上报专用队列:从机在非请求窗口主动 write
 * 过来的帧(is_pem_push_frame() 判定命中)转发到这里。与 xIPMB_RspQueue/2
 * (深度5 FIFO、xQueueSend,突发多条响应排队不覆盖)分开,PEM 事件用
 * xQueueSend 入队,避免连续事件被覆盖丢失。
 * PEM 帧固定 14 字节,专用一个小结构(不复用 129 字节的 ipmb_pkt_t),把队列
 * 堆占用降到几乎为零。 */
#define IPMB_PEM_BUF_SIZE   16
typedef struct {
	uint8_t len;
	uint8_t buf[IPMB_PEM_BUF_SIZE];
} ipmb_pem_pkt_t;
extern xQueueHandle xIPMB_PemQueue;
extern xQueueHandle xIPMB_PemQueue2;

/* PEM 自动轮询上报总开关: 1=每5s自动轮询并打印(默认,保持现状),
 * 0=关闭自动轮询,串口不再周期刷 <RSP-B ...02...> 自动上报。
 * 只关"自动发起",不影响手动 ipmb 3x ... 02 查询(仍正常打印)。 */
#ifndef IPMB_PEM_AUTO_POLL_ENABLE
#define IPMB_PEM_AUTO_POLL_ENABLE   0
#endif

/* 判断一帧是否是 PEM 主动上报/轮询答复帧形状(从机按 IPMB_Build_Request
 * 风格拼帧:netFn=04h 原始值不左移、不带响应位,cmd=0x02),且校验和通过 */
uint8_t is_pem_push_frame(const uint8_t *buf, uint16_t len);

/* 业务板卡 IPMB 地址(7bit), 由 IPMB_Discovery_Task 按槽位动态发现后写入,
 * 发现完成前保留历史默认值 0x8E 兜底。多从机场景下 = 发现表第一个地址,
 * 仅供 ipmboem 自动构帧沿用(单目标),PEM 轮询改用下面的发现表遍历所有从机。 */
extern volatile uint8_t g_slave_ipmb_addr;

/* 发现地址表: IPMB_Discovery_Task 每轮把总线上所有应答的从机地址(去重)记进
 * g_slave_addrs[0..g_slave_count-1], 供 IPMB_PEM_Poll_Task 逐个轮询。
 * 单核 MCU 上发现任务先填 g_slave_addrs 再最后写 g_slave_count, 保证轮询任务
 * 不会读到半张表。 */
#define IPMB_MAX_SLAVES   8
extern volatile uint8_t g_slave_addrs[IPMB_MAX_SLAVES];
extern volatile uint8_t g_slave_count;

/* 主机端当前活跃总线(A优先/失败切B/恢复切回A,见 task_i2c.c 顶部仲裁说明)。
 * 定义放这里(而不是只留在 task_i2c.c 内)方便 bsp_httpd.c 的 /api/v1/health
 * 端点直接读取展示,不用额外加一层封装函数。 */
typedef enum { HOST_BUS_A = 0, HOST_BUS_B } Host_Bus_t;
extern volatile Host_Bus_t g_host_active_bus;

/* 从机 IPMB 地址 <-> 背板槽位号互换(公式见 IPMB_Discovery_Task 注释:
 * addr = 0x30 + (Slot_ID<<1),Slot_ID 0~31)。网页控制台改造(2026-07-27)新增,
 * 供 HTTP 层把 URL 里的槽位号换算成目标地址、把发现表地址换算成槽位号用。 */
#define Ipmb_SlotToAddr(slot)   ((uint8_t)(0x30 + ((slot) << 1)))
#define Ipmb_AddrToSlot(addr)   ((uint8_t)(((addr) - 0x30) >> 1))

/* OEM Get Module Info 单 item 响应最大 3+80=83 字节,双 item ~115 字节,
 * 缓冲需容纳整帧 + 回显,故从 96 提升到 128 */
#define IPMB_TEST_BUF_SIZE   128

typedef struct {
	uint8_t len;
	uint8_t buf[IPMB_TEST_BUF_SIZE];
} ipmb_pkt_t;

void I2C1_Task(void* parameter);
void I2C2_Task(void* parameter);
void Temp_Task(void* parameter);
void IPMB_Test_Command_Task(void* parameter);
void IPMB_Discovery_Task(void* parameter);
extern TaskHandle_t IPMB_Discovery_Task_Handle;

/* 风扇占空比:改由串口 fan <0-100> 命令手动设置,4 路共用一个值,不再按温度调速。
 * g_fan_duty 保存当前设定值,Temp_Task 每周期把它下发到 4 路 PWM。 */
extern uint16_t g_fan_duty;

/* 设置 4 路风扇占空比(0-100,越界自动裁到 100)并立即下发,供串口 fan 命令调用 */
void Fan_Set_All_Duty(uint16_t duty);

/* 保护 stPrintf_Buf 在 Temp_Task / USART5_Task("?"查询) 之间互斥访问,
 * 声明放这里(而不是有循环 include 的 bsp_init.h)确保两边都能直接、可靠地看到 */
extern xSemaphoreHandle xPrintfBufMutex;


#endif
