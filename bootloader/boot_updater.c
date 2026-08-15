#include "boot_updater.h"

#include "board.h"
#include "bsp_internal_flash.h"
#include "bsp_w25q128.h"
#include "ota_boot_request_store.h"
#include "ota_contract.h"
#include "ota_metadata_store.h"
#include "ota_staging.h"
#include "platform_contract.h"

#define BOOT_UPDATE_CHUNK 256U

static void report_progress(BootUpdateProgressFn progress,
                            void *context, uint8_t percent)
{
    if (progress != 0) progress(context, percent);
}

static void record_failure(const OtaMetadata_t *metadata,
                           uint32_t attempt_count, OtaError_t error)
{
    (void)OtaMetadataStore_CommitWithAttempt(
        OTA_STATE_FAILED, metadata->image_size, metadata->image_crc32,
        metadata->app_version, attempt_count, error, 0, 0);
}

static BootUpdateStatus_t erase_app(const OtaMetadata_t *metadata,
                                    uint32_t attempt_count,
                                    BootUpdateProgressFn progress, void *context)
{
    uint32_t address;
    uint32_t page = 0U;
    const uint32_t page_count = PLATFORM_APP_SIZE / PLATFORM_FLASH_PAGE_SIZE;

    for (address = PLATFORM_APP_ADDR; address < PLATFORM_APP_END;
         address += PLATFORM_FLASH_PAGE_SIZE) {
        Board_ForceHeaterOff();
        Watchdog_Kick();
        if (InternalFlash_ErasePage(address) != INTERNAL_FLASH_OK) {
            record_failure(metadata, attempt_count, OTA_ERROR_FLASH);
            return BOOT_UPDATE_ERROR_FLASH;
        }
        ++page;
        report_progress(progress, context, (uint8_t)((page * 20U) / page_count));
    }
    return BOOT_UPDATE_UPDATED;
}

static BootUpdateStatus_t program_app(const OtaMetadata_t *metadata,
                                      uint32_t attempt_count,
                                      BootUpdateProgressFn progress, void *context)
{
    uint8_t buffer[BOOT_UPDATE_CHUNK + 1U];
    uint32_t offset = 0U;

    while (offset < metadata->image_size) {
        uint32_t length = metadata->image_size - offset;
        uint32_t program_length;
        if (length > BOOT_UPDATE_CHUNK) length = BOOT_UPDATE_CHUNK;
        Board_ForceHeaterOff();
        Watchdog_Kick();
        if (W25Q128_Read(PLATFORM_FIRMWARE_ADDR + offset,
                         buffer, length) != W25Q128_OK) {
            record_failure(metadata, attempt_count, OTA_ERROR_STORAGE);
            return BOOT_UPDATE_ERROR_STAGING;
        }
        program_length = length;
        if ((program_length & 1U) != 0U) buffer[program_length++] = UINT8_C(0xFF);
        if (InternalFlash_Write(PLATFORM_APP_ADDR + offset,
                                buffer, program_length) != INTERNAL_FLASH_OK) {
            record_failure(metadata, attempt_count, OTA_ERROR_FLASH);
            return BOOT_UPDATE_ERROR_FLASH;
        }
        offset += length;
        report_progress(progress, context,
            (uint8_t)(20U + (offset * 60U) / metadata->image_size));
    }
    return BOOT_UPDATE_UPDATED;
}

static BootUpdateStatus_t verify_app(const OtaMetadata_t *metadata,
                                     uint32_t attempt_count,
                                     BootUpdateProgressFn progress, void *context)
{
    uint8_t buffer[BOOT_UPDATE_CHUNK];
    uint8_t vectors[8];
    uint32_t offset = 0U;
    uint32_t crc = OtaCrc32_Begin();
    uint32_t initial_sp;
    uint32_t reset_vector;

    if (InternalFlash_Read(PLATFORM_APP_ADDR, vectors, sizeof(vectors)) !=
        INTERNAL_FLASH_OK) {
        record_failure(metadata, attempt_count, OTA_ERROR_FLASH);
        return BOOT_UPDATE_ERROR_VERIFY;
    }
    initial_sp = (uint32_t)vectors[0] | ((uint32_t)vectors[1] << 8) |
                 ((uint32_t)vectors[2] << 16) | ((uint32_t)vectors[3] << 24);
    reset_vector = (uint32_t)vectors[4] | ((uint32_t)vectors[5] << 8) |
                   ((uint32_t)vectors[6] << 16) | ((uint32_t)vectors[7] << 24);
    if (OtaImage_ValidateVector(metadata->image_size, initial_sp, reset_vector) !=
        OTA_ERROR_NONE) {
        record_failure(metadata, attempt_count, OTA_ERROR_RESET_VECTOR);
        return BOOT_UPDATE_ERROR_VERIFY;
    }
    while (offset < metadata->image_size) {
        uint32_t length = metadata->image_size - offset;
        if (length > sizeof(buffer)) length = sizeof(buffer);
        Watchdog_Kick();
        if (InternalFlash_Read(PLATFORM_APP_ADDR + offset,
                               buffer, length) != INTERNAL_FLASH_OK) {
            record_failure(metadata, attempt_count, OTA_ERROR_FLASH);
            return BOOT_UPDATE_ERROR_VERIFY;
        }
        crc = OtaCrc32_Update(crc, buffer, length);
        offset += length;
        report_progress(progress, context,
            (uint8_t)(80U + (offset * 20U) / metadata->image_size));
    }
    if (OtaCrc32_End(crc) != metadata->image_crc32) {
        record_failure(metadata, attempt_count, OTA_ERROR_CRC);
        return BOOT_UPDATE_ERROR_VERIFY;
    }
    return BOOT_UPDATE_UPDATED;
}

BootUpdateStatus_t BootUpdater_Run(BootUpdateProgressFn progress,
                                    void *progress_context)
{
    OtaBootRequest_t request;
    OtaMetadata_t metadata;
    uint32_t metadata_address;
    uint32_t attempt_count;
    OtaError_t staging_result;
    BootUpdateStatus_t result;

    Board_ForceHeaterOff();
    report_progress(progress, progress_context, 0U);
    if (OtaBootRequestStore_Load(&request) != OTA_REQUEST_STORE_OK) {
        return BOOT_UPDATE_NO_REQUEST;
    }
    if (OtaMetadataStore_Load(&metadata, &metadata_address) != OTA_METADATA_STORE_OK) {
        return BOOT_UPDATE_ERROR_METADATA;
    }
    if ((metadata.state == (uint32_t)OTA_STATE_APPLIED) &&
        (metadata.image_crc32 == request.image_crc32)) {
        (void)OtaBootRequestStore_Clear();
        report_progress(progress, progress_context, 100U);
        return BOOT_UPDATE_UPDATED;
    }
    if (((metadata.state != (uint32_t)OTA_STATE_READY) &&
         (metadata.state != (uint32_t)OTA_STATE_APPLYING) &&
         (metadata.state != (uint32_t)OTA_STATE_FAILED)) ||
        (metadata.image_crc32 != request.image_crc32)) {
        return BOOT_UPDATE_ERROR_METADATA;
    }
    if ((metadata.state == (uint32_t)OTA_STATE_READY) &&
        (metadata_address != request.metadata_address)) {
        return BOOT_UPDATE_ERROR_METADATA;
    }
    if (metadata.attempt_count >= BOOT_UPDATE_MAX_ATTEMPTS) {
        return BOOT_UPDATE_ERROR_RETRY_LIMIT;
    }
    staging_result = OtaStaging_Validate(metadata.image_size, metadata.image_crc32);
    if (staging_result != OTA_ERROR_NONE) {
        record_failure(&metadata, metadata.attempt_count, staging_result);
        return BOOT_UPDATE_ERROR_STAGING;
    }
    attempt_count = metadata.attempt_count + 1U;
    if (OtaMetadataStore_CommitWithAttempt(
            OTA_STATE_APPLYING, metadata.image_size, metadata.image_crc32,
            metadata.app_version, attempt_count, OTA_ERROR_NONE,
            &metadata, &metadata_address) != OTA_METADATA_STORE_OK) {
        return BOOT_UPDATE_ERROR_METADATA;
    }

    result = erase_app(&metadata, attempt_count, progress, progress_context);
    if (result != BOOT_UPDATE_UPDATED) return result;
    result = program_app(&metadata, attempt_count, progress, progress_context);
    if (result != BOOT_UPDATE_UPDATED) return result;
    result = verify_app(&metadata, attempt_count, progress, progress_context);
    if (result != BOOT_UPDATE_UPDATED) return result;

    if (OtaMetadataStore_CommitWithAttempt(
            OTA_STATE_APPLIED, metadata.image_size, metadata.image_crc32,
            metadata.app_version, attempt_count, OTA_ERROR_NONE,
            0, 0) != OTA_METADATA_STORE_OK) {
        return BOOT_UPDATE_ERROR_METADATA;
    }
    if (OtaBootRequestStore_Clear() != OTA_REQUEST_STORE_OK) {
        return BOOT_UPDATE_ERROR_FLASH;
    }
    report_progress(progress, progress_context, 100U);
    return BOOT_UPDATE_UPDATED;
}
