#include "ipmb_request_builder.h"

/* IPMB 2's-complement 校验和,跟 task_i2c.c 里的 ipmi_calc_chk / task_web_sensor.c 的
 * web_ipmb_cs 算法完全一致(各文件自带一份是项目既有约定,见 ipmb_request_builder.h 说明) */
static uint8_t rb_calc_chk(const uint8_t *buf, uint8_t start, uint8_t end)
{
	uint8_t s = 0, i;
	for (i = start; i < end; i++) s = (uint8_t)(s + buf[i]);
	return (uint8_t)(-s);
}

/* 填好公共帧头(target/netFn/头校验/rqSA/rqSeq/cmd),返回写到 buf 的下一个下标(即 6) */
static uint8_t rb_put_header(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t netfn,
                              uint8_t rqsa, uint8_t rqseq, uint8_t cmd)
{
	pkt->len = 0;
	pkt->buf[pkt->len++] = target_addr;
	pkt->buf[pkt->len++] = (uint8_t)(netfn << 2);          /* lun 固定0 */
	pkt->buf[pkt->len] = rb_calc_chk(pkt->buf, 0, 2); pkt->len++;
	pkt->buf[pkt->len++] = rqsa;
	pkt->buf[pkt->len++] = (uint8_t)(rqseq << 2);          /* lun 固定0 */
	pkt->buf[pkt->len++] = cmd;
	return pkt->len;
}

/* 补尾校验(覆盖 buf[3..len-1]),写完后 pkt->len 自增1,返回最终帧长 */
static uint8_t rb_put_tail_chk(ipmb_pkt_t *pkt)
{
	pkt->buf[pkt->len] = rb_calc_chk(pkt->buf, 3, pkt->len);
	pkt->len++;
	return pkt->len;
}

uint8_t IPMB_Build_GetDeviceID(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq)
{
	rb_put_header(pkt, target_addr, 0x06, rqsa, rqseq, 0x01);   /* netFn=06h(App), cmd=01h, 无数据 */
	return rb_put_tail_chk(pkt);
}

uint8_t IPMB_Build_GetBoardStatus(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq)
{
	rb_put_header(pkt, target_addr, 0x06, rqsa, rqseq, 0x16);   /* netFn=06h,cmd=16h,无数据 */
	return rb_put_tail_chk(pkt);
}

uint8_t IPMB_Build_GetSlot(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq)
{
	rb_put_header(pkt, target_addr, 0x06, rqsa, rqseq, 0x15);   /* netFn=06h,cmd=15h,无数据 */
	return rb_put_tail_chk(pkt);
}

uint8_t IPMB_Build_GetDeviceSDR(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                 uint16_t record_id)
{
	rb_put_header(pkt, target_addr, 0x04, rqsa, rqseq, 0x21);   /* netFn=04h(Sensor/Event),cmd=21h */
	pkt->buf[pkt->len++] = 0x00;                                /* reservation ID LS(未实现Reserve命令,填0即可) */
	pkt->buf[pkt->len++] = 0x00;                                /* reservation ID MS */
	pkt->buf[pkt->len++] = (uint8_t)(record_id & 0xFF);         /* record ID LS */
	pkt->buf[pkt->len++] = (uint8_t)((record_id >> 8) & 0xFF);  /* record ID MS */
	pkt->buf[pkt->len++] = 0x00;                                /* 记录内偏移量,从头读 */
	pkt->buf[pkt->len++] = 0xFF;                                /* 读取长度,FFh=整条记录 */
	return rb_put_tail_chk(pkt);
}

uint8_t IPMB_Build_GetSensorReading(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                     uint8_t sensor_num)
{
	rb_put_header(pkt, target_addr, 0x04, rqsa, rqseq, 0x2D);   /* netFn=04h,cmd=2Dh */
	pkt->buf[pkt->len++] = sensor_num;                          /* data[0]=Sensor Number */
	return rb_put_tail_chk(pkt);
}

uint8_t IPMB_Build_SetFruActivation(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                     uint8_t control_req, const uint8_t *data, uint8_t data_len)
{
	uint8_t i;
	if (data_len > 4) return 0;   /* 超出协议里最大的0x08(2字节)/预留余量,视为调用方传参错误 */

	rb_put_header(pkt, target_addr, 0x2C, rqsa, rqseq, 0x0C);   /* netFn=2Ch(VSO),cmd=0Ch */
	pkt->buf[pkt->len++] = 0x03;                                /* VSO命令号,固定03h */
	pkt->buf[pkt->len++] = 0x00;                                /* FRU ID,暂只支持1个FRU,固定00h */
	pkt->buf[pkt->len++] = control_req;
	for (i = 0; i < data_len; i++) pkt->buf[pkt->len++] = data[i];
	return rb_put_tail_chk(pkt);
}

uint8_t IPMB_Build_PlatformEventPoll(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq)
{
	rb_put_header(pkt, target_addr, 0x04, rqsa, rqseq, 0x02);   /* netFn=04h,cmd=02h,无数据 */
	return rb_put_tail_chk(pkt);
}

uint8_t IPMB_Build_GetBoardIdentityField(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                          uint8_t field_id)
{
	rb_put_header(pkt, target_addr, 0x06, rqsa, rqseq, 0x17);   /* netFn=06h(App),cmd=17h */
	pkt->buf[pkt->len++] = field_id;                            /* data[0]=field_id */
	return rb_put_tail_chk(pkt);
}

uint8_t IPMB_Build_SetBoardIdentityField(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                          uint8_t field_id, const uint8_t *value, uint8_t value_len)
{
	uint8_t i;
	if (value_len > 63) return 0;   /* 跟从机 IPMB_BOARD_IDENTITY_TEXT_MAX 一致 */

	rb_put_header(pkt, target_addr, 0x06, rqsa, rqseq, 0x18);   /* netFn=06h,cmd=18h */
	pkt->buf[pkt->len++] = field_id;                            /* data[0]=field_id */
	pkt->buf[pkt->len++] = value_len;                           /* data[1]=value_len */
	for (i = 0; i < value_len; i++) pkt->buf[pkt->len++] = value[i];
	return rb_put_tail_chk(pkt);
}

uint8_t IPMB_Build_GetThresholdConfig(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq)
{
	rb_put_header(pkt, target_addr, 0x06, rqsa, rqseq, 0x19);   /* netFn=06h(App),cmd=19h,无数据 */
	return rb_put_tail_chk(pkt);
}

uint8_t IPMB_Build_SetThresholdConfig(ipmb_pkt_t *pkt, uint8_t target_addr, uint8_t rqsa, uint8_t rqseq,
                                       const uint8_t *value, uint8_t value_len)
{
	uint8_t i;
	if (value_len != 6) return 0;   /* 跟从机 IPMB_THRESHOLD_CONFIG_LEN 一致,固定6字节 */

	rb_put_header(pkt, target_addr, 0x06, rqsa, rqseq, 0x1A);   /* netFn=06h,cmd=1Ah */
	for (i = 0; i < value_len; i++) pkt->buf[pkt->len++] = value[i];
	return rb_put_tail_chk(pkt);
}
