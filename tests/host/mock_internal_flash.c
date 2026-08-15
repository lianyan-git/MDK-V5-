#include "bsp_internal_flash.h"
#include "bsp_system_reset.h"
#include "mock_internal_flash.h"
#include "platform_contract.h"
#include "system_time.h"

#include <string.h>

static uint8_t flash_data[PLATFORM_FLASH_SIZE];
static int write_failure;
static int erase_failure;
static int app_write_failure;
static uint16_t page_erase_count[PLATFORM_FLASH_SIZE / PLATFORM_FLASH_PAGE_SIZE];
static uint32_t now_ms;
static uint32_t heater_off_count;
static uint32_t reset_count;
static uint32_t watchdog_kick_count;

static int map_range(uint32_t address, uint32_t length, uint32_t *offset)
{
    if ((address < PLATFORM_FLASH_BASE) || (address > PLATFORM_FLASH_END) ||
        (length > PLATFORM_FLASH_END - address)) return 0;
    *offset = address - PLATFORM_FLASH_BASE;
    return 1;
}

void MockInternalFlash_Reset(void)
{
    memset(flash_data, 0xFF, sizeof(flash_data));
    write_failure = 0;
    erase_failure = 0;
    app_write_failure = 0;
    memset(page_erase_count, 0, sizeof(page_erase_count));
    now_ms = 0U;
    heater_off_count = 0U;
    reset_count = 0U;
    watchdog_kick_count = 0U;
}

void MockInternalFlash_SetWriteFailure(int fail) { write_failure = fail; }
void MockInternalFlash_SetEraseFailure(int fail) { erase_failure = fail; }
void MockInternalFlash_SetAppWriteFailure(int fail) { app_write_failure = fail; }
void MockPlatform_AdvanceMs(uint32_t milliseconds) { now_ms += milliseconds; }
uint32_t MockPlatform_GetHeaterOffCount(void) { return heater_off_count; }
uint32_t MockPlatform_GetResetCount(void) { return reset_count; }
uint32_t MockPlatform_GetWatchdogKickCount(void) { return watchdog_kick_count; }

uint32_t MockInternalFlash_GetPageEraseCount(uint32_t address)
{
    uint32_t offset;
    if (((address & (PLATFORM_FLASH_PAGE_SIZE - 1U)) != 0U) ||
        !map_range(address, PLATFORM_FLASH_PAGE_SIZE, &offset)) return 0U;
    return page_erase_count[offset / PLATFORM_FLASH_PAGE_SIZE];
}

void MockInternalFlash_CorruptByte(uint32_t address, uint8_t xor_mask)
{
    uint32_t offset;
    if (map_range(address, 1U, &offset)) flash_data[offset] ^= xor_mask;
}

InternalFlashStatus_t InternalFlash_Read(uint32_t address,
                                         uint8_t *data, uint32_t length)
{
    uint32_t offset;
    if ((data == 0) && (length != 0U)) return INTERNAL_FLASH_ERROR_ARGUMENT;
    if (!map_range(address, length, &offset)) return INTERNAL_FLASH_ERROR_RANGE;
    memcpy(data, &flash_data[offset], length);
    return INTERNAL_FLASH_OK;
}

InternalFlashStatus_t InternalFlash_ErasePage(uint32_t address)
{
    uint32_t offset;
    if (erase_failure) return INTERNAL_FLASH_ERROR_PROGRAM;
    if (((address & (PLATFORM_FLASH_PAGE_SIZE - 1U)) != 0U) ||
        !map_range(address, PLATFORM_FLASH_PAGE_SIZE, &offset)) {
        return INTERNAL_FLASH_ERROR_RANGE;
    }
    memset(&flash_data[offset], 0xFF, PLATFORM_FLASH_PAGE_SIZE);
    ++page_erase_count[offset / PLATFORM_FLASH_PAGE_SIZE];
    return INTERNAL_FLASH_OK;
}

InternalFlashStatus_t InternalFlash_Write(uint32_t address,
                                          const uint8_t *data, uint32_t length)
{
    uint32_t offset;
    uint32_t index;
    if (write_failure) return INTERNAL_FLASH_ERROR_PROGRAM;
    if (app_write_failure && (address >= PLATFORM_APP_ADDR)) {
        return INTERNAL_FLASH_ERROR_PROGRAM;
    }
    if ((data == 0) && (length != 0U)) return INTERNAL_FLASH_ERROR_ARGUMENT;
    if (!map_range(address, length, &offset)) return INTERNAL_FLASH_ERROR_RANGE;
    for (index = 0U; index < length; ++index) flash_data[offset + index] &= data[index];
    return INTERNAL_FLASH_OK;
}

int InternalFlash_IsErased(uint32_t address, uint32_t length)
{
    uint32_t offset;
    uint32_t index;
    if (!map_range(address, length, &offset)) return 0;
    for (index = 0U; index < length; ++index) {
        if (flash_data[offset + index] != UINT8_C(0xFF)) return 0;
    }
    return 1;
}

uint32_t SystemTime_Millis(void) { return now_ms; }
void Board_ForceHeaterOff(void) { ++heater_off_count; }
void Watchdog_Kick(void) { ++watchdog_kick_count; }
void SystemControl_RequestReset(void) { ++reset_count; }
