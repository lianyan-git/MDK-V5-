#ifndef MOCK_BOOT_PLATFORM_H
#define MOCK_BOOT_PLATFORM_H

#include <stdint.h>

void MockBootPlatform_Reset(void);
void MockBootPlatform_SetVectors(uint32_t initial_sp, uint32_t reset_vector);
uint32_t MockBootPlatform_GetEventCount(void);
uint32_t MockBootPlatform_GetEvent(uint32_t index);
uint32_t MockBootPlatform_GetVectorTable(void);
uint32_t MockBootPlatform_GetMainStack(void);
uint32_t MockBootPlatform_GetJumpAddress(void);

#endif /* MOCK_BOOT_PLATFORM_H */
