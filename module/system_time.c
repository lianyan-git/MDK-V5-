#include "system_time.h"

#include "stm32f10x.h"

static volatile uint32_t system_time_ms = 0U;

void SystemTime_Init(void)
{
    system_time_ms = 0U;
    (void)SysTick_Config(SystemCoreClock / SYSTEM_TIME_TICK_HZ);
}

uint32_t SystemTime_Millis(void)
{
    return system_time_ms;
}

void SystemTime_TickISR(void)
{
    ++system_time_ms;
}
