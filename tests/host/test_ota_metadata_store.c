#include "bsp_w25q128.h"
#include "mock_spi1_bus.h"
#include "ota_contract.h"
#include "ota_metadata_store.h"
#include "ota_staging.h"
#include "platform_contract.h"

#include <stddef.h>
#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static void make_valid_image(uint8_t *image, uint32_t size)
{
    uint32_t index;
    for (index = 0U; index < size; ++index) image[index] = (uint8_t)(index * 29U);
    image[0] = 0x00U;
    image[1] = 0x50U;
    image[2] = 0x00U;
    image[3] = 0x20U;
    image[4] = 0x01U;
    image[5] = 0x49U;
    image[6] = 0x00U;
    image[7] = 0x08U;
}

int main(void)
{
    OtaMetadata_t metadata;
    uint32_t address;
    uint8_t image[700];
    uint32_t crc;

    MockSpi1_Reset();
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_NOT_FOUND);
    CHECK(OtaMetadataStore_BeginReceive() == OTA_METADATA_STORE_OK);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(address == PLATFORM_METADATA_PRIMARY_ADDR);
    CHECK(metadata.sequence == 1U && metadata.state == OTA_STATE_RECEIVING);
    CHECK(OtaMetadataStore_Commit(OTA_STATE_READY, 700U, UINT32_C(0x11223344),
                                  7U, OTA_ERROR_NONE, 0, 0) == OTA_METADATA_STORE_OK);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(address == PLATFORM_METADATA_BACKUP_ADDR);
    CHECK(metadata.sequence == 2U && metadata.state == OTA_STATE_READY);

    MockSpi1_CorruptByte(PLATFORM_METADATA_BACKUP_ADDR +
                         (uint32_t)offsetof(OtaMetadata_t, header_crc32), 1U);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(address == PLATFORM_METADATA_PRIMARY_ADDR);
    CHECK(metadata.state == OTA_STATE_RECEIVING);

    MockSpi1_Reset();
    CHECK(OtaMetadataStore_BeginReceive() == OTA_METADATA_STORE_OK);
    CHECK(OtaMetadataStore_Commit(OTA_STATE_READY, 700U, UINT32_C(0xAABBCCDD),
                                  8U, OTA_ERROR_NONE, 0, 0) == OTA_METADATA_STORE_OK);
    MockSpi1_SetProgramFailure(1);
    CHECK(OtaMetadataStore_BeginReceive() == OTA_METADATA_STORE_ERROR_IO);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(metadata.state == OTA_STATE_READY && metadata.sequence == 2U);
    MockSpi1_SetProgramFailure(0);
    CHECK(OtaMetadataStore_BeginReceive() == OTA_METADATA_STORE_OK);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(metadata.state == OTA_STATE_RECEIVING && metadata.sequence == 3U);
    MockSpi1_SetProgramFailure(1);
    CHECK(OtaMetadataStore_Commit(OTA_STATE_READY, 700U, UINT32_C(0x01020304),
                                  9U, OTA_ERROR_NONE, 0, 0) == OTA_METADATA_STORE_ERROR_IO);
    MockSpi1_SetProgramFailure(0);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(metadata.state == OTA_STATE_RECEIVING && metadata.sequence == 3U);

    MockSpi1_Reset();
    make_valid_image(image, sizeof(image));
    crc = OtaCrc32_Calculate(image, sizeof(image));
    CHECK(OtaMetadataStore_BeginReceive() == OTA_METADATA_STORE_OK);
    CHECK(W25Q128_EraseRange(PLATFORM_FIRMWARE_ADDR,
                             PLATFORM_FIRMWARE_ERASE_SIZE) == W25Q128_OK);
    CHECK(W25Q128_Write(PLATFORM_FIRMWARE_ADDR, image, sizeof(image)) == W25Q128_OK);
    CHECK(OtaStaging_ValidateAndCommit(sizeof(image), crc, 10U,
                                      &metadata, &address) == OTA_ERROR_NONE);
    CHECK(metadata.state == OTA_STATE_READY && metadata.image_crc32 == crc);
    CHECK(address == PLATFORM_METADATA_BACKUP_ADDR);

    MockSpi1_CorruptByte(PLATFORM_FIRMWARE_ADDR + 20U, 0x80U);
    CHECK(OtaStaging_ValidateAndCommit(sizeof(image), crc, 10U,
                                      0, 0) == OTA_ERROR_CRC);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(metadata.state == OTA_STATE_FAILED && metadata.last_error == OTA_ERROR_CRC);

    MockSpi1_Reset();
    make_valid_image(image, sizeof(image));
    image[4] &= 0xFEU;
    CHECK(OtaMetadataStore_BeginReceive() == OTA_METADATA_STORE_OK);
    CHECK(W25Q128_EraseRange(PLATFORM_FIRMWARE_ADDR,
                             PLATFORM_FIRMWARE_ERASE_SIZE) == W25Q128_OK);
    CHECK(W25Q128_Write(PLATFORM_FIRMWARE_ADDR, image, sizeof(image)) == W25Q128_OK);
    CHECK(OtaStaging_ValidateAndCommit(sizeof(image),
          OtaCrc32_Calculate(image, sizeof(image)), 11U, 0, 0) == OTA_ERROR_RESET_VECTOR);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(metadata.state == OTA_STATE_FAILED);

    puts("PASS: redundant metadata atomicity, staging vectors and CRC validation");
    return 0;
}
