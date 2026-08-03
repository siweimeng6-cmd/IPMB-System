#include "ipmb_sensor.h"
#include "ipmb_fru.h"
#include "bsp_mo_i2c.h"     /* board_temp0/1, Board_ADDR90_temp/92_temp */
#include "bsp_adc.h"        /* stADC_Data */
#include "bsp_bpd20550.h"   /* g_bpd20550_current_A */
#include <string.h>
#include <stdio.h>

/* 协议 sensor num 表 (按需求文档):
 *  温度: 04h, 20h, 21h, 22h
 *  电压: 03h(12V), 10h(0.95V), 11h(1.0V), 12h(1.2V), 13h(1.5V), 14h(1.8V), 15h(2.5V), 16h(3.3V), 17h(5V), 19h(0.75V), 31h(1.1V),
 *        50h..66h (扩展)
 *  电流: 07h(12V/40A), 18h(3.3V/10A), 30h(5V/15A), 59h(12V-1/40A), 5Ah(5V-1/15A), 5Bh(3.3V-1/10A)
 *  风扇: 41h
 */
#define IPMB_SENSOR_TABLE_SIZE       48
static IPMB_Sensor_Entry_t g_sensor_table[IPMB_SENSOR_TABLE_SIZE];
static uint8_t g_sensor_count = 0;

static void sensor_add(uint8_t num, uint8_t type, uint8_t unit)
{
    if (g_sensor_count >= IPMB_SENSOR_TABLE_SIZE) return;
    IPMB_Sensor_Entry_t* e = &g_sensor_table[g_sensor_count++];
    e->sensor_num      = num;
    e->sensor_type     = type;
    e->unit_type       = unit;
    e->last_reading    = IPMB_READING_UNAVAILABLE;
    e->upper_nonrecov  = IPMB_THRESHOLD_UNAVAILABLE;
    e->upper_crit      = IPMB_THRESHOLD_UNAVAILABLE;
    e->upper_noncrit   = IPMB_THRESHOLD_UNAVAILABLE;
    e->lower_noncrit   = IPMB_THRESHOLD_UNAVAILABLE;
    e->lower_crit      = IPMB_THRESHOLD_UNAVAILABLE;
    e->lower_nonrecov  = IPMB_THRESHOLD_UNAVAILABLE;
}

void IPMB_Sensor_Init(void)
{
    uint8_t i;
    g_sensor_count = 0;
    memset(g_sensor_table, 0, sizeof(g_sensor_table));

    /* 温度 */
    sensor_add(0x04, IPMB_SENSOR_TYPE_TEMPERATURE, IPMB_UNIT_DEGREES_C);
    sensor_add(0x20, IPMB_SENSOR_TYPE_TEMPERATURE, IPMB_UNIT_DEGREES_C);
    sensor_add(0x21, IPMB_SENSOR_TYPE_TEMPERATURE, IPMB_UNIT_DEGREES_C);
    sensor_add(0x22, IPMB_SENSOR_TYPE_TEMPERATURE, IPMB_UNIT_DEGREES_C);

    /* 电压 (单路) */
    sensor_add(0x03, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 12V */
    sensor_add(0x10, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 0.95V */
    sensor_add(0x11, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 1.0V */
    sensor_add(0x12, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 1.2V */
    sensor_add(0x13, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 1.5V */
    sensor_add(0x14, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 1.8V */
    sensor_add(0x15, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 2.5V */
    sensor_add(0x16, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 3.3V */
    sensor_add(0x17, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 5V */
    sensor_add(0x19, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 0.75V */
    sensor_add(0x31, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);  /* 1.1V */

    /* 电压 (扩展 50h..66h) */
    for (i = 0x50; i <= 0x66; i++) {
        sensor_add(i, IPMB_SENSOR_TYPE_VOLTAGE, IPMB_UNIT_SIGNED);
    }

    /* 电流 */
    sensor_add(0x07, IPMB_SENSOR_TYPE_CURRENT, IPMB_UNIT_SIGNED);  /* 12V/40A */
    sensor_add(0x18, IPMB_SENSOR_TYPE_CURRENT, IPMB_UNIT_SIGNED);  /* 3.3V/10A */
    sensor_add(0x30, IPMB_SENSOR_TYPE_CURRENT, IPMB_UNIT_SIGNED);  /* 5V/15A */
    sensor_add(0x59, IPMB_SENSOR_TYPE_CURRENT, IPMB_UNIT_SIGNED);  /* 12V-1/40A */
    sensor_add(0x5A, IPMB_SENSOR_TYPE_CURRENT, IPMB_UNIT_SIGNED);  /* 5V-1/15A */
    sensor_add(0x5B, IPMB_SENSOR_TYPE_CURRENT, IPMB_UNIT_SIGNED);  /* 3.3V-1/10A */

    /* 风扇 */
    sensor_add(0x41, IPMB_SENSOR_TYPE_FAN, IPMB_UNIT_UNSIGNED);
}

uint8_t IPMB_Sensor_Find(uint8_t sensor_num, IPMB_Sensor_Entry_t** out)
{
    uint8_t i;
    for (i = 0; i < g_sensor_count; i++) {
        if (g_sensor_table[i].sensor_num == sensor_num) {
            *out = &g_sensor_table[i];
            return 0;
        }
    }
    return 1;
}

uint8_t IPMB_Sensor_Update(uint8_t sensor_num, int16_t reading)
{
    IPMB_Sensor_Entry_t* e;
    if (IPMB_Sensor_Find(sensor_num, &e) == 0) {
        e->last_reading = reading;
        return 0;
    }
    return 1;
}

/**
 * @brief  阈值检测结果通知:统一走 IPMB_FRU_HandleControl 的 0x05/0x06 状态机
 *         (和手动测试命令"阈值超限/正常"共用同一条通路),该函数内部会把
 *         g_pem_event_seq 自增一次,task_ipmb.c 的 IPMB_A_Task/IPMB_B_Task
 *         据此触发一次真正的主动 PEM 推送。
 * @param  sensor_num  触发/解除的传感器协议编号(仅用于打印,g_fru_state 本身
 *                      是单一状态标志,不区分具体哪路传感器,维持既有设计不改)
 * @param  event_high  assert=1 时有效:1=高阈值超限(THRESHOLD_HI),0=低阈值(THRESHOLD_LO)
 * @param  assert      1=超限断言,0=解除(恢复正常)
 */
void IPMB_Sensor_Notify_Threshold(uint8_t sensor_num, uint8_t event_high, uint8_t assert)
{
    if (assert) {
        /* IPMB_FRU_HandleControl 的 case 0x05 只看"有没有带 data"(data!=NULL &&
         * data_len>0)来决定 HI/LO,不看 data 的具体值 —— 既有实现如此,这里照做 */
        printf("[SENSOR] sensor 0x%02X threshold %s\r\n", sensor_num, event_high ? "HIGH exceeded" : "LOW exceeded");
        if (event_high) {
            uint8_t dummy = 1;
            IPMB_FRU_HandleControl(0x05, &dummy, 1);
        } else {
            IPMB_FRU_HandleControl(0x05, NULL, 0);
        }
    } else {
        printf("[SENSOR] sensor 0x%02X threshold cleared\r\n", sensor_num);
        IPMB_FRU_HandleControl(0x06, NULL, 0);
    }
}

/**
 * @brief  真正的自动阈值检测:每次调用对 90Temp/92Temp/V12/V0.75/Iout 五路做
 *         迟滞比较,用 s_exceeded_mask 记录"当前处于超限状态"的传感器集合,
 *         只在 0→非0/非0→0 的跳变沿触发一次 IPMB_Sensor_Notify_Threshold,
 *         避免同一持续状态反复推送。门限定义见 ipmb_sensor.h(占位值)。
 */
void IPMB_Sensor_CheckThresholds(void)
{
    static uint8_t s_exceeded_mask = 0;
    uint8_t new_mask = 0;
    uint8_t changed_high = 0;   /* 本次新触发的那一路里, 是否是"高阈值" */
    uint8_t tripped_sensor = 0;

    /* 90Temp (LM75A@0x90), 协议 sensor_num = 0x04 */
    Board_ADDR90_temp();
    if (board_temp0[0] >= IPMB_TH_TEMP_ASSERT_C) {
        new_mask |= 0x01;
        if (!(s_exceeded_mask & 0x01)) { tripped_sensor = 0x04; changed_high = 1; }
    } else if (board_temp0[0] <= IPMB_TH_TEMP_CLEAR_C) {
        /* 低于恢复门限: 不置位 */
    } else if (s_exceeded_mask & 0x01) {
        new_mask |= 0x01;   /* 迟滞区间内维持原状态 */
    }

    /* 92Temp (LM75A@0x92), 协议 sensor_num = 0x20 */
    Board_ADDR92_temp();
    if (board_temp1[0] >= IPMB_TH_TEMP_ASSERT_C) {
        new_mask |= 0x02;
        if (!(s_exceeded_mask & 0x02)) { tripped_sensor = 0x20; changed_high = 1; }
    } else if (board_temp1[0] <= IPMB_TH_TEMP_CLEAR_C) {
        /* 恢复 */
    } else if (s_exceeded_mask & 0x02) {
        new_mask |= 0x02;
    }

    /* V12 (ADC PA6), 协议 sensor_num = 0x03 */
    {
        float v12 = stADC_Data.adc_voltage_val[0];
        if (v12 < IPMB_TH_V12_LOW_ASSERT || v12 > IPMB_TH_V12_HIGH_ASSERT) {
            new_mask |= 0x04;
            if (!(s_exceeded_mask & 0x04)) { tripped_sensor = 0x03; changed_high = (v12 > IPMB_TH_V12_HIGH_ASSERT); }
        } else if (v12 >= IPMB_TH_V12_LOW_CLEAR && v12 <= IPMB_TH_V12_HIGH_CLEAR) {
            /* 恢复 */
        } else if (s_exceeded_mask & 0x04) {
            new_mask |= 0x04;
        }
    }

    /* V0.75 (ADC PA7+PB0 平均), 协议 sensor_num = 0x19 */
    {
        float v075 = (stADC_Data.adc_voltage_val[1] + stADC_Data.adc_voltage_val[2]) / 2.0f;
        if (v075 < IPMB_TH_V075_LOW_ASSERT || v075 > IPMB_TH_V075_HIGH_ASSERT) {
            new_mask |= 0x08;
            if (!(s_exceeded_mask & 0x08)) { tripped_sensor = 0x19; changed_high = (v075 > IPMB_TH_V075_HIGH_ASSERT); }
        } else if (v075 >= IPMB_TH_V075_LOW_CLEAR && v075 <= IPMB_TH_V075_HIGH_CLEAR) {
            /* 恢复 */
        } else if (s_exceeded_mask & 0x08) {
            new_mask |= 0x08;
        }
    }

    /* Iout (BPD20550 PMBus, 12V/40A 路), 协议 sensor_num = 0x07 */
    if (g_bpd20550_current_A >= IPMB_TH_IOUT_ASSERT_A) {
        new_mask |= 0x10;
        if (!(s_exceeded_mask & 0x10)) { tripped_sensor = 0x07; changed_high = 1; }
    } else if (g_bpd20550_current_A <= IPMB_TH_IOUT_CLEAR_A) {
        /* 恢复 */
    } else if (s_exceeded_mask & 0x10) {
        new_mask |= 0x10;
    }

    if (new_mask != 0 && s_exceeded_mask == 0) {
        /* 0 -> 非0: 新出现超限, 推一条 assert 事件 */
        IPMB_Sensor_Notify_Threshold(tripped_sensor, changed_high, 1);
    } else if (new_mask == 0 && s_exceeded_mask != 0) {
        /* 非0 -> 0: 全部恢复, 推一条 clear 事件 */
        IPMB_Sensor_Notify_Threshold(0, 0, 0);
    }
    /* 其余情况(仍有其它传感器超限中, 或本来就没有超限): 只更新掩码, 不重复推送 */

    s_exceeded_mask = new_mask;
}
