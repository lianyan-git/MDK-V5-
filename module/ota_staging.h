#ifndef OTA_STAGING_H
#define OTA_STAGING_H

#include "ota_contract.h"

#include <stdint.h>

OtaError_t OtaStaging_Validate(uint32_t image_size,
                               uint32_t expected_crc32);
OtaError_t OtaStaging_ValidateAndCommit(uint32_t image_size,
                                       uint32_t expected_crc32,
                                       uint32_t app_version,
                                       OtaMetadata_t *metadata,
                                       uint32_t *metadata_address);

#endif /* OTA_STAGING_H */
