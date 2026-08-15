#ifndef BOOTLOADER_BUILD
#include "ota_update_controller.h"

#include "board.h"
#include "bsp_system_reset.h"
#include "ota_boot_request_store.h"
#include "ota_metadata_store.h"
#include "system_time.h"

#include <stdint.h>

#define OTA_RESET_RESPONSE_DELAY_MS 250U

static int update_armed;
static uint32_t reset_deadline;

void OtaUpdate_Init(void)
{
    update_armed = 0;
    reset_deadline = 0U;
}

OtaUpdateStatus_t OtaUpdate_Request(void)
{
    OtaMetadata_t metadata;
    uint32_t metadata_address;

    if (update_armed) return OTA_UPDATE_ERROR_CONFLICT;
    if ((OtaMetadataStore_Load(&metadata, &metadata_address) != OTA_METADATA_STORE_OK) ||
        (metadata.state != (uint32_t)OTA_STATE_READY)) {
        return OTA_UPDATE_ERROR_CONFLICT;
    }
    Board_ForceHeaterOff();
    if (OtaBootRequestStore_Commit(metadata_address, metadata.image_crc32, 0) !=
        OTA_REQUEST_STORE_OK) {
        Board_ForceHeaterOff();
        return OTA_UPDATE_ERROR_STORAGE;
    }
    update_armed = 1;
    reset_deadline = SystemTime_Millis() + OTA_RESET_RESPONSE_DELAY_MS;
    return OTA_UPDATE_OK;
}

void OtaUpdate_Poll(void)
{
    if (!update_armed) return;
    Board_ForceHeaterOff();
    if ((int32_t)(SystemTime_Millis() - reset_deadline) >= 0) {
        SystemControl_RequestReset();
    }
}

int OtaUpdate_IsArmed(void)
{
    return update_armed;
}

#endif /* BOOTLOADER_BUILD */

