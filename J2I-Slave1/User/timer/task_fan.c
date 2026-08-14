#include "bsp_init.h"

TaskHandle_t Fan_Ctrl_Task_Handle = NULL;

typedef struct
{
	volatile uint8_t manual_override; /* 0=自动温控 1=串口手动定占空比,直到"Fan auto"清除 */
	volatile uint8_t manual_duty;     /* 手动占空比(0-100),仅manual_override=1时有效 */
	uint8_t          current_duty;    /* 最近一次实际写入PWM寄存器的占空比 */
}FanChannelState_t;

static FanChannelState_t s_fan_state[FAN_CH_COUNT];

/* 主机(飞腾D2000)通过UART5每秒上报一次真实CPU温度("CPU_TEMP=45.2C\n",见
 * task_usart.c的UART5_Task), 解析后写入这里, 供Fan_Ctrl_Task驱动风扇自动温控。
 * -1沿用board_temp0同款哨兵惯例,表示"从未收到过有效上报" */
static volatile int        s_cpu_reported_temp_c = -1;
static volatile uint8_t    s_cpu_temp_ever_valid  = 0;
static volatile TickType_t s_cpu_temp_last_tick   = 0;

#define FAN_CTRL_LOOP_DELAY_MS       200
#define FAN_AUTO_TEMP_MIN_C           35
#define FAN_AUTO_TEMP_MAX_C           75
#define FAN_AUTO_GEAR_COUNT           20
#define FAN_CPU_TEMP_TIMEOUT_MS     10000   /* 主机每1s上报一次,10s无新帧判定为上报中断 */
#define FAN_AUTO_DUTY_MIN_PCT         30
#define FAN_AUTO_DUTY_MAX_PCT        100
#define FAN_AUTO_DUTY_FAILSAFE_PCT   100   /* 从未收到过CPU_TEMP或上报超时时的故障安全占空比 */
#define FAN_AUTO_HYSTERESIS_PCT        3   /* 自动模式死区,抑制换挡抖动 */

/*
*********************************************************************************************************
*	函 数 名: Fan_UpdateCpuReportedTemp
*	功能说明: 主机UART5上报CPU_TEMP解析成功后调用,刷新最近一次温度和时间戳(供UART5_Task调用)
*	形    参：temp_c: 主机上报的CPU温度(整数摄氏度)
*	返 回 值: 无
*********************************************************************************************************
*/
void Fan_UpdateCpuReportedTemp(int temp_c)
{
	s_cpu_reported_temp_c = temp_c;
	s_cpu_temp_ever_valid = 1;
	s_cpu_temp_last_tick  = xTaskGetTickCount();
}

/*
*********************************************************************************************************
*	函 数 名: Fan_GetCpuTempOrFailsafe
*	功能说明: 取最近一次主机上报的CPU温度; 从未收到过或超过FAN_CPU_TEMP_TIMEOUT_MS未刷新则返回-1(触发故障安全)
*	形    参：无
*	返 回 值: 最近一次有效温度, 或-1
*********************************************************************************************************
*/
static int Fan_GetCpuTempOrFailsafe(void)
{
	if(!s_cpu_temp_ever_valid) return -1;
	if((xTaskGetTickCount() - s_cpu_temp_last_tick) > pdMS_TO_TICKS(FAN_CPU_TEMP_TIMEOUT_MS)) return -1;
	return s_cpu_reported_temp_c;
}

/*
*********************************************************************************************************
*	函 数 名: Fan_GetCpuTempDebug
*	功能说明: 仅供调试打印使用(Sensor_Task), 不参与风扇温控判断逻辑
*	形    参：out_temp_c: 输出最近一次收到的CPU温度(即使已过期也原样输出)
*	返 回 值: 1=新鲜(未过期)  0=过期或从未收到
*********************************************************************************************************
*/
uint8_t Fan_GetCpuTempDebug(int *out_temp_c)
{
	TickType_t now = xTaskGetTickCount();
	uint8_t fresh = s_cpu_temp_ever_valid &&
	                ((now - s_cpu_temp_last_tick) <= pdMS_TO_TICKS(FAN_CPU_TEMP_TIMEOUT_MS));
	*out_temp_c = s_cpu_reported_temp_c;
	return fresh;
}

/*
*********************************************************************************************************
*	函 数 名: Fan_ComputeAutoDuty
*	功能说明: 根据主机上报CPU温度计算自动模式下的占空比。35~75C分20档(2C/档,3.5%/档),
*	          <=35C:30%下限, >=75C:100%上限, -1(从未收到/上报超时):故障安全100%
*	形    参：temp_c: Fan_GetCpuTempOrFailsafe()的返回值
*	返 回 值: 0~100
*********************************************************************************************************
*/
static uint8_t Fan_ComputeAutoDuty(int temp_c)
{
	uint16_t duty_permille;

	if(temp_c == -1)
		return FAN_AUTO_DUTY_FAILSAFE_PCT;

	if(temp_c <= FAN_AUTO_TEMP_MIN_C)
		duty_permille = FAN_AUTO_DUTY_MIN_PCT * 10;
	else if(temp_c >= FAN_AUTO_TEMP_MAX_C)
		duty_permille = FAN_AUTO_DUTY_MAX_PCT * 10;
	else
	{
		/* 此处temp_c必落在(FAN_AUTO_TEMP_MIN_C, FAN_AUTO_TEMP_MAX_C)开区间内,
		 * (temp_c-MIN_C)恒为正,整数除法截断方向不影响结果 */
		uint8_t gear = (uint8_t)((temp_c - FAN_AUTO_TEMP_MIN_C) / 2);
		duty_permille = (uint16_t)(FAN_AUTO_DUTY_MIN_PCT * 10 + gear * 35);
	}
	return (uint8_t)(duty_permille / 10);
}

/*
*********************************************************************************************************
*	函 数 名: Fan_SetManualDuty
*	功能说明: 串口手动命令设置指定通道占空比,并切换为手动模式(供Power_Ctrl_Task的UART4命令解析调用)
*	形    参：channel: FAN_CH1/FAN_CH2   duty_percent: 0~100
*	返 回 值: 无
*********************************************************************************************************
*/
void Fan_SetManualDuty(uint8_t channel, uint8_t duty_percent)
{
	if(channel >= FAN_CH_COUNT) return;
	if(duty_percent > 100) duty_percent = 100;
	s_fan_state[channel].manual_duty     = duty_percent;
	s_fan_state[channel].manual_override = 1;
}

/*
*********************************************************************************************************
*	函 数 名: Fan_ClearManualOverride
*	功能说明: 清除两路风扇的手动模式,恢复自动温控(供"Fan auto"命令调用)
*	形    参：无
*	返 回 值: 无
*********************************************************************************************************
*/
void Fan_ClearManualOverride(void)
{
	uint8_t ch;
	for(ch = 0; ch < FAN_CH_COUNT; ch++)
		s_fan_state[ch].manual_override = 0;
}

/*
*********************************************************************************************************
*	函 数 名: Fan_GetCurrentDuty
*	功能说明: 读取指定风扇通道当前实际下发的占空比(自动/手动模式均适用),供调试打印、IPMB传感器等只读场景使用
*	形    参：channel: FAN_CH1/FAN_CH2
*	返 回 值: 0~100，channel非法时返回0
*********************************************************************************************************
*/
uint8_t Fan_GetCurrentDuty(uint8_t channel)
{
	if(channel >= FAN_CH_COUNT) return 0;
	return s_fan_state[channel].current_duty;
}

/*
*********************************************************************************************************
*	函 数 名: Fan_Ctrl_Task
*	功能说明: 风扇PWM控制任务主体。优先级从高到低:主板确认下电强制0% > 串口手动占空比(一直保持直到收到"Fan auto"或主板重新上电) > CPU温度自动曲线。
*	          主板每次由下电切换到上电,都会自动清除手动占空比设置,回到温控自动模式
*	形    参：void* parameter
*	返 回 值: 无
*********************************************************************************************************
*/
void Fan_Ctrl_Task(void* parameter)
{
	uint8_t ch;
	static uint8_t s_prev_power_state = 0;

	while(1)
	{
		if(mainboard_power_state && !s_prev_power_state)
		{
			/* 主板从下电切换到上电(不管来源是UART的Power on命令、IPMB上电命令，还是PB5硬件反馈),
			 * 强制两路风扇回到CPU温度自动曲线，清除之前可能残留的串口手动占空比设置 */
			Fan_ClearManualOverride();
		}
		s_prev_power_state = mainboard_power_state;

		uint8_t auto_duty = Fan_ComputeAutoDuty(Fan_GetCpuTempOrFailsafe());
		/* 主板电源状态已确认(不是MCU刚上电时的占位默认值) 且 确认为下电 -> 两路风扇强制0%,
		 * 优先级高于串口手动设置；未确认之前维持Fan_PWM_Init()设的100%故障安全默认值 */
		uint8_t power_off_confirmed = (uint8_t)(power_state_confirmed && !mainboard_power_state);

		for(ch = 0; ch < FAN_CH_COUNT; ch++)
		{
			uint8_t target_duty;
			uint8_t should_apply;

			if(power_off_confirmed)
			{
				/* 不清除manual_override/manual_duty，只是暂时不生效；
				 * 主板重新上电的瞬间会被上面的边沿检测统一清除，回到温控自动模式 */
				target_duty  = 0;
				should_apply = (target_duty != s_fan_state[ch].current_duty);
			}
			else if(s_fan_state[ch].manual_override)
			{
				target_duty  = s_fan_state[ch].manual_duty;
				should_apply = (target_duty != s_fan_state[ch].current_duty);
			}
			else
			{
				int diff = (int)auto_duty - (int)s_fan_state[ch].current_duty;
				if(diff < 0) diff = -diff;
				target_duty  = auto_duty;
				should_apply = (diff >= FAN_AUTO_HYSTERESIS_PCT);
			}

			if(should_apply)
			{
				Fan_PWM_SetDuty(ch, target_duty);
				s_fan_state[ch].current_duty = target_duty;
			}
		}

		vTaskDelay(pdMS_TO_TICKS(FAN_CTRL_LOOP_DELAY_MS));
	}
}
