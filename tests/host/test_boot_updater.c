#include "boot_updater.h"
#include "bsp_internal_flash.h"
#include "bsp_w25q128.h"
#include "mock_internal_flash.h"
#include "mock_spi1_bus.h"
#include "ota_boot_request_store.h"
#include "ota_contract.h"
#include "ota_metadata_store.h"
#include "ota_staging.h"
#include "platform_contract.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static uint8_t image[1600];
static uint8_t progress_last;
static uint32_t progress_calls;

static void progress(void *context, uint8_t percent)
{
    (void)context;
    if ((progress_calls != 0U) && (percent < progress_last)) {
        progress_last = 255U;
        return;
    }
    progress_last = percent;
    ++progress_calls;
}

static void make_image(uint32_t size)
{
    uint32_t index;
    for (index = 0U; index < size; ++index) image[index] = (uint8_t)(index * 17U + 3U);
    image[0] = 0x00U;
    image[1] = 0x50U;
    image[2] = 0x00U;
    image[3] = 0x20U;
    image[4] = 0x01U;
    image[5] = 0x49U;
    image[6] = 0x00U;
    image[7] = 0x08U;
}

static int prepare_update(uint32_t size)
{
    OtaMetadata_t metadata;
    uint32_t metadata_address;
    uint32_t crc;

    MockSpi1_Reset();
    MockInternalFlash_Reset();
    make_image(size);
    crc = OtaCrc32_Calculate(image, size);
    if (OtaMetadataStore_BeginReceive() != OTA_METADATA_STORE_OK) return 0;
    if (W25Q128_EraseRange(PLATFORM_FIRMWARE_ADDR,
                           PLATFORM_FIRMWARE_ERASE_SIZE) != W25Q128_OK) return 0;
    if (W25Q128_Write(PLATFORM_FIRMWARE_ADDR, image, size) != W25Q128_OK) return 0;
    if (OtaStaging_ValidateAndCommit(size, crc, 1U,
                                    &metadata, &metadata_address) != OTA_ERROR_NONE) return 0;
    if (OtaBootRequestStore_Commit(metadata_address, crc, 0) != OTA_REQUEST_STORE_OK) return 0;
    return 1;
}

int main(void)
{
    OtaMetadata_t metadata;
    OtaBootRequest_t request;
    uint32_t address;
    uint8_t readback[1600];

    MockSpi1_Reset();
    MockInternalFlash_Reset();
    CHECK(BootUpdater_Run(0, 0) == BOOT_UPDATE_NO_REQUEST);
    CHECK(MockInternalFlash_GetPageEraseCount(PLATFORM_APP_ADDR) == 0U);

    CHECK(prepare_update(1501U));
    progress_last = 0U;
    progress_calls = 0U;
    CHECK(BootUpdater_Run(progress, 0) == BOOT_UPDATE_UPDATED);
    CHECK(progress_last == 100U && progress_calls > 10U);
    CHECK(MockInternalFlash_GetPageEraseCount(PLATFORM_APP_ADDR) == 1U);
    CHECK(MockInternalFlash_GetPageEraseCount(
          PLATFORM_APP_END - PLATFORM_FLASH_PAGE_SIZE) == 1U);
    CHECK(MockInternalFlash_GetPageEraseCount(PLATFORM_BOOT_ADDR) == 0U);
    CHECK(MockInternalFlash_GetPageEraseCount(PLATFORM_WIFI_CONFIG_ADDR) == 0U);
    CHECK(InternalFlash_Read(PLATFORM_APP_ADDR, readback, 1501U) == INTERNAL_FLASH_OK);
    CHECK(memcmp(readback, image, 1501U) == 0);
    CHECK(InternalFlash_Read(PLATFORM_APP_ADDR + 1501U, readback, 1U) == INTERNAL_FLASH_OK);
    CHECK(readback[0] == 0xFFU);
    CHECK(OtaBootRequestStore_Load(&request) == OTA_REQUEST_STORE_NOT_FOUND);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(metadata.state == OTA_STATE_APPLIED && metadata.attempt_count == 1U);
    CHECK(MockPlatform_GetWatchdogKickCount() > 40U);

    CHECK(prepare_update(1500U));
    MockSpi1_CorruptByte(PLATFORM_FIRMWARE_ADDR + 30U, 0x01U);
    CHECK(BootUpdater_Run(0, 0) == BOOT_UPDATE_ERROR_STAGING);
    CHECK(MockInternalFlash_GetPageEraseCount(PLATFORM_APP_ADDR) == 0U);
    CHECK(OtaBootRequestStore_Load(&request) == OTA_REQUEST_STORE_OK);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(metadata.state == OTA_STATE_FAILED && metadata.last_error == OTA_ERROR_CRC);

    CHECK(prepare_update(1500U));
    MockInternalFlash_SetAppWriteFailure(1);
    CHECK(BootUpdater_Run(0, 0) == BOOT_UPDATE_ERROR_FLASH);
    CHECK(OtaBootRequestStore_Load(&request) == OTA_REQUEST_STORE_OK);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(metadata.state == OTA_STATE_FAILED && metadata.attempt_count == 1U);
    MockInternalFlash_SetAppWriteFailure(0);
    CHECK(BootUpdater_Run(0, 0) == BOOT_UPDATE_UPDATED);
    CHECK(OtaMetadataStore_Load(&metadata, &address) == OTA_METADATA_STORE_OK);
    CHECK(metadata.state == OTA_STATE_APPLIED && metadata.attempt_count == 2U);
    CHECK(MockInternalFlash_GetPageEraseCount(PLATFORM_APP_ADDR) == 2U);

    CHECK(prepare_update(1500U));
    CHECK(OtaMetadataStore_CommitWithAttempt(
          OTA_STATE_FAILED, 1500U, OtaCrc32_Calculate(image, 1500U),
          1U, BOOT_UPDATE_MAX_ATTEMPTS, OTA_ERROR_FLASH,
          0, 0) == OTA_METADATA_STORE_OK);
    CHECK(BootUpdater_Run(0, 0) == BOOT_UPDATE_ERROR_RETRY_LIMIT);
    CHECK(MockInternalFlash_GetPageEraseCount(PLATFORM_APP_ADDR) == 0U);

    puts("PASS: Bootloader APP-only erase, odd image, CRC, failure retry and limit");
    return 0;
}
