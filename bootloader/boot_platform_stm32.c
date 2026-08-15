#include "boot_platform.h"

#include "stm32f10x.h"

uint32_t BootPlatform_ReadWord(uint32_t address)
{
    return *(const uint32_t *)(uintptr_t)address;
}

void BootPlatform_DisableInterrupts(void)
{
    __disable_irq();
}

void BootPlatform_StopSysTick(void)
{
    SysTick->CTRL = 0U;
    SysTick->LOAD = 0U;
    SysTick->VAL = 0U;
}

void BootPlatform_ClearInterrupts(void)
{
    uint32_t index;
    for (index = 0U; index < 8U; ++index) {
        NVIC->ICER[index] = UINT32_C(0xFFFFFFFF);
        NVIC->ICPR[index] = UINT32_C(0xFFFFFFFF);
    }
}

void BootPlatform_SetVectorTable(uint32_t address)
{
    SCB->VTOR = address;
    __DSB();
    __ISB();
}

#if defined(__CC_ARM)
__asm void BootPlatform_Jump(uint32_t initial_sp, uint32_t reset_vector)
{
    MSR MSP, r0
    BX r1
}
#else
void BootPlatform_Jump(uint32_t initial_sp, uint32_t reset_vector)
{
    __set_MSP(initial_sp);
    ((void (*)(void))(uintptr_t)reset_vector)();
    for (;;) { }
}
#endif
