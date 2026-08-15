#include "stm32f10x.h"

uint32_t SystemCoreClock = 72000000U;
uint32_t mock_systick_reload = 0U;

uint32_t SysTick_Config(uint32_t ticks)
{
    mock_systick_reload = ticks;
    return 0U;
}
