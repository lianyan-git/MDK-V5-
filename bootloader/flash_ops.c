/*
 * flash_ops.c
 * STM32内部Flash操作实现
 */

#include "flash_ops.h"
#include "shared_defs.h"
#include "stm32f10x.h"

int Flash_ErasePage(uint32_t addr) {
    FLASH_Status status;

    FLASH_Unlock();

    status = FLASH_ErasePage(addr);

    FLASH_Lock();

    return (status == FLASH_COMPLETE) ? 0 : -1;
}

int Flash_EraseAppArea(void) {
    FLASH_Status status;

    FLASH_Unlock();

    for (uint32_t addr = APP_ADDR; addr < APP_ADDR + APP_SIZE; addr += FLASH_PAGE_SIZE) {
        status = FLASH_ErasePage(addr);
        if (status != FLASH_COMPLETE) {
            FLASH_Lock();
            return -1;
        }
    }

    FLASH_Lock();
    return 0;
}

int Flash_Write(uint32_t addr, uint8_t *data, uint32_t len) {
    FLASH_Status status;

    FLASH_Unlock();

    for (uint32_t i = 0; i < len; i += 4) {
        uint32_t word = data[i] | (data[i+1] << 8) | (data[i+2] << 16) | (data[i+3] << 24);
        status = FLASH_ProgramWord(addr + i, word);
        if (status != FLASH_COMPLETE) {
            FLASH_Lock();
            return -1;
        }
    }

    FLASH_Lock();
    return 0;
}

int Flash_Read(uint32_t addr, uint8_t *data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        data[i] = *(__IO uint8_t*)(addr + i);
    }
    return 0;
}
