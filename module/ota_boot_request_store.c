#include "ota_boot_request_store.h"

#include "bsp_internal_flash.h"
#include "platform_contract.h"

#include <string.h>

#define OTA_REQUEST_SLOT_COUNT (PLATFORM_FLAG_SIZE / OTA_BOOT_REQUEST_SIZE)

static uint32_t slot_address(uint32_t slot)
{
    return PLATFORM_FLAG_ADDR + slot * OTA_BOOT_REQUEST_SIZE;
}

OtaRequestStoreStatus_t OtaBootRequestStore_Load(OtaBootRequest_t *request)
{
    OtaBootRequest_t candidate;
    uint32_t slot;
    int found = 0;

    if (request == 0) return OTA_REQUEST_STORE_ERROR_ARGUMENT;
    for (slot = 0U; slot < OTA_REQUEST_SLOT_COUNT; ++slot) {
        if (InternalFlash_Read(slot_address(slot), (uint8_t *)&candidate,
                               sizeof(candidate)) != INTERNAL_FLASH_OK) {
            return OTA_REQUEST_STORE_ERROR_IO;
        }
        if (OtaBootRequest_IsValid(&candidate) &&
            (!found || ((int32_t)(candidate.sequence - request->sequence) > 0))) {
            *request = candidate;
            found = 1;
        }
    }
    return found ? OTA_REQUEST_STORE_OK : OTA_REQUEST_STORE_NOT_FOUND;
}

OtaRequestStoreStatus_t OtaBootRequestStore_Commit(uint32_t metadata_address,
                                                    uint32_t image_crc32,
                                                    OtaBootRequest_t *request)
{
    OtaBootRequest_t newest;
    OtaBootRequest_t candidate;
    OtaBootRequest_t verify;
    OtaRequestStoreStatus_t load_result;
    uint32_t sequence = 1U;
    uint32_t slot;

    load_result = OtaBootRequestStore_Load(&newest);
    if (load_result == OTA_REQUEST_STORE_OK) sequence = newest.sequence + 1U;
    else if (load_result != OTA_REQUEST_STORE_NOT_FOUND) return load_result;

    for (slot = 0U; slot < OTA_REQUEST_SLOT_COUNT; ++slot) {
        if (InternalFlash_IsErased(slot_address(slot), OTA_BOOT_REQUEST_SIZE)) break;
    }
    if (slot == OTA_REQUEST_SLOT_COUNT) {
        if (InternalFlash_ErasePage(PLATFORM_FLAG_ADDR) != INTERNAL_FLASH_OK) {
            return OTA_REQUEST_STORE_ERROR_IO;
        }
        slot = 0U;
    }
    OtaBootRequest_Init(&candidate, sequence, metadata_address, image_crc32);
    if (!OtaBootRequest_IsValid(&candidate)) return OTA_REQUEST_STORE_ERROR_ARGUMENT;
    if (InternalFlash_Write(slot_address(slot), (const uint8_t *)&candidate,
                            sizeof(candidate)) != INTERNAL_FLASH_OK) {
        return OTA_REQUEST_STORE_ERROR_IO;
    }
    if ((InternalFlash_Read(slot_address(slot), (uint8_t *)&verify,
                            sizeof(verify)) != INTERNAL_FLASH_OK) ||
        !OtaBootRequest_IsValid(&verify) ||
        (memcmp(&candidate, &verify, sizeof(candidate)) != 0)) {
        return OTA_REQUEST_STORE_ERROR_VERIFY;
    }
    if (request != 0) *request = candidate;
    return OTA_REQUEST_STORE_OK;
}

OtaRequestStoreStatus_t OtaBootRequestStore_Clear(void)
{
    return (InternalFlash_ErasePage(PLATFORM_FLAG_ADDR) == INTERNAL_FLASH_OK) ?
           OTA_REQUEST_STORE_OK : OTA_REQUEST_STORE_ERROR_IO;
}
