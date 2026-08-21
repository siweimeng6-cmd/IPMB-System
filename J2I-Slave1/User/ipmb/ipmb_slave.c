#include "ipmb_slave.h"
#include "bsp_init.h"
#include "bsp_adc.h"
#include "bsp_mo_i2c.h"  /* Board_ADDR90_temp, board_temp0, board_temp1 */
#include "bsp_bpd20550.h"  /* g_bpd20550_current_A: 12V 输入电流(PMBus, PB8/PB9) */
#include ".\timer\bsp_pwm.h"
#include ".\gpio\bsp_gpio.h"
#include ".\i2c\bsp_eeprom.h"  /* g_runtime_total_minutes, 见 IPMB_Slave_Handle_GetBoardStatus */
#include <string.h>
#include <stdio.h>

extern stADC_DATA_t stADC_Data;

/* OEM Get Module Info: 固定 5 个传感器名称 (各 14 字节, 不足补 0x00)
 * sensor_idx 0→90Temp(LM75A@0x90), 1→92Temp(LM75A@0x92),
 * 2→V12(ADC PC0), 3→V0.81(ADC PC3+PC4平均), 4→Iout(BPD20550 PMBus) */
static const char g_oem_sensor_names[IPMB_OEM_SENSOR_COUNT][14] = {
    "90Temp\0\0\0\0\0\0\0\0",
    "92Temp\0\0\0\0\0\0\0\0",
    "V12\0\0\0\0\0\0\0\0\0\0\0",
    "V0.81\0\0\0\0\0\0\0\0\0",
    "Iout\0\0\0\0\0\0\0\0\0\0",
};
/* 设备 ID 默认配置 */
IPMB_Slave_DeviceID_t g_slave_device_id = {
    .device_id         = 0x01,                     /* 模块类型 ID */
    .device_revision   = 0x81,                     /* [7]=1 提供SDR, [0:3]=1 硬件版本 */
    .firmware_rev1     = 0x81,                     /* [7]=1, [0:3]大版本1, [4:6]小版本0 */
    .firmware_rev2     = 0x21,                     /* IPMC v2.1 */
    .ipmi_version      = 0x51,                     /* IPMI 1.5 */
    .additional_support= 0x23,                     /* IPMC device */
    .manufacturer_id   = {0x00, 0x00, 0x00},
    .product_id        = 0x0001,
    .slot              = 0                         /* 默认槽位 0 (从 GA0..GAP GPIO 读) */
};

void IPMB_Slave_SensorInit(void)
{
    IPMB_Sensor_Init();
}

void IPMB_Slave_FRUInit(void)
{
    IPMB_FRU_Init();
}

void IPMB_Slave_Init(void)
{
    /* 占位: 当前 bsp_i2c.c 中已含 I2C 从机硬件初始化, 协议层无需额外动作 */
}

/* =================== 内部工具函数 =================== */

/**
 * @brief  构建 OEM Get Module Info 响应帧 (非标 netFn 编码)
 * @note   响应 netFn 字节 = (0x2E << 2) | lun, 不设 IPMB_RESP_BIT
 *         其余结构同标准 IPMB 响应帧
 */
static uint8_t oem_build_module_response(uint8_t* tx_buf,
                                          uint8_t rsSA, uint8_t rqSA,
                                          uint8_t rqSeq, uint8_t lun,
                                          uint8_t cc, const uint8_t* data, uint8_t data_len)
{
    uint8_t len = 0;
    uint8_t i;

    tx_buf[len++] = rsSA;
    /* 非标: (netFn << 2) | lun, 不设 IPMB_RESP_BIT */
    tx_buf[len++] = (uint8_t)((IPMB_NETFN_OEM_MODULE << 2) | (lun & 0x03));
    tx_buf[len++] = IPMB_Calc_Checksum(tx_buf, 2);

    tx_buf[len++] = rqSA;
    tx_buf[len++] = IPMB_BUILD_RQSEQ_LUN(rqSeq, lun);
    tx_buf[len++] = IPMB_CMD_GET_MODULE_INFO;

    tx_buf[len++] = cc;
    if (data != NULL && data_len > 0) {
        for (i = 0; i < data_len; i++) {
            tx_buf[len++] = data[i];
        }
    }

    tx_buf[len++] = IPMB_Calc_Checksum(&tx_buf[3], (uint8_t)(len - 3));
    return len;
}

uint8_t read_slot_from_gpio(void)
{
    /* GA0..GA4 = PA8..PC6 (bit0..bit4), GAP = PC7 (校验位, 不计入 slot)
     * 低电平有效: GPIO=0 对应该位为1, GPIO=1 对应该位为0 */
    uint8_t slot = 0;
    if (!GPIO_ReadInputDataBit(GA0_GPIO_PORT, GA0_GPIO_PIN)) slot |= 0x01;
    if (!GPIO_ReadInputDataBit(GA1_GPIO_PORT, GA1_GPIO_PIN)) slot |= 0x02;
    if (!GPIO_ReadInputDataBit(GA2_GPIO_PORT, GA2_GPIO_PIN)) slot |= 0x04;
    if (!GPIO_ReadInputDataBit(GA3_GPIO_PORT, GA3_GPIO_PIN)) slot |= 0x08;
    if (!GPIO_ReadInputDataBit(GA4_GPIO_PORT, GA4_GPIO_PIN)) slot |= 0x10;
    return slot;
}

/* =================== Handler 实现 =================== */

void IPMB_Slave_Handle_GetDeviceID(const uint8_t* req, uint8_t req_len,
                                     uint8_t* resp, uint8_t* resp_len,
                                     uint8_t own_addr)
{
    uint8_t rqSA  = req[3];
    uint8_t rqLun = IPMB_Extract_RqLun(req);
    uint8_t rqSeq = IPMB_Extract_RqSeq(req);
    uint8_t data[11];
    (void)req_len;

    data[0]  = g_slave_device_id.device_id;        /* Device ID (0x8E) */
    data[1]  = g_slave_device_id.device_revision;  /* 硬件版本 */
    data[2]  = g_slave_device_id.firmware_rev1;    /* 固件版本1 */
    data[3]  = g_fru_state.ipmc_version;    /* 固件版本2 (IPMC BCD) —— 2026-07-29改成跟 Get Board Status
                                              * 的固件版本字段共用同一个变量 g_fru_state.ipmc_version,
                                              * 不再读 g_slave_device_id.firmware_rev2 这个独立常量;
                                              * "设置IPMC版本"(control_req=0x09)执行后,Get Device ID
                                              * 和 Get Board Status 两处现在会同时反映新值 */
    data[4]  = g_slave_device_id.ipmi_version;     /* 0x51 */
    data[5]  = g_slave_device_id.additional_support;/* 0x23 IPMC */
    data[6]  = g_slave_device_id.manufacturer_id[0]; /* 低字节在前 */
    data[7]  = g_slave_device_id.manufacturer_id[1];
    data[8]  = g_slave_device_id.manufacturer_id[2];
    data[9]  = g_slave_device_id.product_id & 0xFF;  /* 低字节在前 */
    data[10] = (g_slave_device_id.product_id >> 8) & 0xFF;

    *resp_len = IPMB_Build_Response(resp, rqSA, IPMB_NETFN_APP, rqLun,
                                     own_addr, rqSeq, IPMB_CMD_GET_DEVICE_ID,
                                     IPMB_CC_OK, data, sizeof(data));
}

/**
 * @brief  Get Device SDR(标准 IPMB,netFn=0x04,cmd=0x21)
 * @note   请求携带 reservation ID(2)+record ID(2)+偏移量(1)+读取长度(1),
 *         标准分页读取语义。仓库固定 IPMB_OEM_SENSOR_COUNT(5)条记录,
 *         record ID 0..4 依次对应 5 个传感器(与 OEM item 0x11 同一张
 *         g_oem_sensor_names 表),每条记录 16 字节:
 *         [0]=sensorId(1..5) [1]=sensorType [2..15]=sensorName。
 *         支持按 offset/读取长度分段读取(FFh=从offset读到记录末尾)。
 */
void IPMB_Slave_Handle_GetDeviceSDR(const uint8_t* req, uint8_t req_len,
                                      uint8_t* resp, uint8_t* resp_len,
                                      uint8_t own_addr)
{
    uint8_t rqSA  = req[3];
    uint8_t rqLun = IPMB_Extract_RqLun(req);
    uint8_t rqSeq = IPMB_Extract_RqSeq(req);
    uint8_t cc = IPMB_CC_OK;
    uint8_t data[2 + 16];   /* next_record_id(2) + 最多16字节记录数据 */
    uint8_t data_len = 2;

    data[0] = 0xFF;   /* next record ID, LS byte:先填"无更多记录",按需覆盖 */
    data[1] = 0xFF;   /* next record ID, MS byte */

    /* 最短请求 = 头6 + resv(2)+recID(2)+offset(1)+readlen(1) + 尾校验1 = 13 字节 */
    if (req_len < 13) {
        cc = IPMB_CC_REQUEST_DATA_LEN_INV;
    } else {
        uint16_t record_id = (uint16_t)req[8] | ((uint16_t)req[9] << 8);

        if (record_id >= IPMB_OEM_SENSOR_COUNT) {
            cc = IPMB_CC_DATA_OUT_OF_RANGE;   /* 请求的 record ID 超出仓库范围 */
        } else {
            uint8_t sensor_idx = (uint8_t)record_id;
            uint8_t offset      = req[10];
            uint8_t read_len    = req[11];
            uint8_t record[16];
            uint8_t avail, copy_len, j;

            record[0] = sensor_idx + 1;   /* sensorId: 1..5 */
            switch (sensor_idx) {
            case 0: record[1] = IPMB_OEM_SENSOR_TYPE_TEMP; break;  /* 90Temp  */
            case 1: record[1] = IPMB_OEM_SENSOR_TYPE_TEMP; break;  /* 92Temp  */
            case 2: record[1] = IPMB_OEM_SENSOR_TYPE_VOLT; break;  /* V12     */
            case 3: record[1] = IPMB_OEM_SENSOR_TYPE_VOLT; break;  /* V0.81   */
            case 4: record[1] = IPMB_OEM_SENSOR_TYPE_CURR; break;  /* Iout    */
            default: record[1] = 0xFF; break;
            }
            for (j = 0; j < 14; j++) {
                record[2 + j] = g_oem_sensor_names[sensor_idx][j];
            }

            avail    = (offset < sizeof(record)) ? (uint8_t)(sizeof(record) - offset) : 0;
            copy_len = (read_len == 0xFF || read_len > avail) ? avail : read_len;
            for (j = 0; j < copy_len; j++) {
                data[2 + j] = record[offset + j];
            }
            data_len = (uint8_t)(2 + copy_len);

            if (sensor_idx + 1 < IPMB_OEM_SENSOR_COUNT) {
                data[0] = sensor_idx + 1;   /* 下一条 record ID */
                data[1] = 0;
            }   /* 否则维持上面预填的 0xFFFF(已是最后一条) */
        }
    }

    *resp_len = IPMB_Build_Response(resp, rqSA, IPMB_NETFN_SENSOR, rqLun,
                                     own_addr, rqSeq, IPMB_CMD_GET_SDR_INFO,
                                     cc, data, data_len);
}

void IPMB_Slave_Handle_GetSDR(const uint8_t* req, uint8_t req_len,
                                uint8_t* resp, uint8_t* resp_len,
                                uint8_t own_addr)
{
    uint8_t rqSA  = req[3];
    uint8_t rqLun = IPMB_Extract_RqLun(req);
    uint8_t rqSeq = IPMB_Extract_RqSeq(req);
    uint8_t data[16];
    uint8_t i;
    (void)req_len;

    /* @todo 阶段1 占位返回 16 字节 0xFF — 阶段2 需实现 SDR 动态构建
     *      (根据 g_sensor_table 填充实际传感器记录) */
    for (i = 0; i < 16; i++) data[i] = 0xFF;

    *resp_len = IPMB_Build_Response(resp, rqSA, IPMB_NETFN_SENSOR, rqLun,
                                     own_addr, rqSeq, IPMB_CMD_GET_SDR,
                                     IPMB_CC_OK, data, sizeof(data));
}

/**
 * @brief  把 bsp_temp.c decimal_2_integer() 算好的[整数度,百分位小数]拼成 centi-°C(×100定点)。
 *         decimal_2_integer 对负温度把符号放在 b[0], b[1] 存的是绝对值百分位(0..99),
 *         不能直接相加,需按 b[0] 的符号处理。
 */
static int16_t temp_to_centi(const int b[2])
{
    return (int16_t)(b[0] * 100 + (b[0] < 0 ? -b[1] : b[1]));
}

void IPMB_Slave_Handle_GetSensorReading(const uint8_t* req, uint8_t req_len,
                                          uint8_t* resp, uint8_t* resp_len,
                                          uint8_t own_addr)
{
    uint8_t rqSA  = req[3];
    uint8_t rqLun = IPMB_Extract_RqLun(req);
    uint8_t rqSeq = IPMB_Extract_RqSeq(req);
    IPMB_Sensor_Entry_t* e = NULL;
    uint8_t sensor_num;
    uint8_t data[3];
    uint8_t cc;
    int16_t reading;
    (void)req_len;

    if (req_len < 7 || req[6] == 0) {
        cc = IPMB_CC_INVALID_DATA_FIELD;
        data[0] = 0; data[1] = 0; data[2] = 0;
    } else {
        sensor_num = req[6];
        if (IPMB_Sensor_Find(sensor_num, &e) != 0) {
            cc = IPMB_CC_INVALID_DATA_FIELD;
            data[0] = 0; data[1] = 0; data[2] = 0;
        } else {
            /* 先采集最新数据 (按需触发 ADC/温度) */
            reading = e->last_reading;

            /* 已知 sensor num 直接从硬件读取最新值 (覆盖缓存) */
            switch (sensor_num) {
            case 0x04:  /* 温度 0 - LM75A @ 0x90, ×100 定点(centi-°C) */
                Board_ADDR90_temp();
                reading = (board_temp0[0] == -1 && board_temp0[1] == 0)
                              ? (int16_t)0x7FFF : temp_to_centi(board_temp0);
                break;
            case 0x20:  /* 温度 1 - LM75A @ 0x92, ×100 定点(centi-°C) */
                Board_ADDR92_temp();
                reading = (board_temp1[0] == -1 && board_temp1[1] == 0)
                              ? (int16_t)0x7FFF : temp_to_centi(board_temp1);
                break;
            case 0x03:  /* 12V - PC0, ×100 定点(centi-V) */
                reading = (int16_t)(stADC_Data.adc_voltage_val[0] * 100.0f + 0.5f);
                break;
            case 0x17:  /* 5V - PC1/VRD_MOS (bsp_adc.c: index1 = V_5V 通道), ×100 定点(centi-V) */
                reading = (int16_t)(stADC_Data.adc_voltage_val[1] * 100.0f + 0.5f);
                break;
            case 0x19:  /* 0.81V - DDRPHY 平均 (PC3+PC4, 0.81V1/0.81V2 两路), ×100 定点(centi-V) */
                reading = (int16_t)(((stADC_Data.adc_voltage_val[3] +
                                      stADC_Data.adc_voltage_val[4]) / 2.0f) * 100.0f + 0.5f);
                break;
            case 0x41:  /* 风扇档位 - PWM 占空比(0-100整数,无小数意义,不缩放)
                         * 2026-07-29改用 Fan_GetCurrentDuty(FAN_CH1)(task_fan.c 新增的风扇控制任务,
                         * 真实反映当前PWM占空比,自动/手动模式均适用);原来的 pwm_get_duty() 取自
                         * 从未真正跑起来的死代码链路,读数永远是0 */
                reading = (int16_t)Fan_GetCurrentDuty(FAN_CH1);
                break;
            case 0x07:  /* 12V/40A 电流 - BPD20550 PMBus(缓存值,周期性更新), ×100 定点(centi-A) */
                reading = g_bpd20550_current_valid
                              ? (int16_t)(g_bpd20550_current_A * 100.0f + 0.5f) : (int16_t)0x7FFF;
                break;
            default:
                break;
            }
            e->last_reading = reading;

            cc = IPMB_CC_OK;
            /* 2026-07-20起: data[0..1]恢复完整16位精度(此前只发1字节, ×100定点值截断丢失小数),
             * 原data[1]固定0x80/data[2]固定0xC0两个状态字节压缩进data[2]一个字节:
             * bit7=事件上报使能(原data[1] bit7) bit6=保留(0) bits5:0=阈值状态(本次仍固定0,未接入)。
             * 这是不兼容的协议改动, 主机(F407)侧解析逻辑必须同步更新为
             * "读LSB+MSB拼int16、除以100.0得工程值", 否则会把新MSB/状态字节当旧固定值误读。 */
            data[0] = (uint8_t)(reading & 0xFF);         /* ×100定点值 LSB */
            data[1] = (uint8_t)((reading >> 8) & 0xFF);  /* ×100定点值 MSB */
            data[2] = 0x80;   /* bit7=事件上报使能(1) bit6=保留(0) bits5:0=阈值状态(0) */
        }
    }

    *resp_len = IPMB_Build_Response(resp, rqSA, IPMB_NETFN_SENSOR, rqLun,
                                     own_addr, rqSeq, IPMB_CMD_GET_SENSOR_READING,
                                     cc, data, sizeof(data));
}

void IPMB_Slave_Handle_GetBoardStatus(const uint8_t* req, uint8_t req_len,
                                        uint8_t* resp, uint8_t* resp_len,
                                        uint8_t own_addr)
{
    uint8_t rqSA  = req[3];
    uint8_t rqSeq = IPMB_Extract_RqSeq(req);
    uint8_t data[14];
    (void)req_len;

    /* 按需求文档布局:
     *   [0] CPU 在位
     *   [1] 自检结果
     *   [2..3] 【2026-08-20借用】原"CPU利用率"字段,F103拿不到主机CPU利用率,
     *          恒为占位符0xFFFF,故借来存"累计运行小时数"(LE,见 bsp_eeprom.c
     *          的 g_runtime_total_minutes/Runtime_Task_Update)
     *   [4..5] 【2026-08-20借用】原"内存利用率"字段,同上理由,借来存
     *          "分钟余数0-59"(LE),两个字段合起来在网页拼成"XX小时YY分钟"
     *   [6..7] 带宽1 (LE) —— 仍是占位符,未借用
     *   [8..9] 带宽2 (LE) —— 仍是占位符,未借用
     *   [10] CPU 温度 (实际+128) —— 2026-08-18改为主机经串口上报的真实CPU温度
     *   [11] OS 版本
     *   [12] FW 版本
     *   [13] 电源开关状态(0=断电/1=上电,实时读PA4/PWR_CTL引脚)
     */
    data[0]  = 0xFF;        /* CPU 在位: 未检测 (F103 测不到 CPU) */
    data[1]  = 0x01;        /* 自检: 通过 */
    {
        /* g_runtime_total_minutes 是 uint32_t 永不溢出,但这里只有16位可用。
         * 65535 是这套协议里"未使用"的保留哨兵(网页按 cpu_util===65535 特判),
         * 真实小时数一旦达到65535(约7.48年)会跟这个哨兵撞车、被误显示成
         * "未使用",继续用uint16_t强转还会绕回0导致"累计"时长看起来归零。
         * 封顶在65534,超过后停住不再增长,不撞哨兵、不回绕。 */
        uint32_t total_hours = g_runtime_total_minutes / 60;
        uint16_t rt_hours = (total_hours > 65534u) ? (uint16_t)65534u : (uint16_t)total_hours;
        uint16_t rt_mins  = (uint16_t)(g_runtime_total_minutes % 60);
        data[2] = (uint8_t)(rt_hours);      data[3] = (uint8_t)(rt_hours >> 8);
        data[4] = (uint8_t)(rt_mins);       data[5] = (uint8_t)(rt_mins >> 8);
    }
    data[6]  = 0xFF; data[7] = 0xFF;  /* 带宽1 */
    data[8]  = 0xFF; data[9] = 0xFF;  /* 带宽2 */
    /* CPU 温度: 主机(飞腾D2000)经UART5每秒上报的真实CPU温度(即调试串口打印的
     * CpuTemp:xxC那个值), 见 task_usart.c UART5_Task 解析 + task_fan.c 缓存。
     * 沿用协议既有的 +128 编码; 从未收到过上报时填0xFF表示不可用(主机侧据此
     * 显示"未使用", 避免被解码成127℃当成真实读数)。
     * 板卡本体温度(LM75A@0x90)不在这里报了, 它走传感器轮询那条路(编号0x04)。 */
    {
        int host_cpu_t;
        (void)Fan_GetCpuTempDebug(&host_cpu_t);   /* 新鲜度这里不用, 只取最后一次收到的值 */
        data[10] = (host_cpu_t < 0) ? 0xFF : (uint8_t)(host_cpu_t + 128);
    }
    data[11] = 0xFF;        /* OS 版本 */
    data[12] = g_fru_state.ipmc_version; /* FW 版本 (用 IPMC 版本占位) */
    /* PA4(PWR_CTL)实时电平回读:低=上电=1,高=下电=0,见 引脚功能汇总.md。
     * 现读现填,不转发 g_fru_state.power_state 这个命令下发时的软件镜像 */
    data[13] = (GPIO_ReadInputDataBit(PWR_CTL_GPIO_PORT, PWR_CTL_GPIO_PIN) == Bit_RESET) ? 1 : 0;

    *resp_len = IPMB_Build_Response(resp, rqSA, IPMB_NETFN_APP, 0,
                                     own_addr, rqSeq, IPMB_CMD_GET_BOARD_STATUS,
                                     IPMB_CC_OK, data, sizeof(data));
}

void IPMB_Slave_Handle_GetSlot(const uint8_t* req, uint8_t req_len,
                                uint8_t* resp, uint8_t* resp_len,
                                uint8_t own_addr)
{
    uint8_t rqSA  = req[3];
    uint8_t rqSeq = IPMB_Extract_RqSeq(req);
    uint8_t data[1];
    (void)req_len;

    data[0] = read_slot_from_gpio();
    *resp_len = IPMB_Build_Response(resp, rqSA, IPMB_NETFN_APP, 0,
                                     own_addr, rqSeq, IPMB_CMD_GET_SLOT,
                                     IPMB_CC_OK, data, sizeof(data));
}

void IPMB_Slave_Handle_SetFRUActivation(const uint8_t* req, uint8_t req_len,
                                          uint8_t* resp, uint8_t* resp_len,
                                          uint8_t own_addr)
{
    uint8_t rqSA  = req[3];
    uint8_t rqLun = IPMB_Extract_RqLun(req);
    uint8_t rqSeq = IPMB_Extract_RqSeq(req);
    uint8_t cc, ret;
    uint8_t vso, fru_id, ctrl_req;
    uint8_t data[1];

    if (req_len < 9) {
        cc = IPMB_CC_INVALID_DATA_FIELD;
        data[0] = 0;
    } else {
        vso     = req[6];
        fru_id  = req[7];
        ctrl_req = req[8];
        (void)fru_id;
        if (vso != 0x03) {
            cc = IPMB_CC_INVALID_DATA_FIELD;
            data[0] = 0;
        } else {
            uint8_t sub_data_len = (req_len > 9) ? (req_len - 9) : 0;
            ret = IPMB_FRU_HandleControl(ctrl_req, &req[9], sub_data_len);
            if (ret == 0) {
                cc = IPMB_CC_OK;
            } else {
                cc = IPMB_CC_INVALID_DATA_FIELD;
            }
            data[0] = vso;
        }
    }

    *resp_len = IPMB_Build_Response(resp, rqSA, IPMB_NETFN_GROUP, rqLun,
                                     own_addr, rqSeq, IPMB_CMD_SET_FRU_ACTIVATION,
                                     cc, data, sizeof(data));
}

/**
 * @brief  Platform Event Message(cmd=0x02)—— 拼帧逻辑本身不区分调用来源,
 *         现在有两条触发路径共用这一个函数:
 *           1) 轮询答复:F407 每 5s 发一条 cmd=0x02 请求,经
 *              IPMB_Slave_ProcessRequest 分发到这里,req/req_len 非空(但本
 *              函数本来就不看它们,只读 g_fru_state 当前值实时汇报)。
 *           2) 自主推送(真正的"主动上报"):task_ipmb.c 的 IPMB_A_Task/
 *              IPMB_B_Task 在没有收到任何请求的空闲轮次里,发现
 *              g_pem_event_seq(ipmb_fru.c,由 IPMB_FRU_HandleControl 的
 *              阈值超限/正常 0x05/0x06 分支自增,手动测试命令和 ipmb_sensor.c
 *              的自动阈值检测器都走这同一个入口)变化,就直接以 req=NULL 调
 *              这个函数拿到帧,再临时切 I2C 主模式把帧 write 给主机——不等
 *              被轮询。硬件上这是可行的:I2C1/I2C2 本来就能临时切主模式(响应
 *              推送已经在用),没有"F103 没有硬件通路主动推送"这回事。
 *         两条路径吐出来的帧字节完全一样,协议上无法区分是轮询答复还是真正的
 *         自主推送,只能靠时机(有没有先收到匹配的请求)区分——这也是主机侧
 *         listener(task_i2c.c/bsp_i2c.c)的设计前提。
 *         帧内容:
 *           sensorNum  = 0(固定,阈值超限/正常这两个FRU控制码本身不带具体sensor号)
 *           eventType  = [7]=threshold_exceeded取反(0=超限中/事件产生,1=正常/事件消除)
 *                        [6:0]=6Fh(自定义状态事件)
 *           eventData1 = [7:4]=Ah固定 [3:0]=当前 sys_state
 *           eventData2 = [7:4]=cause(状态变化原因) [3:0]=之前 prev_sys_state
 *           eventData3 = FRU Device ID,固定0
 *         帧结构模拟协议原本"主动上报"的样子(F103作为发送方主动发出的请求
 *         形状的帧,不是响应,没有完成码字段):用 IPMB_Build_Request(而不是
 *         IPMB_Build_Response)拼帧,netFn 保持原始04h(不加响应位),rsSA/rqSA
 *         按"F103是发送方"填,源=F103自己。data共7字节,总帧固定14字节。
 *         【2026-07-28更正】上面"两条路径靠时机区分、内容完全一样"这个描述
 *         只对真正的自主推送(req==NULL)仍然成立;轮询答复(req!=NULL)这里改成
 *         跟其它所有命令 handler 一样回显请求里的 rqSA(目标地址)/rqSeq,不再
 *         无条件固定填0x20+自己的内部计数器——原因:主机侧如果不是用真实主控
 *         地址0x20发起轮询(比如网页控制台为了把响应分流到专属mailbox,用了
 *         其它标记地址),响应帧内容需要能对得上"这次到底是谁发的请求",不能
 *         永远看起来像是自主推送。真正走"时机拦截"这条路的只有自主推送
 *         (task_ipmb.c 里 g_pem_event_seq 变化触发、req==NULL 调用的那条),
 *         这条路径行为完全不变。用真实地址0x20轮询的既有调用方(F407那边
 *         IPMB_PEM_Poll_Task、手动 ipmb 命令)因为回显的还是0x20,行为也不变。
 */
void IPMB_Slave_Handle_PlatformEvent(const uint8_t* req, uint8_t req_len,
                                       uint8_t* resp, uint8_t* resp_len,
                                       uint8_t own_addr)
{
    static uint8_t s_pem_send_rqseq = 0;   /* 仅自主推送(req==NULL)用:没有真实请求可回显,
                                             * 用自己的内部计数器 */
    uint8_t data[7];
    uint8_t target_addr, send_rqseq;

    if (req != NULL && req_len >= 6) {
        target_addr = req[3];
        send_rqseq = IPMB_Extract_RqSeq(req);
    } else {
        target_addr = 0x20;
        send_rqseq = s_pem_send_rqseq;
        s_pem_send_rqseq = (uint8_t)((s_pem_send_rqseq + 1) & 0x3F);
    }
    (void)req_len;

    data[0] = 0x04;   /* EvmRev,固定 */
    data[1] = 0xF0;   /* 传感器类型 = State sensor,固定 */
    data[2] = 0x00;   /* 传感器编号,固定 */
    data[3] = (uint8_t)((g_fru_state.threshold_exceeded ? 0x00 : 0x80) | 0x6F);
    data[4] = (uint8_t)(0xA0 | (g_fru_state.sys_state & 0x0F));
    data[5] = (uint8_t)((g_fru_state.cause << 4) | (g_fru_state.prev_sys_state & 0x0F));
    data[6] = 0x00;   /* FRU Device ID,固定0 */
    *resp_len = IPMB_Build_Request(resp, target_addr, IPMB_NETFN_SENSOR, /*lun=*/0,
                                    /*rqSA(源,F103自己)=*/own_addr, send_rqseq,
                                    IPMB_CMD_PEM, data, sizeof(data));
}

/**
 * @brief  OEM Get Module Info (非标 netFn=0x2E, cmd=0x12)
 * @note   请求 netFn 字节 = 0x2E (不左移), 响应 netFn = (0x2E << 2) | lun (无响应位)
 *         固定 5 个传感器: 90Temp, 92Temp, V12, V0.81, Iout
 */
void IPMB_Slave_Handle_GetModuleInfo(const uint8_t* req, uint8_t req_len,
                                      uint8_t* resp, uint8_t* resp_len,
                                      uint8_t own_addr)
{
    uint8_t rqSA  = req[3];
    uint8_t rqLun = IPMB_Extract_RqLun(req);
    uint8_t rqSeq = IPMB_Extract_RqSeq(req);
    uint8_t item_count;
    uint8_t resp_data[108];  /* 最大: 1 + (3+80) + (3+20) + 1 = 108 */
    uint8_t data_len = 0;
    uint8_t cc = IPMB_CC_OK;
    uint8_t i, j;

    /* 提取请求 rqLun: 请求帧第 4 字节低 2 位 */
    {
        uint8_t rqLun_req = req[4] & 0x03;
        (void)rqLun_req;  /* 保留, 目前未使用 */
    }

    /* 验证最小长度: header(6) + item_count(1) + 至少 1 个 itemID = 8 */
    if (req_len < 8) {
        cc = IPMB_CC_INVALID_DATA_FIELD;
        goto build_resp;
    }

    item_count = req[6];

    /* 验证: item_count 非零 + 帧长度足够容纳所有 itemID */
    if (item_count == 0 || req_len < (uint8_t)(7 + item_count)) {
        cc = IPMB_CC_INVALID_DATA_FIELD;
        goto build_resp;
    }

    /* 响应数据: item_count(1) + 每个 item 的 [itemID(1)+status(1)+len(1)+data] */
    data_len = 1;  /* item_count 占位 */
    /* data_len 在后续追加每个 item 时累加 */

    for (i = 0; i < item_count && data_len < (uint8_t)(sizeof(resp_data) - 4); i++) {
        uint8_t item_id     = req[7 + i];
        uint8_t item_status = 0x00;
        uint8_t item_len    = 0;
        /* item 头部数据存放起始偏移: data_len 指向 resp_data 当前写位置, 预留 3 字节给头部 */
        uint8_t* item_start = &resp_data[data_len];
        uint8_t  sensor_idx;

        data_len += 3;  /* 为 itemID + status + len 预留 */

        switch (item_id) {
        case IPMB_OEM_ITEM_SENSOR_INFO:  /* 0x11: BIT 传感器信息 */
            item_len = IPMB_OEM_SENSOR_COUNT * 16;  /* 5 × 16 = 80 字节 */
            for (sensor_idx = 0; sensor_idx < IPMB_OEM_SENSOR_COUNT; sensor_idx++) {
                uint8_t* entry = &resp_data[data_len + sensor_idx * 16];
                /* 所有5个传感器均有真实采集值 */
                entry[0] = sensor_idx + 1;  /* sensorId: 1..5 */
                switch (sensor_idx) {
                case 0: entry[1] = IPMB_OEM_SENSOR_TYPE_TEMP; break;  /* 90Temp  */
                case 1: entry[1] = IPMB_OEM_SENSOR_TYPE_TEMP; break;  /* 92Temp  */
                case 2: entry[1] = IPMB_OEM_SENSOR_TYPE_VOLT; break;  /* V12     */
                case 3: entry[1] = IPMB_OEM_SENSOR_TYPE_VOLT; break;  /* V0.81   */
                case 4: entry[1] = IPMB_OEM_SENSOR_TYPE_CURR; break;  /* Iout    */
                default: entry[1] = 0xFF; break;
                }
                /* sensorName: 14 字节 */
                for (j = 0; j < 14; j++) {
                    entry[2 + j] = g_oem_sensor_names[sensor_idx][j];
                }
            }
            break;

        case IPMB_OEM_ITEM_SENSOR_VALUE:  /* 0x12: 所有 BIT 值 */
            item_len = IPMB_OEM_SENSOR_COUNT * 4;  /* 5 × 4 = 20 字节 */
            {
                int16_t reading;

                for (sensor_idx = 0; sensor_idx < IPMB_OEM_SENSOR_COUNT; sensor_idx++) {
                    uint8_t* entry = &resp_data[data_len + sensor_idx * 4];
                    uint8_t alarm = 0x00;

                    entry[0] = sensor_idx + 1;  /* sensorId */

                    switch (sensor_idx) {
                    case 0:  /* 90Temp — LM75A @ 0x90 */
                        Board_ADDR90_temp();
                        if (board_temp0[0] != -1) {
                            /* board_temp0[0]=整数°C, board_temp0[1]=百分位 */
                            reading = (int16_t)(board_temp0[0] * 10 + (board_temp0[1] + 5) / 10);
                        } else {
                            /* 温度传感器在位但读取失败(I2C无应答):告警类型用附录二表9
                             * 的合法值 8=未连接(原来写 0xFF 超出 0~8 定义范围) */
                            reading = (int16_t)0x7FFF;  /* 读值不可用 */
                            alarm = 0x08;               /* 未连接 */
                        }
                        break;

                    case 1:  /* 92Temp — LM75A @ 0x92 */
                        Board_ADDR92_temp();
                        if (board_temp1[0] != -1) {
                            reading = (int16_t)(board_temp1[0] * 10 + (board_temp1[1] + 5) / 10);
                        } else {
                            reading = (int16_t)0x7FFF;
                            alarm = 0x08;
                        }
                        break;

                    case 2:  /* V12 — ADC PA6 (index 0) */
                        reading = (int16_t)(stADC_Data.adc_voltage_val[0] * 10.0f + 0.5f);
                        break;

                    case 3:  /* V0.81 — ADC PC3+PC4 两路平均 (index 3, 4; 0.81V1/0.81V2) */
                        {
                            float v0 = stADC_Data.adc_voltage_val[3];
                            float v1 = stADC_Data.adc_voltage_val[4];
                            float v_avg = (v0 + v1) * 0.5f;
                            reading = (int16_t)(v_avg * 10.0f + 0.5f);  /* 分伏(dV), 分辨率0.1V, 与其余4路刻度统一 */
                        }
                        break;

                    case 4:  /* Iout — BPD20550 PMBus(缓存值,周期性更新) */
                        reading = (int16_t)(g_bpd20550_current_A * 10.0f + 0.5f);
                        break;

                    default:
                        entry[0] = 0xFF;
                        reading = (int16_t)0xFFFF;
                        alarm = 0xFF;
                        break;
                    }
                    /* 小端序 Int16 */
                    entry[1] = (uint8_t)(reading & 0xFF);
                    entry[2] = (uint8_t)((reading >> 8) & 0xFF);
                    entry[3] = alarm;
                }
            }
            break;

        default:
            /* 未知 itemID: 标记为无效, 无数据 */
            item_status = 0x01;
            item_len    = 0;
            data_len -= 3;  /* 回退预留的头部 */
            /* 就地写入头部 (itemID + status + len=0) */
            resp_data[data_len]     = item_id;
            resp_data[data_len + 1] = item_status;
            resp_data[data_len + 2] = 0;
            data_len += 3;
            continue;  /* 跳过下面的正常头部写入 */
        }

        /* 正常 item: 写入 itemID + itemStatus + itemLen 头部 */
        item_start[0] = item_id;
        item_start[1] = item_status;
        item_start[2] = item_len;
        data_len += item_len;
    }

    /* 写入实际的 item_count (可能因溢出或跳过而不同于请求) */
    resp_data[0] = (uint8_t)i;  /* 实际处理的 item 数 */

build_resp:
    if (cc == IPMB_CC_OK && data_len == 0) {
        cc = IPMB_CC_INVALID_DATA_FIELD;
    }
    *resp_len = oem_build_module_response(resp, rqSA, own_addr, rqSeq, rqLun,
                                           cc, resp_data, data_len);
}

/* =================== 协议分发 =================== */

uint8_t IPMB_Slave_ProcessRequest(const uint8_t* rx_buf, uint8_t rx_len,
                                   uint8_t* tx_buf, uint8_t tx_max,
                                   uint8_t own_addr)
{
    uint8_t cmd;
    uint8_t resp_len = 0;

    if (rx_buf == NULL || tx_buf == NULL) return 0;
    if (rx_len < IPMB_MIN_FRAME_LEN) return 0;
    if (IPMB_Verify_Checksum(rx_buf, rx_len) != 0) {
        return 0;
    }
    (void)tx_max;

    cmd = rx_buf[5];

    switch (cmd) {
    case IPMB_CMD_GET_DEVICE_ID:
        IPMB_Slave_Handle_GetDeviceID(rx_buf, rx_len, tx_buf, &resp_len, own_addr);
        break;
    case IPMB_CMD_GET_SDR_INFO:
        IPMB_Slave_Handle_GetDeviceSDR(rx_buf, rx_len, tx_buf, &resp_len, own_addr);
        break;
    case IPMB_CMD_GET_SDR:
        IPMB_Slave_Handle_GetSDR(rx_buf, rx_len, tx_buf, &resp_len, own_addr);
        break;
    case IPMB_CMD_GET_SENSOR_READING:
        IPMB_Slave_Handle_GetSensorReading(rx_buf, rx_len, tx_buf, &resp_len, own_addr);
        break;
    case IPMB_CMD_GET_BOARD_STATUS:
        IPMB_Slave_Handle_GetBoardStatus(rx_buf, rx_len, tx_buf, &resp_len, own_addr);
        break;
    case IPMB_CMD_GET_SLOT:
        IPMB_Slave_Handle_GetSlot(rx_buf, rx_len, tx_buf, &resp_len, own_addr);
        break;
    case IPMB_CMD_SET_FRU_ACTIVATION:
        IPMB_Slave_Handle_SetFRUActivation(rx_buf, rx_len, tx_buf, &resp_len, own_addr);
        break;
    case IPMB_CMD_PEM:
        IPMB_Slave_Handle_PlatformEvent(rx_buf, rx_len, tx_buf, &resp_len, own_addr);
        break;
    case IPMB_CMD_GET_MODULE_INFO:
        IPMB_Slave_Handle_GetModuleInfo(rx_buf, rx_len, tx_buf, &resp_len, own_addr);
        break;
    default:
        {
            uint8_t rqSA  = rx_buf[3];
            uint8_t rqLun = IPMB_Extract_RqLun(rx_buf);
            uint8_t rqSeq = IPMB_Extract_RqSeq(rx_buf);
            resp_len = IPMB_Build_Response(tx_buf, rqSA, IPMB_EXTRACT_NETFN(rx_buf[1]),
                                           rqLun, own_addr, rqSeq, cmd,
                                           IPMB_CC_INVALID_COMMAND, NULL, 0);
        }
        break;
    }

    return resp_len;
}

