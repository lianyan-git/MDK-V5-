#ifndef BOOT_PLATFORM_H
#define BOOT_PLATFORM_H

#include <stdint.h>

uint32_t BootPlatform_ReadWord(uint32_t address);
void BootPlatform_DisableInterrupts(void);
void BootPlatform_StopSysTick(void);
void BootPlatform_ClearInterrupts(void);
void BootPlatform_SetVectorTable(uint32_t address);
void BootPlatform_Jump(uint32_t initial_sp, uint32_t reset_vector);

#endif /* BOOT_PLATFORM_H */
