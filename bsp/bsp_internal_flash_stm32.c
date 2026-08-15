#include "bsp_internal_flash.h"

#include "platform_contract.h"
#include "stm32f10x_flash.h"

#include <stddef.h>
#include <string.h>

static int range_valid(uint32_t address, uint32_t length)
{
    return (address >= PLATFORM_FLASH_BASE) &&
           (address <= PLATFORM_FLASH_END) &&
           (length <= PLATFORM_FLASH_END - address);
}

InternalFlashStatus_t InternalFlash_Read(uint32_t address,
                                         uint8_t *data, uint32_t length)
{
    if ((data == NULL) && (length != 0U)) return INTERNAL_FLASH_ERROR_ARGUMENT;
    if (!range_valid(address, length)) return INTERNAL_FLASH_ERROR_RANGE;
    if (length != 0U) memcpy(data, (const void *)(uintptr_t)address, length);
    return INTERNAL_FLASH_OK;
}

InternalFlashStatus_t InternalFlash_ErasePage(uint32_t address)
{
    FLASH_Status result;
    if (((address & (PLATFORM_FLASH_PAGE_SIZE - 1U)) != 0U) ||
        !range_valid(address, PLATFORM_FLASH_PAGE_SIZE)) {
        return INTERNAL_FLASH_ERROR_RANGE;
    }
    FLASH_Unlock();
    result = FLASH_ErasePage(address);
    FLASH_Lock();
    return (result == FLASH_COMPLETE) ? INTERNAL_FLASH_OK : INTERNAL_FLASH_ERROR_PROGRAM;
}

InternalFlashStatus_t InternalFlash_Write(uint32_t address,
                                          const uint8_t *data, uint32_t length)
{
    uint32_t offset;
    FLASH_Status result = FLASH_COMPLETE;

    if ((data == NULL) && (length != 0U)) return INTERNAL_FLASH_ERROR_ARGUMENT;
    if (((address & 1U) != 0U) || ((length & 1U) != 0U) ||
        !range_valid(address, length)) return INTERNAL_FLASH_ERROR_RANGE;
    FLASH_Unlock();
    for (offset = 0U; (offset < length) && (result == FLASH_COMPLETE); offset += 2U) {
        uint16_t value = (uint16_t)data[offset] |
                         (uint16_t)((uint16_t)data[offset + 1U] << 8);
        result = FLASH_ProgramHalfWord(address + offset, value);
    }
    FLASH_Lock();
    if (result != FLASH_COMPLETE) return INTERNAL_FLASH_ERROR_PROGRAM;
    if ((length != 0U) &&
        (memcmp((const void *)(uintptr_t)address, data, length) != 0)) {
        return INTERNAL_FLASH_ERROR_VERIFY;
    }
    return INTERNAL_FLASH_OK;
}

int InternalFlash_IsErased(uint32_t address, uint32_t length)
{
    const uint8_t *bytes;
    uint32_t index;
    if (!range_valid(address, length)) return 0;
    bytes = (const uint8_t *)(uintptr_t)address;
    for (index = 0U; index < length; ++index) {
        if (bytes[index] != UINT8_C(0xFF)) return 0;
    }
    return 1;
}
