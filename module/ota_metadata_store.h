#ifndef OTA_METADATA_STORE_H
#define OTA_METADATA_STORE_H

#include "ota_contract.h"

#include <stdint.h>

typedef enum {
    OTA_METADATA_STORE_OK = 0,
    OTA_METADATA_STORE_NOT_FOUND = 1,
    OTA_METADATA_STORE_ERROR_ARGUMENT = -1,
    OTA_METADATA_STORE_ERROR_IO = -2,
    OTA_METADATA_STORE_ERROR_VERIFY = -3
} OtaMetadataStoreStatus_t;

OtaMetadataStoreStatus_t OtaMetadataStore_Load(OtaMetadata_t *metadata,
                                                uint32_t *metadata_address);
OtaMetadataStoreStatus_t OtaMetadataStore_Commit(OtaState_t state,
                                                  uint32_t image_size,
                                                  uint32_t image_crc32,
                                                  uint32_t app_version,
                                                  OtaError_t last_error,
                                                  OtaMetadata_t *committed,
                                                  uint32_t *metadata_address);
OtaMetadataStoreStatus_t OtaMetadataStore_CommitWithAttempt(
                                                  OtaState_t state,
                                                  uint32_t image_size,
                                                  uint32_t image_crc32,
                                                  uint32_t app_version,
                                                  uint32_t attempt_count,
                                                  OtaError_t last_error,
                                                  OtaMetadata_t *committed,
                                                  uint32_t *metadata_address);
OtaMetadataStoreStatus_t OtaMetadataStore_BeginReceive(void);

#endif /* OTA_METADATA_STORE_H */
