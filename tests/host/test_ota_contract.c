#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ota_contract.h"

static int failures = 0;

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
        ++failures; \
    } \
} while (0)

static void test_crc32(void)
{
    static const uint8_t vector[] = "123456789";
    uint32_t crc = OtaCrc32_Begin();
    crc = OtaCrc32_Update(crc, vector, 4U);
    crc = OtaCrc32_Update(crc, vector + 4, 5U);
    CHECK(OtaCrc32_End(crc) == UINT32_C(0xCBF43926));
    CHECK(OtaCrc32_Calculate(vector, 9U) == UINT32_C(0xCBF43926));
}

static void test_vectors(void)
{
    CHECK(OtaImage_ValidateVector(1024U, PLATFORM_SRAM_END,
                                  PLATFORM_APP_ADDR + 0x101U) == OTA_ERROR_NONE);
    CHECK(OtaImage_ValidateVector(0U, PLATFORM_SRAM_END,
                                  PLATFORM_APP_ADDR + 1U) == OTA_ERROR_SIZE);
    CHECK(OtaImage_ValidateVector(1024U, PLATFORM_SRAM_BASE + 1U,
                                  PLATFORM_APP_ADDR + 1U) == OTA_ERROR_STACK_POINTER);
    CHECK(OtaImage_ValidateVector(1024U, PLATFORM_SRAM_END,
                                  PLATFORM_APP_ADDR + 0x100U) == OTA_ERROR_RESET_VECTOR);
    CHECK(OtaImage_ValidateVector(128U, PLATFORM_SRAM_END,
                                  PLATFORM_APP_ADDR + 0x201U) == OTA_ERROR_RESET_VECTOR);
}

static void test_metadata(void)
{
    OtaMetadata_t primary;
    OtaMetadata_t backup;
    OtaMetadata_t selected;

    OtaMetadata_Init(&primary, 10U, OTA_STATE_READY, 1024U, 0x12345678U, 2U);
    CHECK(sizeof(primary) == OTA_METADATA_SIZE);
    CHECK(OtaMetadata_IsValid(&primary));
    backup = primary;
    backup.sequence = 11U;
    OtaMetadata_Finalize(&backup);
    CHECK(OtaMetadata_SelectNewest(&primary, &backup, &selected));
    CHECK(selected.sequence == 11U);

    primary.image_size ^= 1U;
    CHECK(!OtaMetadata_IsValid(&primary));
    CHECK(OtaMetadata_SelectNewest(&primary, &backup, &selected));
    CHECK(selected.sequence == 11U);
    backup.format_version = 99U;
    CHECK(!OtaMetadata_IsValid(&backup));
}

static void test_boot_request(void)
{
    OtaBootRequest_t request;
    OtaBootRequest_Init(&request, 7U, PLATFORM_METADATA_PRIMARY_ADDR, 0xAABBCCDDU);
    CHECK(sizeof(request) == OTA_BOOT_REQUEST_SIZE);
    CHECK(OtaBootRequest_IsValid(&request));
    request.target_address += 4U;
    CHECK(!OtaBootRequest_IsValid(&request));
}

int main(void)
{
    test_crc32();
    test_vectors();
    test_metadata();
    test_boot_request();
    if (failures != 0) {
        return 1;
    }
    puts("PASS: OTA contract CRC, vectors, metadata and boot request");
    return 0;
}
