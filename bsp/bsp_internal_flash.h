#ifndef BSP_INTERNAL_FLASH_H
#define BSP_INTERNAL_FLASH_H

#include <stdint.h>

typedef enum {
    INTERNAL_FLASH_OK = 0,
    INTERNAL_FLASH_ERROR_ARGUMENT = -1,
    INTERNAL_FLASH_ERROR_RANGE = -2,
    INTERNAL_FLASH_ERROR_PROGRAM = -3,
    INTERNAL_FLASH_ERROR_VERIFY = -4
} InternalFlashStatus_t;

InternalFlashStatus_t InternalFlash_Read(uint32_t address,
                                         uint8_t *data, uint32_t length);
InternalFlashStatus_t InternalFlash_ErasePage(uint32_t address);
InternalFlashStatus_t InternalFlash_Write(uint32_t address,
                                          const uint8_t *data, uint32_t length);
int InternalFlash_IsErased(uint32_t address, uint32_t length);

#endif /* BSP_INTERNAL_FLASH_H */
