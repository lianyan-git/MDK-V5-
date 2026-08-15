#ifndef OTA_CONTRACT_H
#define OTA_CONTRACT_H

#include <stdint.h>

#include "platform_contract.h"

#define OTA_METADATA_MAGIC          UINT32_C(0x3141544F)
#define OTA_BOOT_REQUEST_MAGIC      UINT32_C(0x3152544F)
#define OTA_FORMAT_VERSION          1U
#define OTA_METADATA_SIZE           64U
#define OTA_BOOT_REQUEST_SIZE       32U

typedef enum {
    OTA_STATE_IDLE = 0,
    OTA_STATE_RECEIVING = 1,
    OTA_STATE_READY = 2,
    OTA_STATE_APPLYING = 3,
    OTA_STATE_APPLIED = 4,
    OTA_STATE_FAILED = 5
} OtaState_t;

typedef enum {
    OTA_ERROR_NONE = 0,
    OTA_ERROR_ARGUMENT = 1,
    OTA_ERROR_FORMAT = 2,
    OTA_ERROR_SIZE = 3,
    OTA_ERROR_STACK_POINTER = 4,
    OTA_ERROR_RESET_VECTOR = 5,
    OTA_ERROR_CRC = 6,
    OTA_ERROR_STORAGE = 7,
    OTA_ERROR_FLASH = 8,
    OTA_ERROR_RETRY_LIMIT = 9
} OtaError_t;

typedef struct {
    uint32_t magic;
    uint16_t format_version;
    uint16_t header_size;
    uint32_t sequence;
    uint32_t state;
    uint32_t image_size;
    uint32_t image_crc32;
    uint32_t target_address;
    uint32_t app_version;
    uint32_t attempt_count;
    uint32_t last_error;
    uint32_t header_crc32;
    uint8_t reserved[20];
} OtaMetadata_t;

typedef struct {
    uint32_t magic;
    uint16_t format_version;
    uint16_t request_size;
    uint32_t sequence;
    uint32_t metadata_address;
    uint32_t image_crc32;
    uint32_t target_address;
    uint32_t reserved;
    uint32_t request_crc32;
} OtaBootRequest_t;

uint32_t OtaCrc32_Begin(void);
uint32_t OtaCrc32_Update(uint32_t crc, const uint8_t *data, uint32_t length);
uint32_t OtaCrc32_End(uint32_t crc);
uint32_t OtaCrc32_Calculate(const uint8_t *data, uint32_t length);

OtaError_t OtaImage_ValidateVector(uint32_t image_size,
                                  uint32_t initial_sp,
                                  uint32_t reset_vector);

void OtaMetadata_Init(OtaMetadata_t *metadata,
                      uint32_t sequence,
                      OtaState_t state,
                      uint32_t image_size,
                      uint32_t image_crc32,
                      uint32_t app_version);
void OtaMetadata_Finalize(OtaMetadata_t *metadata);
int OtaMetadata_IsValid(const OtaMetadata_t *metadata);
int OtaMetadata_SelectNewest(const OtaMetadata_t *primary,
                             const OtaMetadata_t *backup,
                             OtaMetadata_t *selected);

void OtaBootRequest_Init(OtaBootRequest_t *request,
                         uint32_t sequence,
                         uint32_t metadata_address,
                         uint32_t image_crc32);
void OtaBootRequest_Finalize(OtaBootRequest_t *request);
int OtaBootRequest_IsValid(const OtaBootRequest_t *request);

#endif /* OTA_CONTRACT_H */
