#include ".\gpio\bsp_gpio.h"
#include <stdio.h>
#include "core_cm3.h"
#include "bsp_usart.h"
#include "bsp_init.h"


// 定义外部变量
TaskHandle_t GPIO_Task_Handle;

/**
 * @brief  GPIO初始化函数
 * @param  无
 * @retval 无
 */
void bsp_gpio_init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    
    // 使能GPIO时钟
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOD, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_AFIO, ENABLE);    
    // 禁用JTAG，释放PA15作为普通GPIO使用
    GPIO_PinRemapConfig(GPIO_Remap_SWJ_JTAGDisable, ENABLE);
/**********************************PA端口初始化***********************************************/
    
    // PA1 - ALERT_JX 告警灯控制输出信号  输出低电平电量等  输出高电平熄灭灯
    GPIO_InitStructure.GPIO_Pin = ALERT_JX_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(ALERT_JX_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(ALERT_JX_GPIO_PORT, ALERT_JX_GPIO_PIN);  // 初始化为低电平（灯亮）

    // PA2 - IPMB_A_EN  NCA9511 使能 (IPMB-A 总线 buffer), 高电平有效
    GPIO_InitStructure.GPIO_Pin = IPMB_A_EN_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(IPMB_A_EN_GPIO_PORT, &GPIO_InitStructure);
    GPIO_SetBits(IPMB_A_EN_GPIO_PORT, IPMB_A_EN_GPIO_PIN);  // 上电直接使能

    // PA3 - IPMB_B_EN  NCA9511 使能 (IPMB-B 总线 buffer), 高电平有效
    GPIO_InitStructure.GPIO_Pin = IPMB_B_EN_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(IPMB_B_EN_GPIO_PORT, &GPIO_InitStructure);
    GPIO_SetBits(IPMB_B_EN_GPIO_PORT, IPMB_B_EN_GPIO_PIN);  // 上电直接使能
    
    
    // PA4 - PWR_CTL    
    GPIO_InitStructure.GPIO_Pin = PWR_CTL_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PWR_CTL_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(PWR_CTL_GPIO_PORT, PWR_CTL_GPIO_PIN);  // 初始化为低电平
    
    // PA8 - GA0   槽位号
    GPIO_InitStructure.GPIO_Pin = GA0_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // input pull-up (read backplane slot)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GA0_GPIO_PORT, &GPIO_InitStructure);

    // PA9 - GA1    槽位号
    GPIO_InitStructure.GPIO_Pin = GA1_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // input pull-up (read backplane slot)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GA1_GPIO_PORT, &GPIO_InitStructure);

    // PA10 - GA2   槽位号
    GPIO_InitStructure.GPIO_Pin = GA2_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // input pull-up (read backplane slot)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GA2_GPIO_PORT, &GPIO_InitStructure);

    // PA11 - GA3   槽位号
    GPIO_InitStructure.GPIO_Pin = GA3_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // input pull-up (read backplane slot)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GA3_GPIO_PORT, &GPIO_InitStructure);
    
    // PA12 - BMC_CTRL_SYSRST 
    GPIO_InitStructure.GPIO_Pin = BMC_CTRL_SYSRST_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(BMC_CTRL_SYSRST_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(BMC_CTRL_SYSRST_GPIO_PORT, BMC_CTRL_SYSRST_GPIO_PIN);  // 初始化复位控制引脚为低电平（正常状态）
    
    // PA15 - LED_BMC_STA 状态灯--闪烁频率2hz
    GPIO_InitStructure.GPIO_Pin = LED_BMC_STA_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_BMC_STA_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(LED_BMC_STA_GPIO_PORT, LED_BMC_STA_GPIO_PIN); //初始化低（灯亮）
    
/******************************************PB端口初始化*****************************************/

    
/******************************************PC端口初始化*****************************************/
    // PC4 - LED_BMC_RESERVE 闪烁频率2hz 
    GPIO_InitStructure.GPIO_Pin = LED_BMC_RESERVE_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;  // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(LED_BMC_RESERVE_GPIO_PORT, &GPIO_InitStructure);
    GPIO_ResetBits(LED_BMC_RESERVE_GPIO_PORT, LED_BMC_RESERVE_GPIO_PIN); //初始化低（灯亮）
    
    // PC6 - GA4    槽位号
    GPIO_InitStructure.GPIO_Pin = GA4_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // input pull-up (read backplane slot)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GA4_GPIO_PORT, &GPIO_InitStructure);

    // PC7 - GAP    槽位号
    GPIO_InitStructure.GPIO_Pin = GAP_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // input pull-up (read backplane slot)
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GAP_GPIO_PORT, &GPIO_InitStructure);
    
    // PC5 - VP0_SYSRESET# 按键控制引脚
    GPIO_InitStructure.GPIO_Pin = VP0_SYSRESET_GPIO_PIN;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;  // 上拉输入
    GPIO_Init(VP0_SYSRESET_GPIO_PORT, &GPIO_InitStructure);
}


/**
 * @brief  GPIO任务处理函数
 * @param  parameter: 任务参数
 * @retval 无
 */
void GPIO_Task(void* parameter)
{
    
    uint8_t last_key_state = 1; // 初始状态为高电平（未按下）
    
    while(1)
    {
        // 1. LED闪烁（2Hz）、LED_BMC_RESERVE、ALERT_JX 告警灯（2Hz）测试用
        GPIO_ResetBits(LED_BMC_STA_GPIO_PORT, LED_BMC_STA_GPIO_PIN); // 亮
        vTaskDelay(250);
        GPIO_SetBits(LED_BMC_STA_GPIO_PORT, LED_BMC_STA_GPIO_PIN); // 灭
        vTaskDelay(250);
        
        GPIO_ResetBits(ALERT_JX_GPIO_PORT, ALERT_JX_GPIO_PIN); 
        vTaskDelay(250);
        GPIO_SetBits(ALERT_JX_GPIO_PORT, ALERT_JX_GPIO_PIN); 
        vTaskDelay(250);
               
        GPIO_ResetBits(LED_BMC_RESERVE_GPIO_PORT, LED_BMC_RESERVE_GPIO_PIN); 
        vTaskDelay(250);
        GPIO_SetBits(LED_BMC_RESERVE_GPIO_PORT, LED_BMC_RESERVE_GPIO_PIN);
        vTaskDelay(250);
        
        // 2. 按键消抖和复位按键
        //读取按键状态
        uint8_t current_key_state = GPIO_ReadInputDataBit(VP0_SYSRESET_GPIO_PORT, VP0_SYSRESET_GPIO_PIN);
        
        //按键消抖和状态检测
        if (current_key_state != last_key_state)
        {
            // 等待消抖
            vTaskDelay(50); // 减少消抖时间，提高灵敏度
            // 再次读取状态
            current_key_state = GPIO_ReadInputDataBit(VP0_SYSRESET_GPIO_PORT, VP0_SYSRESET_GPIO_PIN);
            
            if (current_key_state != last_key_state)
            {
                last_key_state = current_key_state;
                
                if (current_key_state == 0) // 按键按下，PC5为低电平
                {
                    printf("按键按下\r\n");
                    // 拉高PA12
                    GPIO_SetBits(BMC_CTRL_SYSRST_GPIO_PORT, BMC_CTRL_SYSRST_GPIO_PIN);
                }
                else if (current_key_state == 1) // 按键松开，PC5为高电平
                {
                    printf("按键松开\r\n");
                    // 拉低PA12
                    GPIO_ResetBits(BMC_CTRL_SYSRST_GPIO_PORT, BMC_CTRL_SYSRST_GPIO_PIN);
                }
            }
        }
         
        //3.处理上下电指令 
        // 处理poweron命令（上电）
        if(poweron_flag == 1)
        {
            poweron_flag = 0;
            printf("收到poweron命令，准备控制主板上电...\r\n");
            GPIO_ResetBits(PWR_CTL_GPIO_PORT, PWR_CTL_GPIO_PIN);  // 输出低电平控制主板上电
            printf("主板上电控制信号已发送\r\n");
        }
        
        // 处理poweroff命令（下电）
        if(poweroff_flag == 1)
        {
            poweroff_flag = 0;
            printf("收到poweroff命令，准备控制主板下电...\r\n");
            GPIO_SetBits(PWR_CTL_GPIO_PORT, PWR_CTL_GPIO_PIN);  // 输出高电平控制主板下电
            // delay_1ms(10000);// 延时10ms
            // GPIO_ResetBits(PWR_CTL_GPIO_PORT, PWR_CTL_GPIO_PIN);  // 输出低电平控制主板上电
            printf("主板下电控制信号已发送\r\n");
        }

        // 处理powerrst命令（主板复位）
       if(powerrst_flag == 1)
       {
           powerrst_flag = 0;
           printf("收到powerrst命令，准备复位主板...\r\n");

           GPIO_SetBits(BMC_CTRL_SYSRST_GPIO_PORT, BMC_CTRL_SYSRST_GPIO_PIN);    // 恢复高电平
            delay_1ms(1000);// 延时10ms
           GPIO_ResetBits(BMC_CTRL_SYSRST_GPIO_PORT, BMC_CTRL_SYSRST_GPIO_PIN);  // 低电平
    
           printf("复位信号已发送\r\n");
       }
    }
}




