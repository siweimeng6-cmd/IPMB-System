#ifndef __IPMB_PROTOCOL_H
#define __IPMB_PROTOCOL_H

#include "stm32f10x.h"
#include <stdint.h>

/* ============================================================================
 * IPMB 协议帧结构 (IPMI v1.5 / v2.0 §6.4, §8, §13)
 *
 * 帧布局 (单位: 字节):
 *   [0]    rsSA  (7-bit 响应地址)             <-- 接收方 IPMB 地址
 *   [1]    netFn[7:2] | lun[1:0]
 *   [2]    头部校验码 1 (覆盖 [0..1])
 *   [3]    rqSA  (7-bit 请求地址)             <-- 发送方 IPMB 地址
 *   [4]    rqSeq[7:2] | rqLun[1:0]
 *   [5]    命令码 (cmd)
 *   [6..N-1] 数据 (data)
 *   [N]    尾部校验码 2 (覆盖 [3..N-1])
 *
 * 校验算法: 2's complement checksum, 满足 "含校验字节在内的累加和 mod 256 == 0"
 * ==========================================================================*/

/* 帧最小长度 (无数据): rsSA+netfn+cs1 + rqSA+seq+cmd + cs2 = 7 字节
 * (原为 8,会把 Get Device ID 等所有无数据的 7 字节请求误判为 CS error 丢弃) */
#define IPMB_MIN_FRAME_LEN           7
/* 帧最大长度 (含数据 + 两个校验码) */
#define IPMB_MAX_FRAME_LEN           128
/* 单帧最大数据长度 */
#define IPMB_MAX_DATA_LEN            (IPMB_MAX_FRAME_LEN - IPMB_MIN_FRAME_LEN)

/* IPMB 地址约定 (7-bit 形式, 写入总线时需 <<1) */
#define IPMB_ADDR_BROADCAST          0x00    /* 广播地址 (0x00) */
#define IPMB_ADDR_HOST               0x20    /* 主机 (BMC) 7-bit 地址 */
#define IPMB_HOST_I2C_ADDR           (IPMB_ADDR_HOST << 1)  /* 0x40, 总线字节 */
#define IPMB_ADDR_DEFAULT_SLAVE      0x8E    /* 默认业务板 IPMC 地址 */

/* rqSeq/lun 字节编码 */
#define IPMB_LUN_MASK                0x03    /* [0:1] lun */
#define IPMB_RQSEQ_SHIFT             2       /* [2:7] rqSeq */
#define IPMB_RQSEQ_MASK              0x3F    /* 6-bit, 0..63 循环 */
#define IPMB_BUILD_RQSEQ_LUN(rqSeq, lun) \
    (((uint8_t)((rqSeq) & IPMB_RQSEQ_MASK) << IPMB_RQSEQ_SHIFT) | ((uint8_t)((lun) & IPMB_LUN_MASK)))

/* netFn/lun 字节编码 */
#define IPMB_NETFN_SHIFT             2
#define IPMB_NETFN_MASK              0x3F    /* [2:7] 6-bit netFn */
#define IPMB_BUILD_NETFN_LUN(netFn, lun) \
    (((uint8_t)((netFn) & IPMB_NETFN_MASK) << IPMB_NETFN_SHIFT) | ((uint8_t)((lun) & IPMB_LUN_MASK)))
#define IPMB_EXTRACT_NETFN(byte)     ((uint8_t)((byte) >> IPMB_NETFN_SHIFT) & IPMB_NETFN_MASK)
#define IPMB_EXTRACT_LUN(byte)       ((uint8_t)((byte) & IPMB_LUN_MASK))

/* 网络功能码 (NetFn) - 见 IPMI v2.0 §5.1 */
#define IPMB_NETFN_CHASSIS           0x00
#define IPMB_NETFN_BRIDGE            0x02
#define IPMB_NETFN_SENSOR            0x04    /* Sensor/Event 请求 */
#define IPMB_NETFN_APP               0x06    /* App (含 Device, Get Device ID) */
#define IPMB_NETFN_FIRMWARE          0x08
#define IPMB_NETFN_STORAGE           0x0A
#define IPMB_NETFN_TRANSPORT         0x0C
#define IPMB_NETFN_GROUP             0x2C    /* PICMG/VSO 范围 */
#define IPMB_NETFN_OEM               0x30    /* 0x30-0x3F OEM */

/* 设备相关 netFn == App, 沿用 IPMB_NETFN_APP */
#define IPMB_NETFN_DEVICE            IPMB_NETFN_APP

/* OEM Get Module Info (非标 netFn 编码:
 *   请求: netFn 字节 = 原始 6-bit 值 0x2E (不左移)
 *   响应: netFn 字节 = (0x2E << 2) | lun, 不设 IPMB_RESP_BIT) */
#define IPMB_NETFN_OEM_MODULE        0x2E    /* OEM 模块信息 netFn (非标) */
#define IPMB_CMD_GET_MODULE_INFO     0x12    /* 获取模块数据命令 */

/* OEM Get Module Info 数据项 ID */
#define IPMB_OEM_ITEM_SENSOR_INFO    0x11    /* BIT 传感器信息 (名称/类型) */
#define IPMB_OEM_ITEM_SENSOR_VALUE   0x12    /* 所有 BIT 值 (传感器读数) */

/* OEM 传感器类型代号 (附录一) */
#define IPMB_OEM_SENSOR_TYPE_TEMP    1       /* 温度 */
#define IPMB_OEM_SENSOR_TYPE_VOLT    2       /* 电压 */
#define IPMB_OEM_SENSOR_TYPE_CURR    3       /* 电流 */
#define IPMB_OEM_SENSOR_TYPE_FAN     4       /* 风扇 */
#define IPMB_OEM_SENSOR_TYPE_NET     5       /* 网络模块状态 */

/* OEM 模块固定传感器数量 */
#define IPMB_OEM_SENSOR_COUNT        5

/* 响应标识: 响应 netFn = 请求 netFn | 0x04 (Request bit), bit2 置 1 */
#define IPMB_RESP_BIT                0x04
#define IPMB_IS_RESPONSE(netFnByte)  ((netFnByte) & IPMB_RESP_BIT)

/* =================== 命令码 (Cmd) =================== */
/* 设备类 (netFn 0x06) */
#define IPMB_CMD_GET_DEVICE_ID       0x01
#define IPMB_CMD_COLD_RESET          0x02
#define IPMB_CMD_WARM_RESET          0x03
#define IPMB_CMD_GET_SELF_TEST       0x04
#define IPMB_CMD_GET_ACPI_POWER      0x06
#define IPMB_CMD_SET_ACPI_POWER      0x07
/* OEM 自定义 */
#define IPMB_CMD_GET_SLOT            0x15    /* OEM: Get Slot */
#define IPMB_CMD_GET_BOARD_STATUS    0x16    /* OEM: Get Board System Status */

/* 传感器类 (netFn 0x04) */
#define IPMB_CMD_PEM                 0x02    /* Platform Event Message */
#define IPMB_CMD_GET_SDR_INFO        0x21    /* Get Device SDR Info */
#define IPMB_CMD_GET_SDR             0x23    /* Get SDR */
#define IPMB_CMD_GET_SENSOR_READING  0x2D

/* FRU 存储类 (netFn 0x2C, VSO 域) */
#define IPMB_CMD_SET_FRU_ACTIVATION  0x0C    /* VSO 03h */

/* =================== 完成码 (Completion Code) =================== */
#define IPMB_CC_OK                   0x00
#define IPMB_CC_NODE_BUSY            0xC0
#define IPMB_CC_INVALID_COMMAND      0xC1
#define IPMB_CC_INVALID_DATA_FIELD   0xC2
#define IPMB_CC_TIMEOUT              0xC3
#define IPMB_CC_OUT_OF_SPACE         0xC4
#define IPMB_CC_INVALID_RESERVATION  0xC5
#define IPMB_CC_REQUEST_DATA_TRUNC   0xC6
#define IPMB_CC_REQUEST_DATA_LEN_INV 0xC7
#define IPMB_CC_DATA_OUT_OF_RANGE    0xC9
#define IPMB_CC_CANNOT_RETURN        0xCA
#define IPMB_CC_UNSPECIFIED          0xFF

/* =================== API =================== */

/**
 * @brief  计算 IPMB 2's-complement 校验和
 * @param  buf  输入数据
 * @param  len  数据长度 (不含校验字节本身)
 * @return 校验字节. 含此字节后, [原buf] + cs 的 8-bit 累加和 == 0
 */
uint8_t IPMB_Calc_Checksum(uint8_t* buf, uint16_t len);

/**
 * @brief  构造 IPMB 请求帧
 * @param  tx_buf   输出缓冲区 (至少 IPMB_MAX_FRAME_LEN)
 * @param  rsSA     接收方 7-bit IPMB 地址
 * @param  netFn    网络功能码
 * @param  lun      逻辑单元号
 * @param  rqSA     发送方 7-bit IPMB 地址
 * @param  rqSeq    请求序列号
 * @param  cmd      命令码
 * @param  data     数据
 * @param  data_len 数据长度
 * @return 帧总长度
 */
uint8_t IPMB_Build_Request(uint8_t* tx_buf,
                            uint8_t rsSA, uint8_t netFn, uint8_t lun,
                            uint8_t rqSA, uint8_t rqSeq, uint8_t cmd,
                            const uint8_t* data, uint8_t data_len);

/**
 * @brief  构造 IPMB 响应帧
 */
uint8_t IPMB_Build_Response(uint8_t* tx_buf,
                             uint8_t rsSA, uint8_t netFn, uint8_t lun,
                             uint8_t rqSA, uint8_t rqSeq, uint8_t cmd,
                             uint8_t cc, const uint8_t* data, uint8_t data_len);

/**
 * @brief  验证 IPMB 帧校验码
 * @param  rx_buf   接收到的完整帧
 * @param  frame_len 帧总长度
 * @return 0 校验通过, 1 头部校验错, 2 尾部校验错
 */
uint8_t IPMB_Verify_Checksum(const uint8_t* rx_buf, uint16_t frame_len);

/**
 * @brief  从帧中提取 rqSeq
 */
uint8_t IPMB_Extract_RqSeq(const uint8_t* rx_buf);

/**
 * @brief  从帧中提取 rqLun
 */
uint8_t IPMB_Extract_RqLun(const uint8_t* rx_buf);

/**
 * @brief  从读取到的数据中查找IPMB帧实际长度
 * @param  buf      读取到的原始数据缓冲区
 * @param  buf_len  缓冲区可读长度
 * @return 帧长度 (含校验码), 未找到返回 0
 */
uint8_t IPMB_Find_FrameLen(const uint8_t* buf, uint8_t buf_len);

#endif /* __IPMB_PROTOCOL_H */


