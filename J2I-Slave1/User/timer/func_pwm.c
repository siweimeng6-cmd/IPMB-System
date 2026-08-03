#include "bsp_init.h"

float fan1_duty,fan2_duty;
float fan1_freq,fan2_freq;

/*																						
*********************************************************************************************************
*	函 数 名: pwm_func
*	功能说明: 获取风扇转速
*	形    参：
*	返 回 值: 无
*********************************************************************************************************
*/
void pwm_func(void)
{	
		float output_duty;
		float output_freq;
		uint32_t up_time,all_time,down_time;
		char flash_buf[256] = {0};
	
		output_freq = (72000000/Output_TIM_Prescaler)/OUTPUT_CNT;
		output_duty = UP_CNT*100/OUTPUT_CNT;
		sprintf((char *)flash_buf, "pwm输出:频率：%.2f HZ , 占空比：%.1f \r\n",  output_freq, output_duty);
		strcat((char *)stPrintf_Buf.buf, flash_buf);		
		memset(flash_buf, 0, 128);
	
		if(TIM_ICUserValueStructure_CH1.Capture_FinishFlag == 1)
		{
			TIM_ICUserValueStructure_CH1.Capture_FinishFlag = 0;
			up_time = TIM_ICUserValueStructure_CH1.Capture_Period_UP * Input_CNT + (TIM_ICUserValueStructure_CH1.Capture_CcrValue_UP+1);
			all_time = (TIM_ICUserValueStructure_CH1.Capture_Period_DOWN) * Input_CNT + (TIM_ICUserValueStructure_CH1.Capture_CcrValue_DOWN+1);
			printf("CH1:高电平：%d , 全电平：%d\r\n", up_time ,all_time);
			
			fan1_duty = up_time*100/(all_time);
			fan1_freq = (float)(72000000/Input_TIM_Prescaler)/(all_time);
			sprintf((char *)flash_buf,"CH1:频率：%.3f HZ , 占空比：%.1f \r\n", fan1_freq,fan1_duty);
			strcat((char *)stPrintf_Buf.buf, flash_buf);	
			memset(flash_buf, 0, 128);
			
			TIM_ICUserValueStructure_CH1.Capture_Period_DOWN = 0;	// 自动重装载寄存器更新标志清0		
			TIM_ICUserValueStructure_CH1.Capture_Period_UP = 0;	// 自动重装载寄存器更新标志清0
			TIM_ICUserValueStructure_CH1.Get_State = 0;	
			
//			TIM_ICUserValueStructure_CH1.Channel_en = 0;		
//			TIM_DeInit(TIM1);													//关闭定时器1通道1
//			TIM_ITConfig(TIM1,TIM_IT_CC1| TIM_IT_Update, DISABLE);		//失能定时器1通道1中断	
//			TIM_ICUserValueStructure_CH2.Channel_en = 1;		
//			FAN_TACH_TIM_Mode_Config(TIM_Channel_2);										
			
		}
		if(TIM_ICUserValueStructure_CH2.Capture_FinishFlag == 1)
		{
			TIM_ICUserValueStructure_CH2.Capture_FinishFlag = 0;
			up_time = TIM_ICUserValueStructure_CH2.Capture_Period_UP * Input_CNT + (TIM_ICUserValueStructure_CH2.Capture_CcrValue_UP+1);
			all_time = (TIM_ICUserValueStructure_CH2.Capture_Period_DOWN) * Input_CNT + (TIM_ICUserValueStructure_CH2.Capture_CcrValue_DOWN+1);
			printf("CH2:高电平：%d , 全电平：%d\r\n", up_time ,all_time);
			
			fan1_duty = up_time*100/(all_time);
			fan1_freq = (float)(72000000/Input_TIM_Prescaler)/(all_time);
			sprintf((char *)flash_buf,"CH2:频率：%.3f HZ , 占空比：%.1f \r\n", fan1_freq,fan1_duty);
			strcat((char *)stPrintf_Buf.buf, flash_buf);	
			memset(flash_buf, 0, 128);
			
			TIM_ICUserValueStructure_CH2.Capture_Period_DOWN = 0;	// 自动重装载寄存器更新标志清0		
			TIM_ICUserValueStructure_CH2.Capture_Period_UP = 0;	// 自动重装载寄存器更新标志清0
			TIM_ICUserValueStructure_CH2.Get_State = 0;	
			
//			TIM_ICUserValueStructure_CH2.Channel_en = 0;		
//			TIM_DeInit(TIM1);													//关闭定时器1通道1
//			TIM_ITConfig(TIM1,TIM_IT_CC2| TIM_IT_Update, DISABLE);		//失能定时器1通道1中断	
//			TIM_ICUserValueStructure_CH1.Channel_en = 1;		
//			FAN_TACH_TIM_Mode_Config(TIM_Channel_1);											
		}
		if(TIM_ICUserValueStructure_CH3.Capture_FinishFlag == 1)
		{
			TIM_ICUserValueStructure_CH3.Capture_FinishFlag = 0;
			up_time = TIM_ICUserValueStructure_CH3.Capture_Period_UP * Input_CNT + (TIM_ICUserValueStructure_CH3.Capture_CcrValue_UP+1);
			all_time = (TIM_ICUserValueStructure_CH3.Capture_Period_DOWN) * Input_CNT + (TIM_ICUserValueStructure_CH3.Capture_CcrValue_DOWN+1);
			printf("CH2:高电平：%d , 全电平：%d\r\n", up_time ,all_time);
			
			fan1_duty = up_time*100/(all_time);
			fan1_freq = (float)(72000000/Input_TIM_Prescaler)/(all_time);
			sprintf((char *)flash_buf,"CH2:频率：%.3f HZ , 占空比：%.1f \r\n", fan1_freq,fan1_duty);
			strcat((char *)stPrintf_Buf.buf, flash_buf);	
			memset(flash_buf, 0, 128);
			
			TIM_ICUserValueStructure_CH3.Capture_Period_DOWN = 0;	// 自动重装载寄存器更新标志清0		
			TIM_ICUserValueStructure_CH3.Capture_Period_UP = 0;	// 自动重装载寄存器更新标志清0
			TIM_ICUserValueStructure_CH3.Get_State = 0;	
													
		}
		
		if(TIM_ICUserValueStructure_CH4.Capture_FinishFlag == 1)
		{
			TIM_ICUserValueStructure_CH4.Capture_FinishFlag = 0;
			up_time = TIM_ICUserValueStructure_CH4.Capture_Period_UP * Input_CNT + (TIM_ICUserValueStructure_CH4.Capture_CcrValue_UP+1);
			all_time = (TIM_ICUserValueStructure_CH4.Capture_Period_DOWN) * Input_CNT + (TIM_ICUserValueStructure_CH4.Capture_CcrValue_DOWN+1);
			printf("CH2:高电平：%d , 全电平：%d\r\n", up_time ,all_time);
			
			fan1_duty = up_time*100/(all_time);
			fan1_freq = (float)(72000000/Input_TIM_Prescaler)/(all_time);
			sprintf((char *)flash_buf,"CH2:频率：%.3f HZ , 占空比：%.1f \r\n", fan1_freq,fan1_duty);
			strcat((char *)stPrintf_Buf.buf, flash_buf);	
			memset(flash_buf, 0, 128);
			
			TIM_ICUserValueStructure_CH4.Capture_Period_DOWN = 0;	// 自动重装载寄存器更新标志清0		
			TIM_ICUserValueStructure_CH4.Capture_Period_UP = 0;	// 自动重装载寄存器更新标志清0
			TIM_ICUserValueStructure_CH4.Get_State = 0;	
													
		}
}


/*																						
*********************************************************************************************************
*	函 数 名: swtich_wpm_channel
*	功能说明: 切换风扇通道
*	形    参：
*	返 回 值: 无
*********************************************************************************************************
*/
void swtich_wpm_channel(void)
{

		
}