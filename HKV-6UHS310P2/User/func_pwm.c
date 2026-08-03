#include "bsp_init.h"
#include ".\timer\bsp_pwm.h"

// 外部声明的温度变量，用于温度控制风扇
extern int board_temp1[2];           //存储整数化后的实际温度值

/*																								
*********************************************************************************************************
*	函 数 名: pwm_func
*	功能说明: 温度控制风扇PWM占空比
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void pwm_func(void)
{
    char flash_buf[256] = {0};
    uint8_t pwm_duty;
    
    // 获取当前温度值（假设board_temp1[0]是CPU温度）
    int temp = board_temp1[0];
    
    // 根据温度计算PWM占空比
    if(temp < 0) {
        // 温度低于0度，不转
        pwm_duty = 0;
    } else if(temp < 40) {
        // 温度在0-40度之间，转速25%
        pwm_duty = 25;
    } else if(temp < 50) {
        // 温度在40-50度之间，转速50%
        pwm_duty = 50;
    } else if(temp < 60) {
        // 温度在50-60度之间，转速75%
        pwm_duty = 75;
    } else {
        // 温度高于60度，转速100%
        pwm_duty = 100;
    }
    
    // 设置PWM占空比
    pwm_set_duty(pwm_duty);
    
    // 打印温度和PWM占空比到调试缓冲区
    sprintf((char *)flash_buf, "温度: %d°C, 风扇PWM占空比: %d%%\r\n", temp, pwm_duty);
    strcat((char *)stPrintf_Buf.buf, flash_buf);
    memset(flash_buf, 0, 128);
}
