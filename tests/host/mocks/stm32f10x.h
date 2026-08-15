#ifndef HOST_MOCK_STM32F10X_H
#define HOST_MOCK_STM32F10X_H

#include <stdint.h>

extern uint32_t SystemCoreClock;
uint32_t SysTick_Config(uint32_t ticks);

#endif /* HOST_MOCK_STM32F10X_H */
