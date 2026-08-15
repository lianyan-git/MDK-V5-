#ifndef BOOT_RECOVERY_H
#define BOOT_RECOVERY_H

#include "boot_updater.h"

typedef enum {
    BOOT_RECOVERY_JUMP_APP = 0,
    BOOT_RECOVERY_WAIT_FOR_RETRY,
    BOOT_RECOVERY_NO_VALID_APP
} BootRecoveryAction_t;

BootRecoveryAction_t BootRecovery_Decide(BootUpdateStatus_t update_status,
                                         int app_vector_valid);
const char *BootRecovery_StatusText(BootUpdateStatus_t update_status);

#endif /* BOOT_RECOVERY_H */
