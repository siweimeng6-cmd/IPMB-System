#include "ipmb_sensor.h"
#include "ipmb_fru.h"
#include "ipmb_threshold.h" /* g_threshold_config */
#include "bsp_mo_i2c.h"     /* board_temp0/1/2, Board_ADDR90/92/94_temp */
#include "bsp_adc.h"        /* stADC_Data */
#include "bsp_bpd20550.h"   /* g_bpd20550_current_A */
#include ".\gpio\bsp_gpio.h" /* PWR_CTL_GPIO_PORT/PIN, mainboard_power_state, power_state_confirmed */
#include "FreeRTOS.h"       /* taskENTER_CRITICAL/EXIT_CRITICAL */
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

/* 断电触发后锁存,后续调用直接跳过检测,直到 IPMB_Sensor_ClearShutdownLatch
 * 被调用(ipmb_fru.c case 0x01 上电时)才恢复。跟持续超限计数一样是文件级状态,
 * 不放进函数内 static 是因为清除函数需要从外部重置它们。 */
static uint8_t s_shutdown_latched = 0;
static uint8_t s_alarm_active = 0;
static uint8_t s_temp_shutdown_persist = 0;
static uint8_t s_volt_shutdown_persist = 0;

/* 6路电压各自的标称值,跟 sensor_num 的对应关系见 ipmb_slave.c
 * IPMB_Slave_Handle_GetSensorReading 里 adc_voltage_val[] 下标的注释:
 * idx0=12V(0x03) idx1=5V(0x17) idx2=3.3V(0x16) idx3=0.81V-1(0x19)
 * idx4=0.81V-2(0x10) idx5=1.2V(0x12)。这是传感器物理属性,不需要用户配置,
 * 用户只配置报警/断电百分比,乘上这里的标称值折算出实际上下限。 */
static const float s_volt_nominal[6] = { 12.0f, 5.0f, 3.3f, 0.81f, 0.81f, 1.2f };

/**
 * @brief  真正的自动阈值检测(2026-08-21重写,不再是占位阈值):对3路温度
 *         (board_temp0/1/2)和6路电压(stADC_Data.adc_voltage_val[0..5])做检测,
 *         阈值来自运行时可配置的 g_threshold_config。报警(alarm)0→1/1→0跳变时
 *         触发一次 IPMB_Sensor_Notify_Threshold;断电(shutdown)阈值需要连续
 *         IPMB_THRESHOLD_SHUTDOWN_PERSIST_ROUNDS 轮都超限才真正触发,避免传感器
 *         瞬时噪声误触发,触发后锁存直到重新上电。电流(0x07)这一路目前底层采集
 *         从未被调用、数据不可信,这次不纳入检测。
 */
void IPMB_Sensor_CheckThresholds(void)
{
    uint8_t i;
    uint8_t temp_alarm = 0, temp_shutdown = 0;
    uint8_t volt_alarm = 0, volt_shutdown = 0;
    uint8_t any_alarm;
    int *temps[3];

    if (s_shutdown_latched) return;   /* 已经触发过断电,等待人工重新上电 */

    temps[0] = board_temp0;
    temps[1] = board_temp1;
    temps[2] = board_temp2;
    for (i = 0; i < 3; i++)
    {
        int *t = temps[i];
        if (t[0] == -1 && t[1] == 0) continue;   /* 该路从未成功读取过, 跳过 */
        if (t[0] > g_threshold_config.temp_alarm_high || t[0] < g_threshold_config.temp_alarm_low)
            temp_alarm = 1;
        if (t[0] > g_threshold_config.temp_shutdown_high || t[0] < g_threshold_config.temp_shutdown_low)
            temp_shutdown = 1;
    }

    for (i = 0; i < 6; i++)
    {
        float v = stADC_Data.adc_voltage_val[i];
        float nominal = s_volt_nominal[i];
        float alarm_band = nominal * (float)g_threshold_config.volt_alarm_percent / 100.0f;
        float shutdown_band = nominal * (float)g_threshold_config.volt_shutdown_percent / 100.0f;
        if (v > nominal + alarm_band || v < nominal - alarm_band) volt_alarm = 1;
        if (v > nominal + shutdown_band || v < nominal - shutdown_band) volt_shutdown = 1;
    }

    any_alarm = (uint8_t)(temp_alarm || volt_alarm);
    if (any_alarm != s_alarm_active)
    {
        /* sensor_num=0(不区分具体哪路,既有设计如此) event_high=1(占位,HandleControl
         * 的0x05分支只看有没有带data来判HI/LO,不看具体值) */
        IPMB_Sensor_Notify_Threshold(0, 1, any_alarm);
        s_alarm_active = any_alarm;
    }

    if (temp_shutdown) { if (s_temp_shutdown_persist < 255) s_temp_shutdown_persist++; }
    else s_temp_shutdown_persist = 0;
    if (volt_shutdown) { if (s_volt_shutdown_persist < 255) s_volt_shutdown_persist++; }
    else s_volt_shutdown_persist = 0;

    if (s_temp_shutdown_persist >= IPMB_THRESHOLD_SHUTDOWN_PERSIST_ROUNDS ||
        s_volt_shutdown_persist >= IPMB_THRESHOLD_SHUTDOWN_PERSIST_ROUNDS)
    {
        /* 断电动作本身照抄 ipmb_fru.c case 0x00,但两处故意不一样:cause改成
         * SHUTDOWN_THRESHOLD而不是USER_REQUEST(所以不直接复用
         * IPMB_FRU_HandleControl(0x00,...));sys_state改成M6(关键故障)而不是
         * case 0x00那样的M0(2026-08-21改,原来跟人为断电一样设M0,会跟同一屏的
         * "报警中"/"阈值触发断电"放一起显示成自相矛盾的"正常",改成M6后网页能
         * 一眼看出这是异常断电,不用非得去看原因码那一行)。手动重新上电
         * (ipmb_fru.c case 0x01)会把sys_state重置回M0。顺带
         * g_pem_event_seq++ 保证这次断电对主控可见(现有case 0x00本身不会推PEM)。
         * 这几个共享状态目前项目里没有锁保护(既有的潜在竞态),这里是第4个写者,
         * 加临界区保护,降低无人值守断电场景下的风险。 */
        taskENTER_CRITICAL();
        g_fru_state.prev_sys_state = g_fru_state.sys_state;
        g_fru_state.sys_state = IPMB_STATE_M6;
        g_fru_state.cause = IPMB_CAUSE_SHUTDOWN_THRESHOLD;
        g_fru_state.power_state = 0;
        g_fru_state.threshold_exceeded = 1;
        GPIO_SetBits(PWR_CTL_GPIO_PORT, PWR_CTL_GPIO_PIN);
        mainboard_power_state = 0;
        power_state_confirmed = 1;
        g_pem_event_seq++;
        taskEXIT_CRITICAL();

        s_shutdown_latched = 1;
        printf("[SENSOR] AUTO SHUTDOWN: threshold persisted %u rounds (temp=%u volt=%u)\r\n",
               (unsigned)IPMB_THRESHOLD_SHUTDOWN_PERSIST_ROUNDS,
               (unsigned)temp_shutdown, (unsigned)volt_shutdown);
    }
}

void IPMB_Sensor_ClearShutdownLatch(void)
{
    s_shutdown_latched = 0;
    s_temp_shutdown_persist = 0;
    s_volt_shutdown_persist = 0;
}


