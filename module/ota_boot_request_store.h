#ifndef OTA_BOOT_REQUEST_STORE_H
#define OTA_BOOT_REQUEST_STORE_H

#include "ota_contract.h"

#include <stdint.h>

typedef enum {
    OTA_REQUEST_STORE_OK = 0,
    OTA_REQUEST_STORE_NOT_FOUND = 1,
    OTA_REQUEST_STORE_ERROR_ARGUMENT = -1,
    OTA_REQUEST_STORE_ERROR_IO = -2,
    OTA_REQUEST_STORE_ERROR_VERIFY = -3
} OtaRequestStoreStatus_t;

OtaRequestStoreStatus_t OtaBootRequestStore_Load(OtaBootRequest_t *request);
OtaRequestStoreStatus_t OtaBootRequestStore_Commit(uint32_t metadata_address,
                                                    uint32_t image_crc32,
                                                    OtaBootRequest_t *request);
OtaRequestStoreStatus_t OtaBootRequestStore_Clear(void);

#endif /* OTA_BOOT_REQUEST_STORE_H */
