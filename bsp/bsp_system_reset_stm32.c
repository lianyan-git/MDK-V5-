#include "bsp_system_reset.h"

#include "stm32f10x.h"

void SystemControl_RequestReset(void)
{
    NVIC_SystemReset();
}
