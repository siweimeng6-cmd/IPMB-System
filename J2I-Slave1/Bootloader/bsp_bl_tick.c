#include "bsp_bl_tick.h"

/* Bootloader 是裸机程序, 没有 FreeRTOS 的 tick, 自己用 SysTick 起一个
 * 1ms 计数器, 给串口超时/延时用 */
static volatile uint32_t s_tick_ms = 0;

void SysTick_Handler(void)
{
	s_tick_ms++;
}

void BL_Tick_Init(void)
{
	SysTick_Config(SystemCoreClock / 1000);
}

uint32_t BL_GetTick(void)
{
	return s_tick_ms;
}

void BL_Delay(uint32_t ms)
{
	uint32_t start = s_tick_ms;
	while ((s_tick_ms - start) < ms) {
	}
}
