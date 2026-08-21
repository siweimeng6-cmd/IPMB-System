#ifndef __IPMB_BOARD_IDENTITY_H
#define __IPMB_BOARD_IDENTITY_H

#include "stm32f10x.h"

/*
*********************************************************************************************************
*	模 块 名: ipmb_board_identity.c
*	功能说明: 板卡身份信息统一存储(2026-08-20新增)——把原来编译期写死在 g_slave_device_id
*	          (ipmb_slave.c)里的 7 个 Get Device ID 字段,加上 4 个新增自定义文本字段(CPU型号/
*	          内存容量/主板型号/序列号),合并成一张按 field_id(0~10)索引的表,统一走新增的
*	          IPMB_CMD_GET_BOARD_IDENTITY/SET_BOARD_IDENTITY 命令读写,持久化进本机 EEPROM
*	          (bsp_eeprom.c,地址 256 起,每字段固定 32 字节槽位)。
*	          field_id 0~6(数值/原始字节字段)驱动的是标准 Get Device ID(0x01)响应本体——
*	          IPMB_Slave_Handle_GetDeviceID 已改成读这里而不是 g_slave_device_id,所以网页
*	          "获取板卡在位信息"卡片会直接反映这里保存的值。field_id 7~11(文本字段)只通过
*	          新命令读取,不影响标准 Get Device ID 帧格式;其中 field_id 11(OS版本)是独立的
*	          自定义文本字段,跟 Get Board Status 里真实的 os_version 协议字节(仍恒为0xFF
*	          占位符,未改动)没有关系,只是网页选择把它展示在"获取板卡系统状态"卡片里。
*	          EEPROM 未配置过时(擦除态)每个字段回退到今天的硬编码默认值,行为与改动前一致。
*********************************************************************************************************
*/

#define IPMB_BOARD_IDENTITY_FIELD_COUNT   12
#define IPMB_BOARD_IDENTITY_TEXT_MAX      63   /* 文本字段最大字节数(UTF-8),+1 结尾'\0' 凑满 64 字节缓冲区。
                                                 * 纯ASCII下可存63个字符,纯中文(每字3字节)约21个字 */

typedef struct {
	uint8_t  device_id;                                  /* field_id 0 */
	uint8_t  hw_revision;                                /* field_id 1 */
	uint8_t  fw_major_rev;                                /* field_id 2 */
	uint8_t  ipmi_version;                                /* field_id 3 */
	uint8_t  device_support;                              /* field_id 4 */
	uint8_t  mfg_id[3];                                   /* field_id 5,低字节在前 */
	uint8_t  product_id[2];                               /* field_id 6,低字节在前 */
	char     cpu_model[IPMB_BOARD_IDENTITY_TEXT_MAX + 1];     /* field_id 7 */
	char     mem_size[IPMB_BOARD_IDENTITY_TEXT_MAX + 1];      /* field_id 8 */
	char     board_model[IPMB_BOARD_IDENTITY_TEXT_MAX + 1];   /* field_id 9 */
	char     serial_number[IPMB_BOARD_IDENTITY_TEXT_MAX + 1]; /* field_id 10 */
	char     os_version_text[IPMB_BOARD_IDENTITY_TEXT_MAX + 1]; /* field_id 11,命名故意跟
	                                    Get Board Status 里真实的 os_version 字节区分开 */
} IPMB_BoardIdentity_t;

extern IPMB_BoardIdentity_t g_board_identity;

/* 开机调一次(bsp_init.c 里紧跟 Runtime_Init() 之后):从 EEPROM 读回全部 11 个字段,
 * 未配置过(EEPROM 擦除态)的字段回退到今天的硬编码默认值 */
void BoardIdentity_Init(void);

/* 取一个字段的当前值(RAM 缓存,现读现填,不碰 EEPROM)。
 * out_buf 需要至少 IPMB_BOARD_IDENTITY_TEXT_MAX 字节。
 * 返回 1=成功(out_len 有效),0=field_id 非法 */
uint8_t BoardIdentity_GetField(uint8_t field_id, uint8_t *out_buf, uint8_t *out_len);

/* 设一个字段的值:校验长度(数值/原始字节字段要求精确匹配,文本字段要求 ≤ 上限且必须是
 * 合法的 UTF-8——可打印 ASCII 或合法的 2/3/4 字节 UTF-8 序列,覆盖中文等常用字符,拒绝
 * 控制字符/畸形序列)后更新 RAM 并立即写 EEPROM(这是低频人工操作,不像累计运行时长那样
 * 批量延迟写入)。
 * 返回 0=成功,1=校验失败(field_id非法/长度不对/含非法字节或畸形UTF-8),2=EEPROM写入失败 */
uint8_t BoardIdentity_SetField(uint8_t field_id, const uint8_t *value, uint8_t value_len);

#endif
