/*
*********************************************************************************************************
*	模 块 名: bsp_httpd.c
*	功能说明: 网页控制台改造(2026-07-27)重写——JSON API(80端口)。前端网页另行托管
*	          (PC/网关,跨域调用),本文件不再渲染 HTML,只负责路由 + JSON 序列化 + CORS。
*	          只读端点数据来自 task_slot_cache.c 的 g_slot_cache[](由 task_slot_poll.c
*	          后台轮询写入),本文件读取时不发起任何 IPMB 命令,不阻塞 tcpip 线程;
*	          控制类端点(POST .../control)只把请求转交 task_ctrl_dispatch.c 异步处理,
*	          本文件同样不等待/不阻塞 IPMB 总线往返,原理见该文件头部说明。
*	端点列表:
*	  GET  /                          - API 说明
*	  GET  /api/v1/health             - 存活/运行状态
*	  GET  /api/v1/slots              - 已发现槽位列表
*	  GET  /api/v1/slots/{n}          - 槽位 n 全量快照(Device ID/Board Status/Slot/传感器/PEM)
*	  GET  /api/v1/slots/{n}/sensors  - 槽位 n 传感器
*	  GET  /api/v1/slots/{n}/pem      - 槽位 n 最近一次 PEM 状态
*	  POST /api/v1/slots/{n}/control  - 下发 Set FRU Activation 控制命令(见 指令.txt⑧节),
*	                                    body: {"control_req":N} 或 {"action":"power_on"},
*	                                    需要额外数据的 control_req(0x07/0x08/0x09)再加
*	                                    {"value":N};立即返回202+ticket,不等IPMB往返完成
*	  POST /api/v1/slots/{n}/identity - 写入一个板卡身份字段(2026-08-20新增,field_id 0~10,
*	                                    见 task_slot_cache.h 顶部注释),body: {"field_id":N,
*	                                    "value":V};field_id 0~4 的 value 是 0~255 整数,
*	                                    5~6(厂家ID/产品ID)是十六进制字符串(3/2字节),
*	                                    7~10(自定义文本字段)是普通文本(≤31字符);
*	                                    同样立即返回202+ticket,复用同一套 ticket 机制
*	  POST /api/v1/slots/{n}/threshold- 写入报警/断电阈值配置(2026-08-21新增,见
*	                                    task_slot_cache.h 顶部注释),body: 6个具名字段
*	                                    {"temp_alarm_high":N,"temp_alarm_low":N,
*	                                    "temp_shutdown_high":N,"temp_shutdown_low":N,
*	                                    "volt_alarm_percent":N,"volt_shutdown_percent":N},
*	                                    温度-128~127、百分比0~100;同样立即返回202+ticket
*	  GET  /api/v1/tickets/{n}        - 轮询上面某个 ticket 的执行结果
*	  OPTIONS *                       - CORS 预检
*	注    意: 每条 HTTP/1.0 请求都是一条新连接(Connection: close,无keep-alive/管线化),
*	          响应体在 s_httpRespBuf 里一次性生成好后一次 tcp_write 发送——新 accept 出来的
*	          pcb 从没发送过数据,tcp_sndbuf() 一定是满窗口(TCP_SND_BUF=5840B,见lwipopts.h),
*	          本文件所有响应都远小于这个数,不需要像长连接/大文件下载那样做分段发送+
*	          tcp_sent 回调续传的流控状态机,保持和原来的实现一样简单。若以后响应体可能
*	          超过 TCP_SND_BUF(比如 IPMB_MAX_SLAVES 大幅增加),需要重新引入这套机制。
*********************************************************************************************************
*/
#include "bsp_httpd.h"
#include "lwip/tcp.h"
#include "FreeRTOS.h"
#include "task.h"
#include "task_i2c.h"
#include "task_slot_cache.h"
#include "task_ctrl_dispatch.h"
#include <stdio.h>
#include <string.h>

/* 单线程(tcpip线程)顺序调用,不会并发,用 static 而不是栈上局部数组,
 * 避免占用 tcpip 线程的栈空间。3072B 覆盖最大的单槽位全量快照响应仍有富余
 * (2026-08-21从2048扩到3072:11个传感器+5个自定义文本字段的 handle_slot_detail
 * 响应已经逼近2048,这次再加阈值配置JSON块,需要更多余量) */
static char s_httpRespBuf[3072];

static err_t http_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
	/* 响应已经在 http_recv 里一次性 tcp_write+tcp_close 过了,这里无事可做,
	 * 保留这个回调只是因为 tcp_sent() 注册需要一个非NULL的合法函数指针 */
	return ERR_OK;
}

static void http_err(void *arg, err_t err)
{
	/* lwIP在调用这个回调前已经释放了pcb, 这里只是占位, 避免默认回调断言 */
}

/* 统一的响应头(含CORS),写入 buf,返回写入的字节数 */
static uint16_t http_write_headers(char *buf, int status, const char *status_text, const char *content_type)
{
	return (uint16_t)sprintf(buf,
		"HTTP/1.0 %d %s\r\n"
		"Content-Type: %s; charset=utf-8\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
		"Access-Control-Allow-Headers: Content-Type\r\n"
		"Access-Control-Max-Age: 600\r\n"
		"Connection: close\r\n"
		"\r\n",
		status, status_text, content_type);
}

static void http_send_and_close(struct tcp_pcb *pcb, uint16_t len)
{
	tcp_write(pcb, s_httpRespBuf, len, TCP_WRITE_FLAG_COPY);
	tcp_output(pcb);
	tcp_close(pcb);
}

static void handle_options(struct tcp_pcb *pcb)
{
	uint16_t len = (uint16_t)sprintf(s_httpRespBuf,
		"HTTP/1.0 204 No Content\r\n"
		"Access-Control-Allow-Origin: *\r\n"
		"Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
		"Access-Control-Allow-Headers: Content-Type\r\n"
		"Access-Control-Max-Age: 600\r\n"
		"Connection: close\r\n"
		"\r\n");
	http_send_and_close(pcb, len);
}

static void handle_not_found(struct tcp_pcb *pcb)
{
	uint16_t len = http_write_headers(s_httpRespBuf, 404, "Not Found", "application/json");
	len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"not found\"}");
	http_send_and_close(pcb, len);
}

static void handle_root(struct tcp_pcb *pcb)
{
	uint16_t len = http_write_headers(s_httpRespBuf, 200, "OK", "application/json");
	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"{\"service\":\"IPMB Web Console API\",\"version\":\"v1\",\"endpoints\":["
		"\"GET /api/v1/health\",\"GET /api/v1/slots\",\"GET /api/v1/slots/{n}\","
		"\"GET /api/v1/slots/{n}/sensors\",\"GET /api/v1/slots/{n}/pem\","
		"\"POST /api/v1/slots/{n}/control\",\"GET /api/v1/tickets/{n}\"]}");
	http_send_and_close(pcb, len);
}

static void handle_health(struct tcp_pcb *pcb)
{
	uint16_t len = http_write_headers(s_httpRespBuf, 200, "OK", "application/json");
	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"{\"uptime_ms\":%u,\"free_heap\":%u,\"active_bus\":\"%s\",\"slot_count\":%u,\"i2c_speed_hz\":%lu}",
		(unsigned)(xTaskGetTickCount() * portTICK_PERIOD_MS),
		(unsigned)xPortGetFreeHeapSize(),
		(g_host_active_bus == HOST_BUS_A) ? "A" : "B",
		(unsigned)g_slave_count,
		(unsigned long)I2C_Speed);
	http_send_and_close(pcb, len);
}

/* 2026-07-30新增:slot->online 曾经是"轮询成功过一次就永久置1、从没有清0的代码路径"
 * 的粘性标志——从机被拔掉后,IPMB_Discovery_Task 下一轮重扫会把这个地址从 g_slave_addrs
 * 里删掉,task_slot_poll.c 的轮询直接遍历这张表(task_slot_poll.c:70),再也不会去问这个
 * 地址,slot->online 就成了孤儿、永远停在1,网页一直显示"在线"(last_seen_ms 本身没问题,
 * 只是没人用它去纠正 online)。这里改成读取时用 last_seen_tick 的年龄现算,不管轮询任务
 * 还在不在问这个地址,网页都能正确随时间推移表现成离线,不用给 g_slot_cache 加第二个写者。 */
#define IPMB_SLOT_ONLINE_STALE_MS   40000
static uint8_t slot_is_online(const IpmbSlotCache_t *slot, uint32_t now)
{
	return (uint8_t)(((now - slot->last_seen_tick) * portTICK_PERIOD_MS) < IPMB_SLOT_ONLINE_STALE_MS);
}

static void handle_slots_list(struct tcp_pcb *pcb)
{
	uint8_t i, first = 1;
	uint32_t now = xTaskGetTickCount();
	uint16_t len = http_write_headers(s_httpRespBuf, 200, "OK", "application/json");
	len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"slots\":[");
	for (i = 0; i < IPMB_MAX_SLAVES; i++) {
		IpmbSlotCache_t *slot = &g_slot_cache[i];
		if (!slot->in_use) continue;
		len += (uint16_t)sprintf(s_httpRespBuf + len,
			"%s{\"slot_id\":%u,\"addr\":\"0x%02X\",\"online\":%u,\"last_seen_ms\":%u}",
			first ? "" : ",", (unsigned)slot->slot_id, (unsigned)slot->addr,
			(unsigned)slot_is_online(slot, now), (unsigned)((now - slot->last_seen_tick) * portTICK_PERIOD_MS));
		first = 0;
	}
	len += (uint16_t)sprintf(s_httpRespBuf + len, "],\"slot_count\":%u}", (unsigned)g_slave_count);
	http_send_and_close(pcb, len);
}

/* 【2026-07-28新增】把传感器原始定点值格式化成JSON数字文本。0x41(FanDuty)是从机
 * 直接给的整数百分比,不缩放;其余6路是×100定点(见 task_slot_cache.h 里
 * IpmbSensorCache_t.value 的说明),格式化成两位小数。手动拆整数/小数部分拼
 * "%d.%02d",不用浮点 printf(这套工具链对 %f 的支持没有把握,整数拼接更保险)。
 * buf 调用方保证至少16字节。 */
static void http_format_sensor_value(char *buf, uint8_t sensor_num, int16_t raw)
{
	int whole, frac;
	if (sensor_num == 0x41) {
		sprintf(buf, "%d", (int)raw);
		return;
	}
	whole = raw / 100;
	frac = raw % 100;
	if (frac < 0) frac = -frac;
	if (raw < 0 && whole == 0)
		sprintf(buf, "-%d.%02d", whole, frac);   /* 整数部分是0时单独补负号,避免"0.50"丢符号 */
	else
		sprintf(buf, "%d.%02d", whole, frac);
}

/* 【2026-08-20新增】把一个以'\0'结尾的字符串按 JSON 语法转义写进 out(转义引号/反斜杠/
 * ASCII控制字符,不做完整Unicode处理——自定义文本字段本来就只允许可打印ASCII,见从机
 * BoardIdentity_SetField 的校验,这里只是兜底防御)。in 为 NULL 或空串时 out 写成空串。
 * 返回写入的字节数(不含结尾'\0')。 */
static uint16_t http_json_escape_str(char *out, uint16_t out_cap, const char *in)
{
	uint16_t j = 0;
	if (in == NULL) { out[0] = '\0'; return 0; }
	while (*in != '\0' && (uint16_t)(j + 1) < out_cap) {
		unsigned char c = (unsigned char)*in;
		if (c == '\"' || c == '\\') {
			if ((uint16_t)(j + 2) >= out_cap) break;
			out[j++] = '\\';
			out[j++] = (char)c;
		} else if (c < 0x20) {
			if ((uint16_t)(j + 6) >= out_cap) break;
			j += (uint16_t)sprintf(&out[j], "\\u%04x", (unsigned)c);
		} else {
			out[j++] = (char)c;
		}
		in++;
	}
	out[j] = '\0';
	return j;
}

/* 【2026-07-28新增】见 指令.txt①节:device support 是固定值查表(93h=机箱管理CMHC,
 * 23h=功能板IPMC),不是位域,原样十进制显示没有意义,查表转成文字。 */
static const char* http_device_support_label(uint8_t v)
{
	switch (v) {
	case 0x93: return "chassis mgmt (CMHC)";
	case 0x23: return "function board (IPMC)";
	default:   return "unknown";
	}
}

static void handle_slot_detail(struct tcp_pcb *pcb, uint8_t slot_id)
{
	IpmbSlotCache_t *slot = Ipmb_SlotCache_FindBySlotId(slot_id);
	uint16_t len;
	uint8_t i;
	uint32_t now;

	if (slot == NULL) {
		len = http_write_headers(s_httpRespBuf, 404, "Not Found", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"slot %u not online\"}", (unsigned)slot_id);
		http_send_and_close(pcb, len);
		return;
	}

	now = xTaskGetTickCount();
	len = http_write_headers(s_httpRespBuf, 200, "OK", "application/json");

	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"{\"slot_id\":%u,\"addr\":\"0x%02X\",\"online\":%u,\"last_seen_ms\":%u,",
		(unsigned)slot->slot_id, (unsigned)slot->addr, (unsigned)slot_is_online(slot, now),
		(unsigned)((now - slot->last_seen_tick) * portTICK_PERIOD_MS));

	/* 见 指令.txt①节。hw_revision/fw_major_rev/ipmc_sw_version/ipmi_version 都是位域/BCD
	 * 编码,原样显示原始字节的十进制值(比如0x81显示成129)看不出实际含义——这里按协议
	 * 描述解码成"major.minor"人类可读形式,同时括号里保留16进制原始值方便协议级核对。
	 * hw_revision: bit7=是否支持SDR,bits[0:3]=硬件版本号(BCD)。
	 * fw_major_rev: bits[0:3]=固件大版本(BCD),bits[4:6]=固件小版本(BCD)。
	 * ipmc_sw_version/ipmi_version: 整字节BCD,低4位=大版本,高4位=小版本。 */
	{
		char hwrev_buf[24], fwrev_buf[24], ipmcver_buf[24], ipmiver_buf[24];
		sprintf(hwrev_buf, "%u%s (0x%02X)", (unsigned)(slot->hw_revision & 0x0F),
			(slot->hw_revision & 0x80) ? ",sdr" : "", (unsigned)slot->hw_revision);
		sprintf(fwrev_buf, "%u.%u (0x%02X)", (unsigned)(slot->fw_major_rev & 0x0F),
			(unsigned)((slot->fw_major_rev >> 4) & 0x07), (unsigned)slot->fw_major_rev);
		sprintf(ipmcver_buf, "%u.%u (0x%02X)", (unsigned)(slot->ipmc_sw_version & 0x0F),
			(unsigned)((slot->ipmc_sw_version >> 4) & 0x0F), (unsigned)slot->ipmc_sw_version);
		sprintf(ipmiver_buf, "%u.%u (0x%02X)", (unsigned)(slot->ipmi_version & 0x0F),
			(unsigned)((slot->ipmi_version >> 4) & 0x0F), (unsigned)slot->ipmi_version);

		len += (uint16_t)sprintf(s_httpRespBuf + len,
			"\"device_id\":{\"valid\":%u,\"device_id\":%u,\"hw_revision\":\"%s\",\"fw_major_rev\":\"%s\","
			"\"ipmc_sw_version\":\"%s\",\"ipmi_version\":\"%s\",\"device_support\":\"%s (0x%02X)\","
			"\"mfg_id\":\"%02X-%02X-%02X\",\"product_id\":\"%02X%02X\"},",
			(unsigned)slot->devid_valid, (unsigned)slot->device_id,
			hwrev_buf, fwrev_buf, ipmcver_buf, ipmiver_buf,
			http_device_support_label(slot->device_support), (unsigned)slot->device_support,
			(unsigned)slot->mfg_id[0], (unsigned)slot->mfg_id[1], (unsigned)slot->mfg_id[2],
			(unsigned)slot->product_id[0], (unsigned)slot->product_id[1]);
	}

	/* 见 指令.txt②节:cpu_present/self_test/cpu_util/mem_util/bw1/bw2/os_version 硬件限制恒为
	 * 占位符(0xFF/0xFFFF),不是采集失败;只有 cpu_temp_c/fw_version/power_state 是真实值。
	 * cpu_temp_valid=0 表示原始字节是0xFF(主机从未上报过CPU温度),此时 cpu_temp_c
	 * 解码出来的127无意义,前端应显示"未使用"而不是当成真实读数 */
	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"\"board_status\":{\"valid\":%u,\"cpu_present\":%u,\"self_test\":%u,"
		"\"cpu_util\":%u,\"mem_util\":%u,\"bw1\":%u,\"bw2\":%u,"
		"\"cpu_temp_c\":%d,\"cpu_temp_valid\":%u,\"os_version\":%u,\"fw_version\":%u,\"power_state\":%u},",
		(unsigned)slot->boardstat_valid, (unsigned)slot->cpu_present, (unsigned)slot->self_test,
		(unsigned)slot->cpu_util, (unsigned)slot->mem_util, (unsigned)slot->bw1, (unsigned)slot->bw2,
		(int)slot->cpu_temp_raw - 128, (unsigned)(slot->cpu_temp_raw != 0xFF),
		(unsigned)slot->os_version, (unsigned)slot->board_fw_version,
		(unsigned)slot->power_state);

	/* 见 指令.txt③节 */
	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"\"slot_readback\":{\"valid\":%u,\"slot_id\":%u},",
		(unsigned)slot->slot_valid, (unsigned)slot->slot_readback);

	/* 见 指令.txt⑤节:11个已知能读到真实值的传感器(2026-08-21从7个扩到11个);
	 * 数值编码见 http_format_sensor_value 注释 */
	len += (uint16_t)sprintf(s_httpRespBuf + len, "\"sensors\":[");
	for (i = 0; i < IPMB_SLOT_SENSOR_COUNT; i++) {
		IpmbSensorCache_t *sn = &slot->sensors[i];
		char valbuf[16];
		http_format_sensor_value(valbuf, sn->sensor_num, sn->value);
		len += (uint16_t)sprintf(s_httpRespBuf + len,
			"%s{\"sensor_num\":%u,\"name\":\"%s\",\"unit\":\"%s\",\"value\":%s,\"valid\":%u}",
			(i == 0) ? "" : ",", (unsigned)sn->sensor_num, Ipmb_SensorName(sn->sensor_num),
			Ipmb_SensorUnit(sn->sensor_num), valbuf, (unsigned)sn->valid);
	}
	len += (uint16_t)sprintf(s_httpRespBuf + len, "],");

	/* 见 指令.txt⑦节:从机侧传感器编号字段固定00h,PEM 只反映全局系统状态变化 */
	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"\"pem\":{\"valid\":%u,\"threshold_exceeded\":%u,\"sys_state\":%u,\"cause\":%u,\"age_ms\":%u},",
		(unsigned)slot->pem_valid, (unsigned)slot->pem_threshold_exceeded,
		(unsigned)slot->pem_sys_state, (unsigned)slot->pem_cause,
		(unsigned)((now - slot->pem_last_tick) * portTICK_PERIOD_MS));

	/* 板卡身份信息 field_id 7~11(2026-08-20新增,见 task_slot_cache.h):自定义文本字段,
	 * 用户自由输入,过 http_json_escape_str 转义后再拼进 JSON,防止内容里的双引号/
	 * 反斜杠/控制字符打断格式(从机 BoardIdentity_SetField 已经只允许可打印ASCII,
	 * 这里是第二层防御,兜住其它来源写入的旧数据/非常规字节)。os_version_text
	 * (field_id 11)跟真实的 board_status.os_version 协议字节没有关系,网页选择把它
	 * 展示在"系统状态"卡片里,这里只是原样透传。 */
	{
		char cpu_model_esc[96], mem_size_esc[96], board_model_esc[96], serial_number_esc[96], os_version_esc[96];
		http_json_escape_str(cpu_model_esc, sizeof(cpu_model_esc), slot->cpu_model);
		http_json_escape_str(mem_size_esc, sizeof(mem_size_esc), slot->mem_size);
		http_json_escape_str(board_model_esc, sizeof(board_model_esc), slot->board_model);
		http_json_escape_str(serial_number_esc, sizeof(serial_number_esc), slot->serial_number);
		http_json_escape_str(os_version_esc, sizeof(os_version_esc), slot->os_version_text);
		len += (uint16_t)sprintf(s_httpRespBuf + len,
			"\"custom_fields\":{\"valid\":%u,\"cpu_model\":\"%s\",\"mem_size\":\"%s\","
			"\"board_model\":\"%s\",\"serial_number\":\"%s\",\"os_version_text\":\"%s\"},",
			(unsigned)slot->custom_valid, cpu_model_esc, mem_size_esc, board_model_esc,
			serial_number_esc, os_version_esc);
	}

	/* 报警/断电阈值配置(2026-08-21新增,见 task_slot_cache.h):温度是绝对摄氏度,
	 * 电压是百分比,网页自己按标称值折算成实际范围显示 */
	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"\"threshold\":{\"valid\":%u,\"temp_alarm_high\":%d,\"temp_alarm_low\":%d,"
		"\"temp_shutdown_high\":%d,\"temp_shutdown_low\":%d,"
		"\"volt_alarm_percent\":%u,\"volt_shutdown_percent\":%u}}",
		(unsigned)slot->threshold_valid,
		(int)slot->temp_alarm_high, (int)slot->temp_alarm_low,
		(int)slot->temp_shutdown_high, (int)slot->temp_shutdown_low,
		(unsigned)slot->volt_alarm_percent, (unsigned)slot->volt_shutdown_percent);

	http_send_and_close(pcb, len);
}

static void handle_slot_sensors(struct tcp_pcb *pcb, uint8_t slot_id)
{
	IpmbSlotCache_t *slot = Ipmb_SlotCache_FindBySlotId(slot_id);
	uint16_t len;
	uint8_t i;

	if (slot == NULL) {
		len = http_write_headers(s_httpRespBuf, 404, "Not Found", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"slot %u not online\"}", (unsigned)slot_id);
		http_send_and_close(pcb, len);
		return;
	}

	len = http_write_headers(s_httpRespBuf, 200, "OK", "application/json");
	len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"slot_id\":%u,\"sensors\":[", (unsigned)slot->slot_id);
	for (i = 0; i < IPMB_SLOT_SENSOR_COUNT; i++) {
		IpmbSensorCache_t *sn = &slot->sensors[i];
		char valbuf[16];
		http_format_sensor_value(valbuf, sn->sensor_num, sn->value);
		len += (uint16_t)sprintf(s_httpRespBuf + len,
			"%s{\"sensor_num\":%u,\"name\":\"%s\",\"unit\":\"%s\",\"value\":%s,\"valid\":%u}",
			(i == 0) ? "" : ",", (unsigned)sn->sensor_num, Ipmb_SensorName(sn->sensor_num),
			Ipmb_SensorUnit(sn->sensor_num), valbuf, (unsigned)sn->valid);
	}
	len += (uint16_t)sprintf(s_httpRespBuf + len, "]}");
	http_send_and_close(pcb, len);
}

static void handle_slot_pem(struct tcp_pcb *pcb, uint8_t slot_id)
{
	IpmbSlotCache_t *slot = Ipmb_SlotCache_FindBySlotId(slot_id);
	uint16_t len;
	uint32_t now;

	if (slot == NULL) {
		len = http_write_headers(s_httpRespBuf, 404, "Not Found", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"slot %u not online\"}", (unsigned)slot_id);
		http_send_and_close(pcb, len);
		return;
	}

	now = xTaskGetTickCount();
	len = http_write_headers(s_httpRespBuf, 200, "OK", "application/json");
	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"{\"slot_id\":%u,\"valid\":%u,\"threshold_exceeded\":%u,\"sys_state\":%u,\"cause\":%u,\"age_ms\":%u,"
		"\"note\":\"slave does not identify which sensor channel tripped, global state only\"}",
		(unsigned)slot->slot_id, (unsigned)slot->pem_valid, (unsigned)slot->pem_threshold_exceeded,
		(unsigned)slot->pem_sys_state, (unsigned)slot->pem_cause,
		(unsigned)((now - slot->pem_last_tick) * portTICK_PERIOD_MS));
	http_send_and_close(pcb, len);
}

/* control_req(见 指令.txt⑧节/419-434行)的可读别名,POST body 里 "action" 字段用这个 */
typedef struct { const char *name; uint8_t control_req; } CtrlActionMap_t;
static const CtrlActionMap_t s_ctrl_actions[] = {
	{"power_off", 0x00}, {"power_on", 0x01}, {"reset", 0x02},
	{"soft_shutdown", 0x03}, {"soft_reset", 0x04},
	{"set_threshold_exceeded", 0x05}, {"set_threshold_normal", 0x06},
	{"set_fan_default_level", 0x07}, {"set_fan_max_rpm", 0x08},
	{"set_ipmc_version", 0x09},
	{"resume_temp_control", 0x0A},
	{"set_i2c_speed_100k", 0x10}, {"set_i2c_speed_400k", 0x11},
};

/* 0x07/0x08/0x10/0x11 这四个 control_req 在从机侧目前是已知的空实现(只存变量/只打印,
 * 没有真实硬件效果),见 指令.txt⑧节419-434行;控制响应里附带这个说明,别的 control_req
 * 返回空字符串 */
static const char* http_ctrl_note(uint8_t control_req)
{
	switch (control_req) {
	case 0x07: return "slave stub: value stored only, no PWM effect, see 指令.txt";
	case 0x08: return "slave stub: value stored only, no PWM effect, see 指令.txt";
	case 0x10: return "slave stub: logs only, does not switch I2C clock, see 指令.txt";
	case 0x11: return "slave stub: logs only, does not switch I2C clock, see 指令.txt";
	default: return "";
	}
}

/* 极简 JSON 取值:在 body 里找 "key" 后面的十进制整数/字符串值。够用即可,不是通用 JSON
 * 解析器——本机是受信管理网络内部 API,body 都是本文件自己文档里定义的简单固定形状 */
static uint8_t http_json_find_uint(const char *body, const char *key, uint32_t *out)
{
	const char *p = strstr(body, key);
	uint32_t v = 0;
	uint8_t n = 0;
	if (p == NULL) return 0;
	p += strlen(key);
	while (*p == ' ' || *p == ':') p++;
	while (p[n] >= '0' && p[n] <= '9') { v = v * 10 + (uint32_t)(p[n] - '0'); n++; if (n > 9) return 0; }
	if (n == 0) return 0;
	*out = v;
	return 1;
}

/* 跟 http_json_find_uint 同一个精神,够用即可:多支持一个可选前导'-',给报警/断电
 * 阈值里可以是负数的温度字段用(比如断电下限默认-10) */
static uint8_t http_json_find_int(const char *body, const char *key, int32_t *out)
{
	const char *p = strstr(body, key);
	int32_t v = 0;
	uint8_t neg = 0;
	uint8_t n = 0;
	if (p == NULL) return 0;
	p += strlen(key);
	while (*p == ' ' || *p == ':') p++;
	if (*p == '-') { neg = 1; p++; }
	while (p[n] >= '0' && p[n] <= '9') { v = v * 10 + (int32_t)(p[n] - '0'); n++; if (n > 9) return 0; }
	if (n == 0) return 0;
	*out = neg ? -v : v;
	return 1;
}

static uint8_t http_json_find_str(const char *body, const char *key, char *out, uint16_t out_cap)
{
	const char *p = strstr(body, key);
	uint16_t j;
	if (p == NULL) return 0;
	p += strlen(key);
	while (*p == ' ' || *p == ':') p++;
	if (*p != '\"') return 0;
	p++;
	j = 0;
	/* 【2026-08-20】遇到反斜杠时,原样多拷贝一个字符再继续——不做完整JSON反转义,
	 * 只是不让被转义的引号(\")误判成字符串结束。这里新增的自定义文本字段(CPU型号等)
	 * 是用户自由输入,含引号是完全合理的输入,原来的实现会在第一个 \" 处截断,
	 * 静默存错值(action 字段从没暴露过这个问题,因为动作名是固定枚举,从不含引号)。 */
	while (*p != '\"' && *p != '\0' && (uint16_t)(j + 1) < out_cap) {
		if (*p == '\\' && p[1] != '\0' && (uint16_t)(j + 2) < out_cap) {
			out[j++] = *p++;
		}
		out[j++] = *p++;
	}
	out[j] = '\0';
	return (j > 0) ? 1 : 0;
}

static void handle_slot_control(struct tcp_pcb *pcb, uint8_t slot_id, const char *body)
{
	IpmbSlotCache_t *slot = Ipmb_SlotCache_FindBySlotId(slot_id);
	uint16_t len;
	uint32_t control_req_val, value_val;
	uint8_t control_req = 0, have_req = 0;
	uint8_t data[IPMB_CTRL_DATA_MAX];
	uint8_t data_len = 0;
	uint16_t ticket;
	char action_name[32];
	const char *note;

	/* 2026-07-30新增诊断:排查"提交失败: {}"问题——这行打印能确认POST请求
	 * 是否真的到达了主控,跟GET数据采集接口对比,帮助判断问题在网络层还是IPMB层 */
	printf(">>[HTTPD] recv control POST: slot=%u\r\n", (unsigned)slot_id);

	if (slot == NULL) {
		len = http_write_headers(s_httpRespBuf, 404, "Not Found", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"slot %u not online\"}", (unsigned)slot_id);
		http_send_and_close(pcb, len);
		return;
	}

	if (http_json_find_uint(body, "\"control_req\"", &control_req_val) && control_req_val <= 0x11) {
		control_req = (uint8_t)control_req_val;
		have_req = 1;
	} else if (http_json_find_str(body, "\"action\"", action_name, sizeof(action_name))) {
		uint8_t i;
		for (i = 0; i < (uint8_t)(sizeof(s_ctrl_actions) / sizeof(s_ctrl_actions[0])); i++) {
			if (strcmp(action_name, s_ctrl_actions[i].name) == 0) {
				control_req = s_ctrl_actions[i].control_req;
				have_req = 1;
				break;
			}
		}
	}

	if (!have_req) {
		len = http_write_headers(s_httpRespBuf, 400, "Bad Request", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"missing/invalid control_req or action\"}");
		http_send_and_close(pcb, len);
		return;
	}

	if (http_json_find_uint(body, "\"value\"", &value_val)) {
		if (control_req == 0x08) {   /* 设风扇最大转速:2字节,低字节在前 */
			data[0] = (uint8_t)(value_val & 0xFF);
			data[1] = (uint8_t)((value_val >> 8) & 0xFF);
			data_len = 2;
		} else {   /* 0x07档位 / 0x09 IPMC版本号:1字节 */
			data[0] = (uint8_t)(value_val & 0xFF);
			data_len = 1;
		}
	}

	ticket = Ipmb_Ctrl_Submit(slot->addr, control_req, data, data_len);
	if (ticket == 0) {
		printf(">>[HTTPD] Ipmb_Ctrl_Submit failed (queue full)\r\n");
		len = http_write_headers(s_httpRespBuf, 503, "Service Unavailable", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"control queue full, retry later\"}");
		http_send_and_close(pcb, len);
		return;
	}

	note = http_ctrl_note(control_req);
	len = http_write_headers(s_httpRespBuf, 202, "Accepted", "application/json");
	if (note[0] != '\0')
		len += (uint16_t)sprintf(s_httpRespBuf + len,
			"{\"ticket\":%u,\"slot_id\":%u,\"control_req\":%u,\"note\":\"%s\"}",
			(unsigned)ticket, (unsigned)slot_id, (unsigned)control_req, note);
	else
		len += (uint16_t)sprintf(s_httpRespBuf + len,
			"{\"ticket\":%u,\"slot_id\":%u,\"control_req\":%u}",
			(unsigned)ticket, (unsigned)slot_id, (unsigned)control_req);
	http_send_and_close(pcb, len);
}

/* 把 s(以'\0'结尾)开头恰好 out_len*2 个十六进制字符解析成 out_len 个字节;
 * 长度不对(太短/太长)、出现非十六进制字符都返回0失败,不写 out */
static uint8_t http_parse_hex_bytes(const char *s, uint8_t *out, uint8_t out_len)
{
	uint8_t i;
	for (i = 0; i < out_len; i++) {
		uint8_t hi, lo;
		char c1 = s[i * 2], c2 = s[i * 2 + 1];
		if (c1 >= '0' && c1 <= '9') hi = (uint8_t)(c1 - '0');
		else if (c1 >= 'A' && c1 <= 'F') hi = (uint8_t)(c1 - 'A' + 10);
		else if (c1 >= 'a' && c1 <= 'f') hi = (uint8_t)(c1 - 'a' + 10);
		else return 0;
		if (c2 >= '0' && c2 <= '9') lo = (uint8_t)(c2 - '0');
		else if (c2 >= 'A' && c2 <= 'F') lo = (uint8_t)(c2 - 'A' + 10);
		else if (c2 >= 'a' && c2 <= 'f') lo = (uint8_t)(c2 - 'a' + 10);
		else return 0;
		out[i] = (uint8_t)((hi << 4) | lo);
	}
	if (s[(uint16_t)out_len * 2] != '\0') return 0;   /* 多余字符,长度不对 */
	return 1;
}

/*
*********************************************************************************************************
*	函 数 名: handle_slot_identity_set
*	功能说明: POST /api/v1/slots/{n}/identity(2026-08-20新增)——流程完全比照
*	          handle_slot_control:解析 body → Ipmb_BoardIdentity_Submit → 202+ticket,
*	          不等 IPMB 往返完成。body 里 value 的 JSON 类型按 field_id 分三种:
*	          0~4(数值字段)是整数,5~6(厂家ID/产品ID)是十六进制字符串,
*	          7~11(自定义文本字段,UTF-8,最长63字节)是普通文本,见 task_slot_cache.h 顶部注释。
*********************************************************************************************************
*/
static void handle_slot_identity_set(struct tcp_pcb *pcb, uint8_t slot_id, const char *body)
{
	IpmbSlotCache_t *slot = Ipmb_SlotCache_FindBySlotId(slot_id);
	uint16_t len;
	uint32_t field_id_val;
	uint8_t field_id;
	uint8_t data[64];
	uint8_t data_len = 0;
	uint16_t ticket;

	if (slot == NULL) {
		len = http_write_headers(s_httpRespBuf, 404, "Not Found", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"slot %u not online\"}", (unsigned)slot_id);
		http_send_and_close(pcb, len);
		return;
	}

	if (!http_json_find_uint(body, "\"field_id\"", &field_id_val) || field_id_val > 11) {
		len = http_write_headers(s_httpRespBuf, 400, "Bad Request", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"missing/invalid field_id (0-11)\"}");
		http_send_and_close(pcb, len);
		return;
	}
	field_id = (uint8_t)field_id_val;

	if (field_id <= 4) {
		uint32_t v;
		if (!http_json_find_uint(body, "\"value\"", &v) || v > 255) {
			len = http_write_headers(s_httpRespBuf, 400, "Bad Request", "application/json");
			len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"value must be an integer 0-255\"}");
			http_send_and_close(pcb, len);
			return;
		}
		data[0] = (uint8_t)v;
		data_len = 1;
	} else if (field_id == 5 || field_id == 6) {
		char hex[16];
		uint8_t want_len = (field_id == 5) ? 3 : 2;   /* 5=厂家ID(3字节) 6=产品ID(2字节) */
		if (!http_json_find_str(body, "\"value\"", hex, sizeof(hex)) ||
		    !http_parse_hex_bytes(hex, data, want_len)) {
			len = http_write_headers(s_httpRespBuf, 400, "Bad Request", "application/json");
			len += (uint16_t)sprintf(s_httpRespBuf + len,
				"{\"error\":\"value must be a %u-byte hex string\"}", (unsigned)want_len);
			http_send_and_close(pcb, len);
			return;
		}
		data_len = want_len;
	} else {
		char text[72];
		uint16_t text_len;
		if (!http_json_find_str(body, "\"value\"", text, sizeof(text))) {
			len = http_write_headers(s_httpRespBuf, 400, "Bad Request", "application/json");
			len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"value must be text\"}");
			http_send_and_close(pcb, len);
			return;
		}
		text_len = (uint16_t)strlen(text);
		if (text_len > 63) {
			len = http_write_headers(s_httpRespBuf, 400, "Bad Request", "application/json");
			len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"value too long, max 63 bytes\"}");
			http_send_and_close(pcb, len);
			return;
		}
		data_len = (uint8_t)text_len;
		memcpy(data, text, data_len);
	}

	ticket = Ipmb_BoardIdentity_Submit(slot->addr, field_id, data, data_len);
	if (ticket == 0) {
		len = http_write_headers(s_httpRespBuf, 503, "Service Unavailable", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"control queue full, retry later\"}");
		http_send_and_close(pcb, len);
		return;
	}

	len = http_write_headers(s_httpRespBuf, 202, "Accepted", "application/json");
	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"{\"ticket\":%u,\"slot_id\":%u,\"field_id\":%u}",
		(unsigned)ticket, (unsigned)slot_id, (unsigned)field_id);
	http_send_and_close(pcb, len);
}

/*
*********************************************************************************************************
*	函 数 名: handle_slot_threshold_set
*	功能说明: POST /api/v1/slots/{n}/threshold(2026-08-21新增)——流程比照
*	          handle_slot_identity_set:解析body(这次是6个具名字段一次性写全部配置,
*	          不是field_id+value单值形式)→ Ipmb_ThresholdConfig_Submit → 202+ticket,
*	          不等 IPMB 往返完成。body: {"temp_alarm_high":N,"temp_alarm_low":N,
*	          "temp_shutdown_high":N,"temp_shutdown_low":N,"volt_alarm_percent":N,
*	          "volt_shutdown_percent":N},温度字段允许负数(-128~127),百分比0~100。
*********************************************************************************************************
*/
static void handle_slot_threshold_set(struct tcp_pcb *pcb, uint8_t slot_id, const char *body)
{
	IpmbSlotCache_t *slot = Ipmb_SlotCache_FindBySlotId(slot_id);
	uint16_t len;
	int32_t  t_ah, t_al, t_sh, t_sl;
	uint32_t v_ap, v_sp;
	uint8_t  data[6];
	uint16_t ticket;

	if (slot == NULL) {
		len = http_write_headers(s_httpRespBuf, 404, "Not Found", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"slot %u not online\"}", (unsigned)slot_id);
		http_send_and_close(pcb, len);
		return;
	}

	if (!http_json_find_int(body, "\"temp_alarm_high\"", &t_ah) || t_ah < -128 || t_ah > 127 ||
	    !http_json_find_int(body, "\"temp_alarm_low\"", &t_al) || t_al < -128 || t_al > 127 ||
	    !http_json_find_int(body, "\"temp_shutdown_high\"", &t_sh) || t_sh < -128 || t_sh > 127 ||
	    !http_json_find_int(body, "\"temp_shutdown_low\"", &t_sl) || t_sl < -128 || t_sl > 127 ||
	    !http_json_find_uint(body, "\"volt_alarm_percent\"", &v_ap) || v_ap > 100 ||
	    !http_json_find_uint(body, "\"volt_shutdown_percent\"", &v_sp) || v_sp > 100) {
		len = http_write_headers(s_httpRespBuf, 400, "Bad Request", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len,
			"{\"error\":\"missing/invalid threshold fields (temp -128~127, percent 0~100)\"}");
		http_send_and_close(pcb, len);
		return;
	}

	data[0] = (uint8_t)(int8_t)t_ah;
	data[1] = (uint8_t)(int8_t)t_al;
	data[2] = (uint8_t)(int8_t)t_sh;
	data[3] = (uint8_t)(int8_t)t_sl;
	data[4] = (uint8_t)v_ap;
	data[5] = (uint8_t)v_sp;

	ticket = Ipmb_ThresholdConfig_Submit(slot->addr, data, sizeof(data));
	if (ticket == 0) {
		len = http_write_headers(s_httpRespBuf, 503, "Service Unavailable", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"control queue full, retry later\"}");
		http_send_and_close(pcb, len);
		return;
	}

	len = http_write_headers(s_httpRespBuf, 202, "Accepted", "application/json");
	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"{\"ticket\":%u,\"slot_id\":%u}", (unsigned)ticket, (unsigned)slot_id);
	http_send_and_close(pcb, len);
}

static void handle_ticket_get(struct tcp_pcb *pcb, uint16_t ticket)
{
	const IpmbCtrlResult_t *r = Ipmb_Ctrl_GetResult(ticket);
	uint16_t len;
	const char *status_str;

	if (r == NULL) {
		len = http_write_headers(s_httpRespBuf, 404, "Not Found", "application/json");
		len += (uint16_t)sprintf(s_httpRespBuf + len, "{\"error\":\"ticket %u not found\"}", (unsigned)ticket);
		http_send_and_close(pcb, len);
		return;
	}

	switch (r->status) {
	case IPMB_CTRL_DONE:    status_str = "done"; break;
	case IPMB_CTRL_TIMEOUT: status_str = "timeout"; break;
	default:                status_str = "pending"; break;
	}

	len = http_write_headers(s_httpRespBuf, 200, "OK", "application/json");
	len += (uint16_t)sprintf(s_httpRespBuf + len,
		"{\"ticket\":%u,\"status\":\"%s\",\"control_req\":%u,\"completion_code\":%u}",
		(unsigned)r->ticket, status_str, (unsigned)r->control_req, (unsigned)r->completion_code);
	http_send_and_close(pcb, len);
}

/* 从请求首行 "METHOD PATH[?QUERY] HTTP/1.x" 里切出 method 和 path(query串直接丢弃,
 * 目前所有 GET 端点都用路径段传参,没有端点需要读 query) */
static void http_parse_method_path(const char *req, char *method, uint16_t method_cap,
                                    char *path, uint16_t path_cap)
{
	uint16_t i = 0, j;
	method[0] = '\0';
	path[0] = '\0';

	while (req[i] == ' ') i++;
	j = 0;
	while (req[i] != '\0' && req[i] != ' ' && (uint16_t)(j + 1) < method_cap) method[j++] = req[i++];
	method[j] = '\0';

	while (req[i] == ' ') i++;
	j = 0;
	while (req[i] != '\0' && req[i] != ' ' && req[i] != '?' && (uint16_t)(j + 1) < path_cap) path[j++] = req[i++];
	path[j] = '\0';
}

/* 解析形如 "31/sensors" 里开头的十进制槽位号(0~31),*rest 指向数字后面剩下的部分
 * (可能是空串、"/sensors"、"/pem"),失败(非数字开头/超范围/超过2位数字)返回0 */
static uint8_t http_parse_slot_id(const char *s, uint8_t *out, const char **rest)
{
	uint32_t v = 0;
	uint8_t n = 0;
	while (s[n] >= '0' && s[n] <= '9') {
		v = v * 10 + (uint32_t)(s[n] - '0');
		n++;
		if (v > 31 || n > 2) return 0;
	}
	if (n == 0) return 0;
	*out = (uint8_t)v;
	*rest = s + n;
	return 1;
}

/* 解析 ticket 号(不限0~31,普通十进制数字串即可),失败(非数字开头/超5位)返回0 */
static uint8_t http_parse_ticket_id(const char *s, uint32_t *out)
{
	uint32_t v = 0;
	uint8_t n = 0;
	while (s[n] >= '0' && s[n] <= '9') {
		v = v * 10 + (uint32_t)(s[n] - '0');
		n++;
		if (n > 5) return 0;
	}
	if (n == 0) return 0;
	*out = v;
	return 1;
}

/*
 * 已知限制: POST body 依赖它跟请求行/头部在同一个 pbuf 里一起到达(用
 * strstr(req_buf,"\r\n\r\n")找body起点)。本控制台的POST body都只有几十字节
 * (比如 {"control_req":1}),配合请求行+头部通常远小于一个MSS,浏览器fetch()/
 * curl 也几乎总是一次性发出——受信管理网络内部API,接受这个简化,没有做
 * 跨多次 http_recv 回调累积body的状态机。真遇到body被分片的极端情况,
 * 后果是拿到空body、按"缺少control_req/action"回400,不是崩溃/脏数据。
 */
static err_t http_recv(void *arg, struct tcp_pcb *pcb, struct pbuf *p, err_t err)
{
	char req_buf[768];
	char method[8];
	char path[80];
	u16_t copied;
	uint8_t slot_id;
	uint32_t ticket_id;
	const char *rest;
	const char *body;

	if (p == NULL)
	{
		/* 对端关闭连接 */
		tcp_close(pcb);
		return ERR_OK;
	}

	if (err != ERR_OK)
	{
		pbuf_free(p);
		return err;
	}

	tcp_recved(pcb, p->tot_len);

	copied = pbuf_copy_partial(p, req_buf, (u16_t)(sizeof(req_buf) - 1), 0);
	req_buf[copied] = '\0';
	pbuf_free(p);

	http_parse_method_path(req_buf, method, sizeof(method), path, sizeof(path));

	body = strstr(req_buf, "\r\n\r\n");
	body = (body != NULL) ? (body + 4) : "";

	if (strcmp(method, "OPTIONS") == 0) { handle_options(pcb); return ERR_OK; }

	if (strcmp(method, "GET") == 0)
	{
		if (strcmp(path, "/") == 0) { handle_root(pcb); return ERR_OK; }
		if (strcmp(path, "/api/v1/health") == 0) { handle_health(pcb); return ERR_OK; }
		if (strcmp(path, "/api/v1/slots") == 0) { handle_slots_list(pcb); return ERR_OK; }

		if (strncmp(path, "/api/v1/slots/", 14) == 0)
		{
			rest = path + 14;
			if (http_parse_slot_id(rest, &slot_id, &rest))
			{
				if (rest[0] == '\0') { handle_slot_detail(pcb, slot_id); return ERR_OK; }
				if (strcmp(rest, "/sensors") == 0) { handle_slot_sensors(pcb, slot_id); return ERR_OK; }
				if (strcmp(rest, "/pem") == 0) { handle_slot_pem(pcb, slot_id); return ERR_OK; }
			}
		}

		if (strncmp(path, "/api/v1/tickets/", 16) == 0)
		{
			if (http_parse_ticket_id(path + 16, &ticket_id))
			{
				handle_ticket_get(pcb, (uint16_t)ticket_id);
				return ERR_OK;
			}
		}

		handle_not_found(pcb);
		return ERR_OK;
	}

	if (strcmp(method, "POST") == 0)
	{
		if (strncmp(path, "/api/v1/slots/", 14) == 0)
		{
			rest = path + 14;
			if (http_parse_slot_id(rest, &slot_id, &rest))
			{
				if (strcmp(rest, "/control") == 0)
				{
					handle_slot_control(pcb, slot_id, body);
					return ERR_OK;
				}
				if (strcmp(rest, "/identity") == 0)
				{
					handle_slot_identity_set(pcb, slot_id, body);
					return ERR_OK;
				}
				if (strcmp(rest, "/threshold") == 0)
				{
					handle_slot_threshold_set(pcb, slot_id, body);
					return ERR_OK;
				}
			}
		}
		handle_not_found(pcb);
		return ERR_OK;
	}

	handle_not_found(pcb);
	return ERR_OK;
}

static err_t http_accept(void *arg, struct tcp_pcb *newpcb, err_t err)
{
	tcp_setprio(newpcb, TCP_PRIO_NORMAL);
	tcp_arg(newpcb, NULL);
	tcp_recv(newpcb, http_recv);
	tcp_sent(newpcb, http_sent);
	tcp_err(newpcb, http_err);
	return ERR_OK;
}

/*
*********************************************************************************************************
*	函 数 名: httpd_init
*	功能说明: 在80端口起一个TCP监听, 提供网页控制台 JSON API(前端另行托管,跨域调用)
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void httpd_init(void)
{
	struct tcp_pcb *pcb;

	pcb = tcp_new();
	if (pcb == NULL)
	{
		printf(">>[HTTPD]tcp_new失败---ERR\r\n");
		return;
	}

	if (tcp_bind(pcb, IP_ADDR_ANY, 80) != ERR_OK)
	{
		printf(">>[HTTPD]tcp_bind失败---ERR\r\n");
		tcp_close(pcb);
		return;
	}

	pcb = tcp_listen(pcb);
	tcp_accept(pcb, http_accept);
	printf(">>[HTTPD]网页控制台JSON API已启动(80端口)...OK\r\n");
}
