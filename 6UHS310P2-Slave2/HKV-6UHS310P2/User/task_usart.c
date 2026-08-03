#include "task_usart.h"
#include "bsp_init.h"
#include "bsp_i2c.h"
#include "bsp_mo_i2c.h"
#include "bsp_adc.h"
#include "bsp_usart.h"
#include "stm32f10x.h"
#include "stm32f10x_i2c.h"
#include ".\timer\bsp_pwm.h"
#include ".\ipmb\bsp_i2c_slave.h"
#include ".\ipmb\ipmb_slave.h"
#include "bsp_bpd20550.h"
#include <stdio.h>
#include <string.h>

/* 诊断 */
extern volatile uint32_t g_uart4_rx_byte_cnt;
extern volatile uint32_t g_uart4_isr_cnt;
extern volatile uint32_t g_uart4_line_cnt;

TaskHandle_t Sensor_Task_Handle = NULL;
stPRINTF_BUF_t stCpu_Buf;

/*
*********************************************************************************************************
*	函 数 名: Sensor_Task
*	功能说明: Sensor_Task任务主体
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void Sensor_Task(void* parameter)
{
//Debug串口发送信息到CPU测试
//	uint8_t send_data[] = "123456";
    
//    printf("Sensor_Task 开始运行\r\n");
//    
	while (1)
  {
    // 清空打印缓冲区
    memset(stPrintf_Buf.buf, 0, sizeof(stPrintf_Buf.buf));   
    // 1. 计算电压值
    ADC_Calculation();   
    // 2. 采集温度
    Board_ADDR90_temp();
    Board_ADDR92_temp();
    // 3. 温度控制风扇PWM   打印风扇信息
    pwm_func();   
    // 4. 打印所有值
    printf("\r\n********************* System State *********************\r\n");
    printf("项目名:HKV-6UHS310P2\r\n");  
    printf("版本号:V1.1\r\n");
    printf("编译日期2026年3月11日\r\n");
    printf("初始化完成\r\n");

    printf("槽位号:%u\r\n", read_slot_from_gpio());
    printf("  GA0(PA8) =%u\r\n", GPIO_ReadInputDataBit(GA0_GPIO_PORT, GA0_GPIO_PIN));
    printf("  GA1(PA9) =%u\r\n", GPIO_ReadInputDataBit(GA1_GPIO_PORT, GA1_GPIO_PIN));
    printf("  GA2(PA10)=%u\r\n", GPIO_ReadInputDataBit(GA2_GPIO_PORT, GA2_GPIO_PIN));
    printf("  GA3(PA11)=%u\r\n", GPIO_ReadInputDataBit(GA3_GPIO_PORT, GA3_GPIO_PIN));
    printf("  GA4(PC6) =%u\r\n", GPIO_ReadInputDataBit(GA4_GPIO_PORT, GA4_GPIO_PIN));
    printf("  GAP(PC7) =%u\r\n", GPIO_ReadInputDataBit(GAP_GPIO_PORT, GAP_GPIO_PIN));

    printf("打印参数说明：\r\n");
    printf("温度值：ADDR0x90, ADDR0x92\r\n");
    printf("电压值：12V_HOT_Mointor, 0.75V_DDRPHY_M, 0.75V_DDRPHY_S, 3.8V_V_VSYS_Mointor, 3.3V_VRD_MOS_Mointor\r\n");

    /* IPMB 在线状态 */
    {
        uint32_t tick_a = g_i2c_slave_a.last_active_tick;
        uint32_t tick_b = g_i2c_slave_b.last_active_tick;
        uint32_t now    = xTaskGetTickCount();

        printf("IPMB-A (I2C1 addr=0x%02X): %s\r\n",
               g_i2c_slave_a.slave_addr,
               (now - tick_a < pdMS_TO_TICKS(3000)) ? "在线" : "离线");
        printf("IPMB-B (I2C2 addr=0x%02X): %s\r\n",
               g_i2c_slave_b.slave_addr,
               (now - tick_b < pdMS_TO_TICKS(3000)) ? "在线" : "离线");
        printf("UART4 diag: ISR=%lu  bytes=%lu  lines=%lu\r\n",
               g_uart4_isr_cnt, g_uart4_rx_byte_cnt, g_uart4_line_cnt);
    }
      
//    // 打印风扇信息
//    uint8_t pwm_duty = pwm_get_duty();
//    printf("风扇PWM占空比: %d%%\r\n", pwm_duty);
    
    printf("%s", stPrintf_Buf.buf);

    /* BPD20550 (PB8/PB9 bit-bang I2C): MFR_ID / Vin / Iout / Temp */
    {
        char bpd_buf[192];
        bpd_buf[0] = '\0';
        Board_BPD20550_current(bpd_buf);
        printf("%s", bpd_buf);
    }

    /* 真正的自动阈值检测:复用本轮已经采到的 board_temp0/1、ADC、BPD20550
     * 数据,跳变时会触发 IPMB_A_Task/IPMB_B_Task 主动推一条 PEM 帧给主机
     * (见 ipmb_sensor.c / task_ipmb.c) */
    IPMB_Sensor_CheckThresholds();

    printf("********************************************************\r\n");
//    
////    // 发送数据到CPU
////    USART5_SendData(send_data, sizeof(send_data) - 1);
////    printf("USART5发送数据到CPU成功: 123456\r\n");
    
    vTaskDelay(2000); // 每1秒发送一次
    }
}



