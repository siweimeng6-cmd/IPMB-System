#ifndef __TASK_SLOT_CACHE_H
#define __TASK_SLOT_CACHE_H

#include "task_i2c.h"   /* ipmb_pkt_t, ipmb_pem_pkt_t, IPMB_MAX_SLAVES */

/*
*********************************************************************************************************
*	模 块 名: task_slot_cache.c
*	功能说明: 网页控制台改造(2026-07-27)新增——每槽位状态缓存。task_slot_poll.c(阶段2)作为
*	          唯一写者,后台轮询到的 Get Device ID/Get Board Status/Get Slot/Get Sensor Reading
*	          响应,以及 task_i2c.c 里 PEM 主动上报的解析结果,都存进这里;bsp_httpd.c(阶段3)
*	          作为读者直接渲染成 JSON,不用每次 HTTP 请求现发 IPMB 命令等待总线往返。
*	          单写者/多读者、不加锁,跟项目里 board_temp0/1/2、stADC_Data、旧的
*	          g_web_sensor_snapshot 同样的既有约定(见 task_web_sensor.h 注释)。
*	注    意: g_slot_cache 是静态 BSS 数组(IPMB_MAX_SLAVES=8 项),不占 FreeRTOS 堆。
*	          响应字节偏移量严格照抄仓库根目录 指令.txt 里①②③⑤节已用真实从机验证过的
*	          帧格式(比协议需求文档更贴近实测行为,冲突以 指令.txt 为准,细节见各 Store 函数注释)。
*********************************************************************************************************
*/

#define IPMB_SLOT_SENSOR_COUNT   7   /* 0x04,0x20,0x03,0x17,0x19,0x41,0x07 —— 指令.txt⑤节
                                        "目前接了真实硬件、能读到实际值的全部传感器" */

typedef struct {
	uint8_t sensor_num;
	int16_t value;   /* 【2026-07-28更正】从机固件已升级为2字节小端×100定点数(分辨率0.01,
	                   * 有符号),不再是 指令.txt⑤节描述的单字节截断整数——已用真实抓包对照从机
	                   * 源码(两个从机项目 ipmb_slave.c 的 IPMB_Slave_Handle_GetSensorReading)
	                   * 核实。这里存的是原始定点值(比如19.29V存作1929),真正的工程值单位
	                   * 换算(0x41风扇档位这一路是例外,本身就是整数,不参与×100)放在
	                   * bsp_httpd.c 渲染 JSON 时统一处理,缓存这里只做协议原始值的忠实转存。
	                   * raw==0x7FFF 视为"数据不可用"哨兵(来自 HKV-6UD2K-J2I_BMC 版本的约定)。 */
	uint8_t valid;   /* 最近一次轮询是否拿到有效响应(完成码=00、长度够、且不是0x7FFF哨兵) */
} IpmbSensorCache_t;

typedef struct {
	/* 身份/在线状态 */
	uint8_t  in_use;          /* 该槽位缓存条目是否已被某个发现到的地址占用 */
	uint8_t  addr;             /* IPMB 地址(0x30 + slot_id<<1) */
	uint8_t  slot_id;          /* 背板槽位号(0~31) */
	uint8_t  online;           /* 最近一轮后台轮询是否成功收到过至少一条响应 */
	uint32_t last_seen_tick;   /* 最近一次成功收到任意响应的 xTaskGetTickCount() */

	/* Get Device ID(01h),见 指令.txt①节 */
	uint8_t  devid_valid;
	uint8_t  device_id;
	uint8_t  hw_revision;      /* Payload硬件版本 */
	uint8_t  fw_major_rev;     /* Payload软件版本(固件大版本+bit7标志) */
	uint8_t  ipmc_sw_version;  /* IPMC软件版本(BCD) */
	uint8_t  ipmi_version;
	uint8_t  device_support;
	uint8_t  mfg_id[3];
	uint8_t  product_id[2];

	/* Get Board System Status(16h),见 指令.txt②节——已知只有 cpu_temp_raw/board_fw_version/
	 * power_state 是真实值,其余字段硬件限制恒为占位符(0xFF/0xFFFF),不是采集失败 */
	uint8_t  boardstat_valid;
	uint8_t  cpu_present;
	uint8_t  self_test;
	uint16_t cpu_util;
	uint16_t mem_util;
	uint16_t bw1;
	uint16_t bw2;
	uint8_t  cpu_temp_raw;     /* 真实值:LM75A实测温度+128 */
	uint8_t  os_version;
	uint8_t  board_fw_version; /* 真实值:借用IPMC软件版本号 */
	uint8_t  power_state;      /* 真实值:从机实时读PA4(PWR_CTL)电平,0=断电/1=上电,
	                             * 2026-08-17复用原保留字节新增,见 指令.txt②节 */

	/* Get Slot(15h),见 指令.txt③节——真实读取GA0~GA4硬件电平 */
	uint8_t  slot_valid;
	uint8_t  slot_readback;

	/* Get Sensor Reading(2Dh) x7,见 指令.txt⑤节 */
	IpmbSensorCache_t sensors[IPMB_SLOT_SENSOR_COUNT];

	/* PEM(Platform Event Message,主动上报或轮询答复),见 指令.txt⑦节
	 * 已知限制:从机侧传感器编号字段固定填00h,只反映全局系统状态变化,
	 * 不区分具体哪一路传感器越限 */
	uint8_t  pem_valid;
	uint8_t  pem_threshold_exceeded;
	uint8_t  pem_sys_state;
	uint8_t  pem_cause;
	uint32_t pem_last_tick;
} IpmbSlotCache_t;

extern IpmbSlotCache_t g_slot_cache[IPMB_MAX_SLAVES];

/* 按地址查找已有条目,没有则找一个空槽位分配(清零并预填 sensors[].sensor_num);
 * 发现表已满(8个不同地址都在用)时返回 NULL —— 调用方按现有 IPMB_MAX_SLAVES=8 上限处理 */
IpmbSlotCache_t* Ipmb_SlotCache_FindOrAlloc(uint8_t addr);

/* 按背板槽位号查找,找不到(未发现/不在线)返回 NULL,供 bsp_httpd.c 用 */
IpmbSlotCache_t* Ipmb_SlotCache_FindBySlotId(uint8_t slot_id);

/* 解析响应帧存入缓存,rsp 是 Get Device ID/Board Status/Slot/Sensor Reading 的完整响应帧;
 * 完成码非0或长度不够时对应 *_valid 置0,不覆盖旧数值(避免瞬时超时把上次的有效读数冲掉) */
void Ipmb_SlotCache_StoreDeviceID(IpmbSlotCache_t *slot, const ipmb_pkt_t *rsp);
void Ipmb_SlotCache_StoreBoardStatus(IpmbSlotCache_t *slot, const ipmb_pkt_t *rsp);
void Ipmb_SlotCache_StoreSlotReadback(IpmbSlotCache_t *slot, const ipmb_pkt_t *rsp);
void Ipmb_SlotCache_StoreSensor(IpmbSlotCache_t *slot, uint8_t sensor_num, const ipmb_pkt_t *rsp);

/* 存一条 PEM 事件(主动上报或轮询答复都能用),addr=帧里的源从机地址(pem->buf[3]);
 * 找不到对应槽位缓存条目(地址还没被 Get Device ID 等命令 FindOrAlloc 过)时按 addr 现分配一个 */
void Ipmb_SlotCache_StorePem(uint8_t addr, const ipmb_pem_pkt_t *pem);

/* 7个已知有真实读数的传感器编号对应的名称/单位,供 bsp_httpd.c 渲染 JSON 用;
 * 传入未知 sensor_num 返回 NULL/空字符串 */
const char* Ipmb_SensorName(uint8_t sensor_num);
const char* Ipmb_SensorUnit(uint8_t sensor_num);

#endif
