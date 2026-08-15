#include "boot_recovery.h"

BootRecoveryAction_t BootRecovery_Decide(BootUpdateStatus_t update_status,
                                         int app_vector_valid)
{
    if (update_status == BOOT_UPDATE_UPDATED) {
        return app_vector_valid ? BOOT_RECOVERY_JUMP_APP : BOOT_RECOVERY_NO_VALID_APP;
    }
    if ((update_status == BOOT_UPDATE_NO_REQUEST) ||
        (update_status == BOOT_UPDATE_ERROR_METADATA) ||
        (update_status == BOOT_UPDATE_ERROR_STAGING)) {
        return app_vector_valid ? BOOT_RECOVERY_JUMP_APP : BOOT_RECOVERY_NO_VALID_APP;
    }
    return app_vector_valid ? BOOT_RECOVERY_WAIT_FOR_RETRY : BOOT_RECOVERY_NO_VALID_APP;
}

const char *BootRecovery_StatusText(BootUpdateStatus_t update_status)
{
    switch (update_status) {
    case BOOT_UPDATE_UPDATED: return "UPDATE OK";
    case BOOT_UPDATE_NO_REQUEST: return "START APP";
    case BOOT_UPDATE_ERROR_METADATA: return "META ERROR";
    case BOOT_UPDATE_ERROR_STAGING: return "IMAGE ERROR";
    case BOOT_UPDATE_ERROR_FLASH: return "FLASH RETRY";
    case BOOT_UPDATE_ERROR_VERIFY: return "VERIFY RETRY";
    case BOOT_UPDATE_ERROR_RETRY_LIMIT: return "RETRY LIMIT";
    default: return "BOOT ERROR";
    }
}
