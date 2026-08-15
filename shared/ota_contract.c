#include "ota_contract.h"

#include <stddef.h>
#include <string.h>

typedef char ota_metadata_size_check[(sizeof(OtaMetadata_t) == OTA_METADATA_SIZE) ? 1 : -1];
typedef char ota_request_size_check[(sizeof(OtaBootRequest_t) == OTA_BOOT_REQUEST_SIZE) ? 1 : -1];

uint32_t OtaCrc32_Begin(void)
{
    return UINT32_C(0xFFFFFFFF);
}

uint32_t OtaCrc32_Update(uint32_t crc, const uint8_t *data, uint32_t length)
{
    uint32_t index;
    uint8_t bit;

    if ((data == NULL) && (length != 0U)) {
        return crc;
    }
    for (index = 0U; index < length; ++index) {
        crc ^= data[index];
        for (bit = 0U; bit < 8U; ++bit) {
            crc = (crc >> 1) ^ (UINT32_C(0xEDB88320) & (uint32_t)(0U - (crc & 1U)));
        }
    }
    return crc;
}

uint32_t OtaCrc32_End(uint32_t crc)
{
    return crc ^ UINT32_C(0xFFFFFFFF);
}

uint32_t OtaCrc32_Calculate(const uint8_t *data, uint32_t length)
{
    return OtaCrc32_End(OtaCrc32_Update(OtaCrc32_Begin(), data, length));
}

OtaError_t OtaImage_ValidateVector(uint32_t image_size,
                                  uint32_t initial_sp,
                                  uint32_t reset_vector)
{
    uint32_t handler;

    if ((image_size < 8U) || (image_size > PLATFORM_APP_SIZE)) {
        return OTA_ERROR_SIZE;
    }
    if ((initial_sp < PLATFORM_SRAM_BASE) || (initial_sp > PLATFORM_SRAM_END) ||
        ((initial_sp & 7U) != 0U)) {
        return OTA_ERROR_STACK_POINTER;
    }
    if ((reset_vector & 1U) == 0U) {
        return OTA_ERROR_RESET_VECTOR;
    }
    handler = reset_vector & ~UINT32_C(1);
    if ((handler < PLATFORM_APP_ADDR) ||
        (handler >= (PLATFORM_APP_ADDR + image_size)) ||
        (handler >= PLATFORM_APP_END)) {
        return OTA_ERROR_RESET_VECTOR;
    }
    return OTA_ERROR_NONE;
}

static uint32_t metadata_crc(const OtaMetadata_t *metadata)
{
    OtaMetadata_t copy = *metadata;
    copy.header_crc32 = 0U;
    return OtaCrc32_Calculate((const uint8_t *)&copy, (uint32_t)sizeof(copy));
}

void OtaMetadata_Init(OtaMetadata_t *metadata,
                      uint32_t sequence,
                      OtaState_t state,
                      uint32_t image_size,
                      uint32_t image_crc32,
                      uint32_t app_version)
{
    if (metadata == NULL) {
        return;
    }
    memset(metadata, 0, sizeof(*metadata));
    metadata->magic = OTA_METADATA_MAGIC;
    metadata->format_version = OTA_FORMAT_VERSION;
    metadata->header_size = OTA_METADATA_SIZE;
    metadata->sequence = sequence;
    metadata->state = (uint32_t)state;
    metadata->image_size = image_size;
    metadata->image_crc32 = image_crc32;
    metadata->target_address = PLATFORM_APP_ADDR;
    metadata->app_version = app_version;
    OtaMetadata_Finalize(metadata);
}

void OtaMetadata_Finalize(OtaMetadata_t *metadata)
{
    if (metadata != NULL) {
        metadata->header_crc32 = metadata_crc(metadata);
    }
}

int OtaMetadata_IsValid(const OtaMetadata_t *metadata)
{
    if ((metadata == NULL) ||
        (metadata->magic != OTA_METADATA_MAGIC) ||
        (metadata->format_version != OTA_FORMAT_VERSION) ||
        (metadata->header_size != OTA_METADATA_SIZE) ||
        (metadata->state > (uint32_t)OTA_STATE_FAILED) ||
        (metadata->target_address != PLATFORM_APP_ADDR) ||
        (metadata->image_size > PLATFORM_APP_SIZE)) {
        return 0;
    }
    if (((metadata->state == (uint32_t)OTA_STATE_READY) ||
         (metadata->state == (uint32_t)OTA_STATE_APPLYING) ||
         (metadata->state == (uint32_t)OTA_STATE_APPLIED)) &&
        (metadata->image_size < 8U)) {
        return 0;
    }
    return metadata_crc(metadata) == metadata->header_crc32;
}

int OtaMetadata_SelectNewest(const OtaMetadata_t *primary,
                             const OtaMetadata_t *backup,
                             OtaMetadata_t *selected)
{
    int primary_valid;
    int backup_valid;

    if (selected == NULL) {
        return 0;
    }
    primary_valid = OtaMetadata_IsValid(primary);
    backup_valid = OtaMetadata_IsValid(backup);
    if (!primary_valid && !backup_valid) {
        return 0;
    }
    if (primary_valid && (!backup_valid ||
        ((int32_t)(primary->sequence - backup->sequence) >= 0))) {
        *selected = *primary;
    } else {
        *selected = *backup;
    }
    return 1;
}

static uint32_t request_crc(const OtaBootRequest_t *request)
{
    OtaBootRequest_t copy = *request;
    copy.request_crc32 = 0U;
    return OtaCrc32_Calculate((const uint8_t *)&copy, (uint32_t)sizeof(copy));
}

void OtaBootRequest_Init(OtaBootRequest_t *request,
                         uint32_t sequence,
                         uint32_t metadata_address,
                         uint32_t image_crc32)
{
    if (request == NULL) {
        return;
    }
    memset(request, 0, sizeof(*request));
    request->magic = OTA_BOOT_REQUEST_MAGIC;
    request->format_version = OTA_FORMAT_VERSION;
    request->request_size = OTA_BOOT_REQUEST_SIZE;
    request->sequence = sequence;
    request->metadata_address = metadata_address;
    request->image_crc32 = image_crc32;
    request->target_address = PLATFORM_APP_ADDR;
    OtaBootRequest_Finalize(request);
}

void OtaBootRequest_Finalize(OtaBootRequest_t *request)
{
    if (request != NULL) {
        request->request_crc32 = request_crc(request);
    }
}

int OtaBootRequest_IsValid(const OtaBootRequest_t *request)
{
    if ((request == NULL) ||
        (request->magic != OTA_BOOT_REQUEST_MAGIC) ||
        (request->format_version != OTA_FORMAT_VERSION) ||
        (request->request_size != OTA_BOOT_REQUEST_SIZE) ||
        ((request->metadata_address != PLATFORM_METADATA_PRIMARY_ADDR) &&
         (request->metadata_address != PLATFORM_METADATA_BACKUP_ADDR)) ||
        (request->target_address != PLATFORM_APP_ADDR)) {
        return 0;
    }
    return request_crc(request) == request->request_crc32;
}
