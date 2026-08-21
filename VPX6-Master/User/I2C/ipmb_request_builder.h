#ifndef __IPMB_REQUEST_BUILDER_H
#define __IPMB_REQUEST_BUILDER_H

#include "task_i2c.h"   /* ipmb_pkt_t */

/*
*********************************************************************************************************
*	模 块 名: ipmb_request_builder.c
*	功能说明: 网页控制台改造(2026-07-27)新增——统一的 IPMB 请求帧构造器,给多槽位后台轮询
*	          (task_slot_poll.c)和异步控制通道(task_ctrl_dispatch.c)用,避免这两处新代码
*	          再各自手工拼帧。字节格式严格照抄仓库根目录 指令.txt 里已经用真实从机验证过的
*	          示例(而不是协议需求文档里少数几处和实测不一致的描述,冲突以 指令.txt 为准)。
*	注    意: 只给新代码用。IPMB_Discovery_Task(task_i2c.c)、IPMB_PEM_Poll_Task(task_usart.c)、
*	          控制台 ipmb/ipmboem 手动命令(task_usart.c)都已经跑通、经过现场验证,不做迁移,
*	          避免影响已稳定的路径。跟项目里"各文件自带一份 checksum/入队小函数"的既有约定
*	          一样,本文件自己复制一份 checksum 静态函数,不跨文件引用 task_i2c.c 里的 static
*	          ipmi_calc_chk。
*
*	所有 Build 函数的公共参数:
*	  pkt         - 输出;构造好的请求帧写入这里,pkt->len 同时是返回值
*	  target_addr - 目标从机 IPMB 地址(0x30 + Slot_ID<<1)
*	  rqsa        - 请求方地址字段(buf[3]):正常主控业务填 0x20;后台轮询/控制通道为了让
*	                I2C1_Task/I2C2_Task 把响应分流到专属 mailbox、不与手动命令回显互抢,
*	                会填一个不会出现在真实地址里的奇数标记(比如 IPMB_WEB_HOST_ADDR_7BIT)
*	  rqseq       - 请求序列号(6bit,调用方自行递增),lun 固定填0
*	返回值: 构造出的帧长度(pkt->len),同时也写进 pkt->len,失败(data_len 超界)返回0
*********************************************************************************************************
*/

uint8_t IPMB_Build_GetDeviceID(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq);

uint8_t IPMB_Build_GetBoardStatus(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq);

uint8_t IPMB_Build_GetSlot(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq);

/* record_id: 0~4(见 指令.txt④节,仓库固定5条记录 0=90Temp 1=92Temp 2=V12 3=V0.75 4=Iout) */
uint8_t IPMB_Build_GetDeviceSDR(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                 uint16_t record_id);

/* sensor_num: 见 指令.txt⑤节,目前只有 0x04/0x20/0x03/0x17/0x19/0x41/0x07 这7个能读到真实值 */
uint8_t IPMB_Build_GetSensorReading(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                     uint8_t sensor_num);

/* control_req: 0x00~0x11,见 指令.txt⑧节逐条核实过的行为表(含2026-08-20新增的
 * 0x0A=恢复温控自动模式)。
 * data/data_len: 只有 0x07(1字节档位)/0x08(2字节rpm,低字节在前)/0x09(1字节BCD版本)
 * 这三个控制请求需要附带数据,其余(含新增的0x0A)传 data=NULL,data_len=0 即可。
 * data_len 最大 4。 */
uint8_t IPMB_Build_SetFruActivation(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                     uint8_t control_req, const uint8_t *data, uint8_t data_len);

uint8_t IPMB_Build_PlatformEventPoll(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq);

/* 板卡身份信息(2026-08-20新增,对应从机 ipmb_board_identity.c):field_id 0~10,
 * 0~6 是原来 Get Device ID 里的 7 个字段(现在可写了),7~10 是新增自定义文本字段
 * (CPU型号/内存容量/主板型号/序列号)。field_id 含义见 task_slot_cache.h 顶部注释。 */
uint8_t IPMB_Build_GetBoardIdentityField(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                          uint8_t field_id);

/* value_len 上限 63(跟从机 IPMB_BOARD_IDENTITY_TEXT_MAX 一致,两个工程各自独立烧录,
 * 没有共享头文件,这里的 63 是两侧约定好的协议常量,不是巧合) */
uint8_t IPMB_Build_SetBoardIdentityField(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                          uint8_t field_id, const uint8_t *value, uint8_t value_len);

#endif
