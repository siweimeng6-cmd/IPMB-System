#include "task_pwm.h"
#include "bsp_pwm.h"
extern int board_temp0[2];
extern int board_temp1[2];

TaskHandle_t PWM_Task_Handle = NULL;

uint8_t input1_configure_flag=0;	//��ǰ��ʱ��1ͨ��1�������벶��
uint8_t input2_configure_flag=0;
uint8_t input3_configure_flag=0;
uint8_t input4_configure_flag=0;
static	uint8_t pwm_channel=1;

xTaskHandle timechan1getSemaphore    = NULL; 
xTaskHandle timechan2getSemaphore    = NULL; 



// �¶ȿ��Ʋ�������
#define TEMP_MIN         30.0f    // ����¶���ֵ (��)
#define TEMP_MAX         70.0f    // ����¶���ֵ (��)
#define PWM_MIN          30       // ��СPWMռ�ձ� (0-100)
#define PWM_MAX          100      // ���PWMռ�ձ� (0-100)

// �������ռ�ձ�
uint16_t calculate_fan_duty(float temp)
{
    uint16_t duty;
    
    if (temp <= TEMP_MIN) {
        duty = PWM_MIN;
    } else if (temp >= TEMP_MAX) {
        duty = PWM_MAX;
    } else {
        // ����ӳ���¶ȵ�PWMռ�ձ�
        duty = PWM_MIN + (uint16_t)((temp - TEMP_MIN) * (PWM_MAX - PWM_MIN) / (TEMP_MAX - TEMP_MIN));
    }
    
    return duty;
}

// �������ת�� (RPM)
uint16_t calculate_fan_rpm(TIM_ICUserValueTypeDef *tach_struct)
{
    uint32_t period_us;
    uint16_t rpm = 0;
    uint16_t period_max = tach_struct->timer_period;  // 该通道实际的定时器period
    uint8_t  tick_us    = tach_struct->timer_tick_us;  // 每微秒tick数

    if (tach_struct->Capture_FinishFlag == 1) {
        // 计算周期 (ticks)
        uint32_t period_ticks;
        if (tach_struct->Capture_Period_UP == 0 && tach_struct->Capture_Period_DOWN == 0) {
            period_ticks = tach_struct->Capture_CcrValue_DOWN + tach_struct->Capture_CcrValue_UP;
        } else {
            period_ticks = (tach_struct->Capture_Period_UP * period_max) + tach_struct->Capture_CcrValue_UP +
                           (tach_struct->Capture_Period_DOWN * period_max) + tach_struct->Capture_CcrValue_DOWN;
        }

        // ticks → 微秒转换
        period_us = period_ticks / tick_us;

        if (period_us > 0) {
            rpm = (uint16_t)(60 * 1000000 / (period_us * 2));
        }

        tach_struct->Capture_FinishFlag = 0;
        tach_struct->Get_State = 0;
    }

    return rpm;
}

// ����PWMռ�ձ�
void set_pwm_duty(uint8_t channel, uint16_t duty)
{
    uint16_t pulse;
    
    // ��ռ�ձ�(0-100)ת��Ϊ����ֵ
    pulse = (uint16_t)((uint32_t)duty * (OUTPUT_CNT - 1) / 100);
    
    switch (channel) {
        case OCPWM1_CHANNEL:
            TIM_SetCompare1(TIM9, pulse);
            break;
        case OCPWM2_CHANNEL:
            TIM_SetCompare2(TIM9, pulse);
            break;
        case OCPWM3_CHANNEL:
            {
//                /* FAN_3(PC8/TIM8_CH3) 板上驱动电路反相,设定值需要取反再下发:
//                 * 设30%实际是70%、设100%实际是0%,和其余3路含义保持一致 */
//                uint16_t inv_duty = (duty > 100) ? 0 : (100 - duty);
//                uint16_t inv_pulse = (uint16_t)((uint32_t)inv_duty * (OUTPUT_CNT - 1) / 100);
//                TIM_SetCompare3(TIM8, inv_pulse);
                            TIM_SetCompare3(TIM8, pulse);
            }
            break;
        case OCPWM4_CHANNEL:
            TIM_SetCompare2(TIM8, pulse);
            break;
    }
}





/*																						
*********************************************************************************************************
*	�� �� ��: PWM_Task
*	����˵��: PWM_Task��������
						�ڸð����ϣ�����Ӳ��Ӱ���ԭ�򣬴Ӱ��������5000HZ�����װ�������ʵ����10Khz
						��˲²�Ӳ����pwm���������һ��Ӱ�죬���ȶ�Ƶ����stm32��Ƶ�ʵ�2��
						�ó�����pwm���ֱ��������ʱ���ܹ�׼ȷ�����������Ƶ�ʣ���Ҫ�������ʱƵ�ʡ�2������ʱ�ٳ�2�����ñ���ԭ��
*	��    �Σ�void* parameter
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void PWM_Task(void* parameter)
{
    float temp1 = 0.0f, temp2 = 0.0f;
    uint16_t duty1 = PWM_MIN, duty2 = PWM_MIN;
    uint16_t rpm1 = 0, rpm2 = 0;
    char output_buf[128] = {0};
    
    while (1) {
        // ��ȡ�¶ȴ���������
        Board_ADDR90_temp(); // ��ȡ��ַ0x90���¶ȴ�����
        Board_ADDR92_temp(); // ��ȡ��ַ0x92���¶ȴ�����
        
        // ����ʵ���¶�ֵ (��)
        temp1 = board_temp0[0] + (board_temp0[1] / 100.0f);
        temp2 = board_temp1[0] + (board_temp1[1] / 100.0f);
        
        // �������ռ�ձ�
        duty1 = calculate_fan_duty(temp1);
        duty2 = calculate_fan_duty(temp2);
        
        // ����PWMռ�ձ�
        set_pwm_duty(OCPWM1_CHANNEL, duty1);
        set_pwm_duty(OCPWM2_CHANNEL, duty2);
        
        // �������ת��
        rpm1 = calculate_fan_rpm(&TIM_ICUserValueStructure_TACH1);
        rpm2 = calculate_fan_rpm(&TIM_ICUserValueStructure_TACH2);
        
        // ������
        memset(output_buf, 0, sizeof(output_buf));
        sprintf(output_buf, "\r\n" 
                "FAN_1: %3d%% fan1PWM\r\n" 
                "FAN_2: %3d%% fan2PWM\r\n" 
                "TEMP_1: %.1f temp1\r\n" 
                "TEMP_2: %.1f temp2\r\n" 
                "RPM_1: %5d fan1RPM\r\n" 
                "RPM_2: %5d fan2RPM\r\n",
                duty1, duty2, temp1, temp2, rpm1, rpm2);
        
        // 不再往共享打印缓冲 stPrintf_Buf 追加:
        // 该 1024 字节缓冲的周期清零(memset)只由 Temp_Task 负责,Temp_Task 一旦
        // 被注释掉就没人清,PWM_Task 每秒 strcat 追加会在约 8 秒后越界,踩坏其后的
        // 全局内存(含 IPMB 队列/收发结构),导致整个 IPMB 通信失效。故去掉这行打印。
        // (风扇控制 set_pwm_duty 不受影响)
        // strcat((char *)stPrintf_Buf.buf, output_buf);

        vTaskDelay(1000); // 1�����һ��
    }
}


