#include "bsp_init.h"

float fan1_duty,fan2_duty;
float fan1_freq,fan2_freq;
uint16_t fan_output_duty = UP_CNT;
extern int board_temp1[2];           //�洢���������ʵ���¶�ֵ
/*																						
*********************************************************************************************************

*********************************************************************************************************
*/
void pwm_func(void)
{	
		float output1_duty;
		float output1_freq;
		uint32_t up_time,all_time,down_time;
		char flash_buf[256] = {0};
	
//		output1_freq = (APB2_CLK/APB2_Output_TIM_Prescaler)/OUTPUT_CNT;
//		output1_duty = fan_output_duty*100/OUTPUT_CNT;
//		sprintf((char *)flash_buf, "TIM1 PWM OUT:FREQ:%.2f HZ , DUTY:%.1f \r\n",  output1_freq, output1_duty);
//		strncat((char *)stPrintf_Buf.buf, flash_buf, sizeof(stPrintf_Buf.buf) - strlen((char *)stPrintf_Buf.buf) - 1);		
//		memset(flash_buf, 0, 128);
	
		if(TIM_ICUserValueStructure_TACH1.Capture_FinishFlag == 1)
		{
		
			up_time = TIM_ICUserValueStructure_TACH1.Capture_Period_UP * INPUT_CNT + (TIM_ICUserValueStructure_TACH1.Capture_CcrValue_UP+1);
			all_time = (TIM_ICUserValueStructure_TACH1.Capture_Period_DOWN) * INPUT_CNT + (TIM_ICUserValueStructure_TACH1.Capture_CcrValue_DOWN+1);
			printf("TACH1:HIGH:%d , ALL:%d\r\n", up_time ,all_time);
			
			fan1_duty = up_time*100/(all_time);
			fan1_freq = (float)(APB1_CLK/APB1_TIM_Prescaler)/(all_time);
			sprintf((char *)flash_buf,"TACH1:FREQ��%.3f HZ , DUTY��%.1f \r\n", fan1_freq,fan1_duty);
			strncat((char *)stPrintf_Buf.buf, flash_buf, sizeof(stPrintf_Buf.buf) - strlen((char *)stPrintf_Buf.buf) - 1);	
			memset(flash_buf, 0, 128);
			
			TIM_ICUserValueStructure_TACH1.Capture_FinishFlag = 0;
			TIM_ICUserValueStructure_TACH1.Capture_Period_DOWN = 0;	// �Զ���װ�ؼĴ������±�־��0		
			TIM_ICUserValueStructure_TACH1.Capture_Period_UP = 0;	// �Զ���װ�ؼĴ������±�־��0
			TIM_ICUserValueStructure_TACH1.Timer_Cnt=0;			
			TIM_ICUserValueStructure_TACH1.Get_State = 0;	
			TIM_ClearITPendingBit ( TIM1, TIM_IT_Update ); 
											
			
		}
		if(TIM_ICUserValueStructure_TACH2.Capture_FinishFlag == 1)
		{
			//TIM_ICUserValueStructure_TACH2.Capture_FinishFlag = 0;
			up_time = TIM_ICUserValueStructure_TACH2.Capture_Period_UP * INPUT_CNT + (TIM_ICUserValueStructure_TACH2.Capture_CcrValue_UP+1);
			all_time = (TIM_ICUserValueStructure_TACH2.Capture_Period_DOWN) * INPUT_CNT + (TIM_ICUserValueStructure_TACH2.Capture_CcrValue_DOWN+1);
			printf("TACH2:HIGH:%d , ALL:%d\r\n", up_time ,all_time);
			
			fan1_duty = up_time*100/(all_time);
			fan1_freq = (float)(APB2_CLK/APB2_TIM_Prescaler)/(all_time);
			sprintf((char *)flash_buf,"TACH2:FREQ��%.3f HZ , DUTY��%.1f \r\n", fan1_freq,fan1_duty);
			strncat((char *)stPrintf_Buf.buf, flash_buf, sizeof(stPrintf_Buf.buf) - strlen((char *)stPrintf_Buf.buf) - 1);	
			memset(flash_buf, 0, 128);
			
			TIM_ICUserValueStructure_TACH2.Capture_FinishFlag = 0;
			TIM_ICUserValueStructure_TACH2.Capture_Period_DOWN = 0;	// �Զ���װ�ؼĴ������±�־��0		
			TIM_ICUserValueStructure_TACH2.Capture_Period_UP = 0;	// �Զ���װ�ؼĴ������±�־��0
			TIM_ICUserValueStructure_TACH2.Get_State = 0;	
													
		}
		if(TIM_ICUserValueStructure_TACH3.Capture_FinishFlag == 1)
		{
			up_time = TIM_ICUserValueStructure_TACH3.Capture_Period_UP * INPUT_CNT + (TIM_ICUserValueStructure_TACH3.Capture_CcrValue_UP+1);
			all_time = (TIM_ICUserValueStructure_TACH3.Capture_Period_DOWN) * INPUT_CNT + (TIM_ICUserValueStructure_TACH3.Capture_CcrValue_DOWN+1);
			printf("TACH3:HIGH:%d , ALL:%d\r\n", up_time ,all_time);
			
			fan1_duty = up_time*100/(all_time);
			fan1_freq = (float)(APB1_CLK/APB1_TIM_Prescaler)/(all_time);
			sprintf((char *)flash_buf,"TACH3:FREQ��%.3f HZ , DUTY��%.1f \r\n", fan1_freq,fan1_duty);
			strncat((char *)stPrintf_Buf.buf, flash_buf, sizeof(stPrintf_Buf.buf) - strlen((char *)stPrintf_Buf.buf) - 1);	
			memset(flash_buf, 0, 128);
			
			TIM_ICUserValueStructure_TACH3.Capture_FinishFlag = 0;	
			TIM_ICUserValueStructure_TACH3.Capture_Period_DOWN = 0;	// �Զ���װ�ؼĴ������±�־��0		
			TIM_ICUserValueStructure_TACH3.Capture_Period_UP = 0;	// �Զ���װ�ؼĴ������±�־��0
			TIM_ICUserValueStructure_TACH3.Get_State = 0;	
													
		}
		if(TIM_ICUserValueStructure_TACH4.Capture_FinishFlag == 1)
		{
			up_time = TIM_ICUserValueStructure_TACH4.Capture_Period_UP * INPUT_CNT + (TIM_ICUserValueStructure_TACH4.Capture_CcrValue_UP+1);
			all_time = (TIM_ICUserValueStructure_TACH4.Capture_Period_DOWN) * INPUT_CNT + (TIM_ICUserValueStructure_TACH4.Capture_CcrValue_DOWN+1);
			printf("TACH4:HIGH:%d , ALL:%d\r\n", up_time ,all_time);
			
			fan1_duty = up_time*100/(all_time);
			fan1_freq = (float)(APB1_CLK/APB1_TIM_Prescaler)/(all_time);
			sprintf((char *)flash_buf,"TACH4:FREQ��%.3f HZ , DUTY��%.1f \r\n", fan1_freq,fan1_duty);
			strncat((char *)stPrintf_Buf.buf, flash_buf, sizeof(stPrintf_Buf.buf) - strlen((char *)stPrintf_Buf.buf) - 1);	
			memset(flash_buf, 0, 128);
			
			TIM_ICUserValueStructure_TACH4.Capture_FinishFlag = 0;
			TIM_ICUserValueStructure_TACH4.Capture_Period_DOWN = 0;	// �Զ���װ�ؼĴ������±�־��0		
			TIM_ICUserValueStructure_TACH4.Capture_Period_UP = 0;	// �Զ���װ�ؼĴ������±�־��0
			TIM_ICUserValueStructure_TACH4.Get_State = 0;												
		}
}


/*																						
*********************************************************************************************************
*	�� �� ��: temp_ctl_fan
*	����˵��: ͨ���¶ȿ��Ʒ���ת��ռ�ձ�
*	��    �Σ�
*	�� �� ֵ: ��
*********************************************************************************************************
*/
void temp_ctl_fan(void)
{  
	uint16_t output_duty;
	output_duty = OUTPUT_CNT/100;
	if(board_temp1[0]<30)  // �޸�Ϊ <30��
  {
		fan_output_duty = 0;  // ֹͣ��0%ռ�ձ�
		printf("[fun control]:temperature<30, fun stop, PWM 0%%\r\n");
	}
	else if((board_temp1[0]>=30)&&(board_temp1[0]<40))  // �޸�Ϊ 30-40��
	{
		fan_output_duty = 20*output_duty;  // ��һ����20% PWM��1200ת/����
		printf("[fun control]:temperature30-40, first gear(1200r/min), PWM 20%%\r\n");
	}
	else if((board_temp1[0]>=40)&&(board_temp1[0]<50))  // 40-50��
	{
		fan_output_duty = 30*output_duty;  // �ڶ�����30% PWM��1800ת/����
		printf("[fun control]:temperature40-50, second gear(1800r/min), PWM 30%%\r\n");
	}
	else if((board_temp1[0]>=50)&&(board_temp1[0]<60))  // 50-60��
	{
		fan_output_duty = 60*output_duty;  // ��������60% PWM
		printf("[fun control]:temperature50-60, third gear, PWM 60%%\r\n");
	}
	else  // ��60��
	{
		fan_output_duty = 100*output_duty;  // ���ĵ���100% PWM
		printf("[fun control]:temperature��60, fourth gear, PWM 100%%\r\n");
	}
	
	// ȡ������4�е�ע�ͣ�ʹPWM������Ч
	TIM_SetCompare2(TIM1, fan_output_duty);
	TIM_SetCompare4(TIM1, fan_output_duty);
	TIM_SetCompare3(TIM3, fan_output_duty);
	TIM_SetCompare4(TIM3, fan_output_duty);
}


