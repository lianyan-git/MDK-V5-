#include "boot_jump.h"

#include "boot_platform.h"
#include "ota_contract.h"
#include "platform_contract.h"

int BootJump_IsApplicationValid(void)
{
    uint32_t initial_sp = BootPlatform_ReadWord(PLATFORM_APP_ADDR);
    uint32_t reset_vector = BootPlatform_ReadWord(PLATFORM_APP_ADDR + 4U);
    return OtaImage_ValidateVector(PLATFORM_APP_SIZE,
                                   initial_sp, reset_vector) == OTA_ERROR_NONE;
}

BootJumpStatus_t BootJump_ToApplication(void)
{
    uint32_t initial_sp;
    uint32_t reset_vector;

    if (!BootJump_IsApplicationValid()) return BOOT_JUMP_ERROR_VECTOR;
    initial_sp = BootPlatform_ReadWord(PLATFORM_APP_ADDR);
    reset_vector = BootPlatform_ReadWord(PLATFORM_APP_ADDR + 4U);
    BootPlatform_DisableInterrupts();
    BootPlatform_StopSysTick();
    BootPlatform_ClearInterrupts();
    BootPlatform_SetVectorTable(PLATFORM_APP_ADDR);
    BootPlatform_Jump(initial_sp, reset_vector);
    return BOOT_JUMP_RETURNED;
}
