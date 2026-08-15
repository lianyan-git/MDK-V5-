#include "ota_staging.h"

#include "bsp_w25q128.h"
#include "ota_metadata_store.h"
#include "platform_contract.h"

#include <stddef.h>

#define OTA_STAGING_READ_CHUNK 256U

static uint32_t read_u32_le(const uint8_t bytes[4])
{
    return ((uint32_t)bytes[0]) |
           ((uint32_t)bytes[1] << 8) |
           ((uint32_t)bytes[2] << 16) |
           ((uint32_t)bytes[3] << 24);
}

static void commit_failure(uint32_t image_size, uint32_t crc,
                           uint32_t app_version, OtaError_t error)
{
    (void)OtaMetadataStore_Commit(OTA_STATE_FAILED, image_size, crc,
                                  app_version, error, 0, 0);
}

OtaError_t OtaStaging_Validate(uint32_t image_size,
                               uint32_t expected_crc32)
{
    uint8_t vectors[8];
    uint8_t buffer[OTA_STAGING_READ_CHUNK];
    uint32_t offset = 0U;
    uint32_t crc = OtaCrc32_Begin();
    OtaError_t error;

    if ((image_size < 8U) || (image_size > PLATFORM_FIRMWARE_MAX_SIZE)) {
        return OTA_ERROR_SIZE;
    }
    if (W25Q128_Read(PLATFORM_FIRMWARE_ADDR, vectors, sizeof(vectors)) != W25Q128_OK) {
        return OTA_ERROR_STORAGE;
    }
    error = OtaImage_ValidateVector(image_size,
                                    read_u32_le(vectors), read_u32_le(&vectors[4]));
    if (error != OTA_ERROR_NONE) {
        return error;
    }
    while (offset < image_size) {
        uint32_t length = image_size - offset;
        if (length > sizeof(buffer)) length = sizeof(buffer);
        if (W25Q128_Read(PLATFORM_FIRMWARE_ADDR + offset,
                         buffer, length) != W25Q128_OK) {
            return OTA_ERROR_STORAGE;
        }
        crc = OtaCrc32_Update(crc, buffer, length);
        offset += length;
    }
    crc = OtaCrc32_End(crc);
    if (crc != expected_crc32) {
        return OTA_ERROR_CRC;
    }
    return OTA_ERROR_NONE;
}

OtaError_t OtaStaging_ValidateAndCommit(uint32_t image_size,
                                       uint32_t expected_crc32,
                                       uint32_t app_version,
                                       OtaMetadata_t *metadata,
                                       uint32_t *metadata_address)
{
    OtaError_t error = OtaStaging_Validate(image_size, expected_crc32);
    if (error != OTA_ERROR_NONE) {
        commit_failure((image_size <= PLATFORM_FIRMWARE_MAX_SIZE) ? image_size : 0U,
                       0U, app_version, error);
        return error;
    }
    if (OtaMetadataStore_Commit(OTA_STATE_READY, image_size, expected_crc32,
                                app_version, OTA_ERROR_NONE,
                                metadata, metadata_address) != OTA_METADATA_STORE_OK) {
        return OTA_ERROR_STORAGE;
    }
    return OTA_ERROR_NONE;
}
