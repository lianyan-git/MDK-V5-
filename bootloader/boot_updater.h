#ifndef BOOT_UPDATER_H
#define BOOT_UPDATER_H

#include <stdint.h>

#define BOOT_UPDATE_MAX_ATTEMPTS 3U

typedef enum {
    BOOT_UPDATE_UPDATED = 0,
    BOOT_UPDATE_NO_REQUEST = 1,
    BOOT_UPDATE_ERROR_METADATA = -1,
    BOOT_UPDATE_ERROR_STAGING = -2,
    BOOT_UPDATE_ERROR_FLASH = -3,
    BOOT_UPDATE_ERROR_VERIFY = -4,
    BOOT_UPDATE_ERROR_RETRY_LIMIT = -5
} BootUpdateStatus_t;

typedef void (*BootUpdateProgressFn)(void *context, uint8_t percent);

BootUpdateStatus_t BootUpdater_Run(BootUpdateProgressFn progress,
                                    void *progress_context);

#endif /* BOOT_UPDATER_H */
