#include "ipmb_protocol.h"
#include <string.h>

/*
 * IPMI 校验和算法 (2's complement):
 *   cs = (uint8_t)(0 - sum(buf[0..len-1]))
 *   性质: sum(buf[0..len-1]) + cs == 0 (mod 256)
 *
 * IPMB 帧含两个校验码:
 *   cs1 覆盖 [0..1]  (rsSA + netFn/lun)
 *   cs2 覆盖 [3..N-1] (rqSA + rqSeq/lun + cmd + data)
 */
uint8_t IPMB_Calc_Checksum(uint8_t* buf, uint8_t len)
{
    uint16_t sum = 0;
    uint8_t  i;
    for (i = 0; i < len; i++) {
        sum += buf[i];
    }
    return (uint8_t)(0u - sum);
}

/*
 * 构造 IPMB 请求帧
 */
uint8_t IPMB_Build_Request(uint8_t* tx_buf,
                            uint8_t rsSA, uint8_t netFn, uint8_t lun,
                            uint8_t rqSA, uint8_t rqSeq, uint8_t cmd,
                            const uint8_t* data, uint8_t data_len)
{
    uint8_t len = 0;
    uint8_t i;

    tx_buf[len++] = rsSA;
    tx_buf[len++] = IPMB_BUILD_NETFN_LUN(netFn, lun);
    tx_buf[len++] = IPMB_Calc_Checksum(tx_buf, 2);     /* cs1 覆盖 [0..1] */

    tx_buf[len++] = rqSA;
    tx_buf[len++] = IPMB_BUILD_RQSEQ_LUN(rqSeq, lun);
    tx_buf[len++] = cmd;

    if (data != NULL && data_len > 0) {
        for (i = 0; i < data_len; i++) {
            tx_buf[len++] = data[i];
        }
    }

    /* cs2 覆盖 [3..len-1] */
    tx_buf[len++] = IPMB_Calc_Checksum(&tx_buf[3], (uint8_t)(len - 3));

    return len;
}

/*
 * 构造 IPMB 响应帧
 * 响应: netFn 自动 OR 0x04 (Response bit, IPMI 规范 §6.4.4)
 * 数据: 第 1 字节为 cc, 其后为用户 data
 */
uint8_t IPMB_Build_Response(uint8_t* tx_buf,
                             uint8_t rsSA, uint8_t netFn, uint8_t lun,
                             uint8_t rqSA, uint8_t rqSeq, uint8_t cmd,
                             uint8_t cc, const uint8_t* data, uint8_t data_len)
{
    uint8_t len = 0;
    uint8_t i;

    tx_buf[len++] = rsSA;
    /* 响应位 = netFn6 位值 +1 (即左移 2 位后 | 0x04)。
     * 注意 OR 必须在 <<2 之后:若在左移前对 0x06/0x04/0x2C 去 OR 0x04,
     * 因这些值 bit2 本就为 1 → 空操作,响应位永远置不上(响应 netFn 恒等于请求)。
     * 修正后: App 0x06→0x1C, Sensor 0x04→0x14, Group 0x2C→0xB4 (与协议文档逐字节一致)。 */
    tx_buf[len++] = (uint8_t)(IPMB_BUILD_NETFN_LUN(netFn, lun) | IPMB_RESP_BIT);
    tx_buf[len++] = IPMB_Calc_Checksum(tx_buf, 2);

    tx_buf[len++] = rqSA;
    tx_buf[len++] = IPMB_BUILD_RQSEQ_LUN(rqSeq, lun);
    tx_buf[len++] = cmd;

    tx_buf[len++] = cc;
    if (data != NULL && data_len > 0) {
        for (i = 0; i < data_len; i++) {
            tx_buf[len++] = data[i];
        }
    }

    tx_buf[len++] = IPMB_Calc_Checksum(&tx_buf[3], (uint8_t)(len - 3));
    return len;
}

/*
 * 验证 IPMB 帧
 */
uint8_t IPMB_Verify_Checksum(const uint8_t* rx_buf, uint8_t frame_len)
{
    uint8_t calc1, calc2;
    if (rx_buf == NULL || frame_len < IPMB_MIN_FRAME_LEN) {
        return 1;
    }
    /* 头部校验: [0..1] + rx_buf[2] 累加应为 0 */
    calc1 = IPMB_Calc_Checksum((uint8_t*)rx_buf, 2);
    if (calc1 != rx_buf[2]) {
        return 1;
    }
    /* 尾部校验: [3..N-2] + rx_buf[N-1] 累加应为 0 */
    calc2 = IPMB_Calc_Checksum((uint8_t*)&rx_buf[3], (uint8_t)(frame_len - 4));
    if (calc2 != rx_buf[frame_len - 1]) {
        return 2;
    }
    return 0;
}

uint8_t IPMB_Extract_RqSeq(const uint8_t* rx_buf)
{
    if (rx_buf == NULL) return 0;
    return (rx_buf[4] >> IPMB_RQSEQ_SHIFT) & IPMB_RQSEQ_MASK;
}

uint8_t IPMB_Extract_RqLun(const uint8_t* rx_buf)
{
    if (rx_buf == NULL) return 0;
    return rx_buf[4] & IPMB_LUN_MASK;
}

uint8_t IPMB_Find_FrameLen(const uint8_t* buf, uint8_t buf_len)
{
    uint8_t len;
    if (buf == NULL || buf_len < IPMB_MIN_FRAME_LEN) {
        return 0;
    }
    /* 从最小帧长度开始, 遍历每个候选长度, 校验通过则返回 */
    for (len = IPMB_MIN_FRAME_LEN; len <= buf_len; len++) {
        if (IPMB_Verify_Checksum(buf, len) == 0) {
            return len;
        }
    }
    return 0;
}
