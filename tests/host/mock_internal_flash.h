#ifndef MOCK_INTERNAL_FLASH_H
#define MOCK_INTERNAL_FLASH_H

#include <stdint.h>

void MockInternalFlash_Reset(void);
void MockInternalFlash_SetWriteFailure(int fail);
void MockInternalFlash_SetEraseFailure(int fail);
void MockInternalFlash_CorruptByte(uint32_t address, uint8_t xor_mask);
void MockInternalFlash_SetAppWriteFailure(int fail);
uint32_t MockInternalFlash_GetPageEraseCount(uint32_t address);
uint32_t MockPlatform_GetWatchdogKickCount(void);
void MockPlatform_AdvanceMs(uint32_t milliseconds);
uint32_t MockPlatform_GetHeaterOffCount(void);
uint32_t MockPlatform_GetResetCount(void);

#endif /* MOCK_INTERNAL_FLASH_H */
