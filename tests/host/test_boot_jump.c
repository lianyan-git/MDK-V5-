#include "boot_jump.h"
#include "boot_recovery.h"
#include "boot_updater.h"
#include "mock_boot_platform.h"
#include "platform_contract.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    uint32_t index;
    static const uint32_t expected_events[] = {1U, 2U, 3U, 4U, 5U, 6U};

    MockBootPlatform_Reset();
    CHECK(!BootJump_IsApplicationValid());
    CHECK(BootJump_ToApplication() == BOOT_JUMP_ERROR_VECTOR);
    CHECK(MockBootPlatform_GetEventCount() == 0U);

    MockBootPlatform_SetVectors(PLATFORM_SRAM_END, PLATFORM_APP_ADDR + 0x100U);
    CHECK(!BootJump_IsApplicationValid());
    MockBootPlatform_SetVectors(PLATFORM_SRAM_END, PLATFORM_APP_ADDR + 0x101U);
    CHECK(BootJump_IsApplicationValid());
    CHECK(BootJump_ToApplication() == BOOT_JUMP_RETURNED);
    CHECK(MockBootPlatform_GetEventCount() == 6U);
    for (index = 0U; index < 6U; ++index) {
        CHECK(MockBootPlatform_GetEvent(index) == expected_events[index]);
    }
    CHECK(MockBootPlatform_GetVectorTable() == PLATFORM_APP_ADDR);
    CHECK(MockBootPlatform_GetMainStack() == PLATFORM_SRAM_END);
    CHECK(MockBootPlatform_GetJumpAddress() == PLATFORM_APP_ADDR + 0x101U);

    CHECK(BootRecovery_Decide(BOOT_UPDATE_UPDATED, 1) == BOOT_RECOVERY_JUMP_APP);
    CHECK(BootRecovery_Decide(BOOT_UPDATE_NO_REQUEST, 1) == BOOT_RECOVERY_JUMP_APP);
    CHECK(BootRecovery_Decide(BOOT_UPDATE_ERROR_METADATA, 1) == BOOT_RECOVERY_JUMP_APP);
    CHECK(BootRecovery_Decide(BOOT_UPDATE_ERROR_STAGING, 1) == BOOT_RECOVERY_JUMP_APP);
    CHECK(BootRecovery_Decide(BOOT_UPDATE_ERROR_FLASH, 1) == BOOT_RECOVERY_WAIT_FOR_RETRY);
    CHECK(BootRecovery_Decide(BOOT_UPDATE_ERROR_VERIFY, 1) == BOOT_RECOVERY_WAIT_FOR_RETRY);
    CHECK(BootRecovery_Decide(BOOT_UPDATE_ERROR_RETRY_LIMIT, 1) == BOOT_RECOVERY_WAIT_FOR_RETRY);
    CHECK(BootRecovery_Decide(BOOT_UPDATE_NO_REQUEST, 0) == BOOT_RECOVERY_NO_VALID_APP);
    CHECK(BootRecovery_Decide(BOOT_UPDATE_ERROR_FLASH, 0) == BOOT_RECOVERY_NO_VALID_APP);
    CHECK(strcmp(BootRecovery_StatusText(BOOT_UPDATE_ERROR_STAGING), "IMAGE ERROR") == 0);
    CHECK(strcmp(BootRecovery_StatusText(BOOT_UPDATE_ERROR_RETRY_LIMIT), "RETRY LIMIT") == 0);

    puts("PASS: APP vector gate, jump teardown order and recovery decision table");
    return 0;
}
