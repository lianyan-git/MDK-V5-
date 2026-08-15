#include "ota_metadata_store.h"

#include "bsp_w25q128.h"
#include "platform_contract.h"

#include <string.h>

static int read_slot(uint32_t address, OtaMetadata_t *metadata)
{
    if (W25Q128_Read(address, (uint8_t *)metadata,
                     (uint32_t)sizeof(*metadata)) != W25Q128_OK) return -1;
    return OtaMetadata_IsValid(metadata) ? 1 : 0;
}

OtaMetadataStoreStatus_t OtaMetadataStore_Load(OtaMetadata_t *metadata,
                                                uint32_t *metadata_address)
{
    OtaMetadata_t primary;
    OtaMetadata_t backup;
    int primary_valid;
    int backup_valid;

    if (metadata == 0) return OTA_METADATA_STORE_ERROR_ARGUMENT;
    primary_valid = read_slot(PLATFORM_METADATA_PRIMARY_ADDR, &primary);
    backup_valid = read_slot(PLATFORM_METADATA_BACKUP_ADDR, &backup);
    if ((primary_valid < 0) || (backup_valid < 0)) return OTA_METADATA_STORE_ERROR_IO;
    if (!OtaMetadata_SelectNewest(&primary, &backup, metadata)) {
        return OTA_METADATA_STORE_NOT_FOUND;
    }
    if (metadata_address != 0) {
        if (primary_valid && (metadata->sequence == primary.sequence)) {
            *metadata_address = PLATFORM_METADATA_PRIMARY_ADDR;
        } else {
            *metadata_address = PLATFORM_METADATA_BACKUP_ADDR;
        }
    }
    return OTA_METADATA_STORE_OK;
}

OtaMetadataStoreStatus_t OtaMetadataStore_CommitWithAttempt(
                                                  OtaState_t state,
                                                  uint32_t image_size,
                                                  uint32_t image_crc32,
                                                  uint32_t app_version,
                                                  uint32_t attempt_count,
                                                  OtaError_t last_error,
                                                  OtaMetadata_t *committed,
                                                  uint32_t *metadata_address)
{
    OtaMetadata_t newest;
    OtaMetadata_t candidate;
    OtaMetadata_t verify;
    uint32_t newest_address = 0U;
    uint32_t target_address;
    uint32_t sequence = 1U;
    OtaMetadataStoreStatus_t load_result;

    load_result = OtaMetadataStore_Load(&newest, &newest_address);
    if (load_result == OTA_METADATA_STORE_OK) sequence = newest.sequence + 1U;
    else if (load_result != OTA_METADATA_STORE_NOT_FOUND) return load_result;
    target_address = (newest_address == PLATFORM_METADATA_PRIMARY_ADDR) ?
                     PLATFORM_METADATA_BACKUP_ADDR : PLATFORM_METADATA_PRIMARY_ADDR;

    OtaMetadata_Init(&candidate, sequence, state, image_size,
                     image_crc32, app_version);
    candidate.attempt_count = attempt_count;
    candidate.last_error = (uint32_t)last_error;
    OtaMetadata_Finalize(&candidate);
    if (!OtaMetadata_IsValid(&candidate)) return OTA_METADATA_STORE_ERROR_ARGUMENT;
    if (W25Q128_EraseSector(target_address) != W25Q128_OK) {
        return OTA_METADATA_STORE_ERROR_IO;
    }
    if (W25Q128_Write(target_address, (const uint8_t *)&candidate,
                      (uint32_t)sizeof(candidate)) != W25Q128_OK) {
        return OTA_METADATA_STORE_ERROR_IO;
    }
    if ((read_slot(target_address, &verify) != 1) ||
        (memcmp(&candidate, &verify, sizeof(candidate)) != 0)) {
        return OTA_METADATA_STORE_ERROR_VERIFY;
    }
    if (committed != 0) *committed = candidate;
    if (metadata_address != 0) *metadata_address = target_address;
    return OTA_METADATA_STORE_OK;
}

OtaMetadataStoreStatus_t OtaMetadataStore_Commit(OtaState_t state,
                                                  uint32_t image_size,
                                                  uint32_t image_crc32,
                                                  uint32_t app_version,
                                                  OtaError_t last_error,
                                                  OtaMetadata_t *committed,
                                                  uint32_t *metadata_address)
{
    return OtaMetadataStore_CommitWithAttempt(state, image_size, image_crc32,
                                              app_version, 0U, last_error,
                                              committed, metadata_address);
}

OtaMetadataStoreStatus_t OtaMetadataStore_BeginReceive(void)
{
    return OtaMetadataStore_Commit(OTA_STATE_RECEIVING, 0U, 0U, 0U,
                                   OTA_ERROR_NONE, 0, 0);
}
