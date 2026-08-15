#include "http_server.h"
#include "mock_internal_flash.h"
#include "mock_spi1_bus.h"
#include "ota_boot_request_store.h"
#include "ota_contract.h"
#include "ota_http.h"
#include "ota_metadata_store.h"
#include "ota_update_controller.h"
#include "platform_contract.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static char response[256];
static uint16_t response_length;
static uint8_t close_count;

static int send_data(void *context, uint8_t link_id,
                     const uint8_t *data, uint16_t length)
{
    (void)context;
    (void)link_id;
    if (length > sizeof(response) - 1U - response_length) return -1;
    memcpy(&response[response_length], data, length);
    response_length = (uint16_t)(response_length + length);
    response[response_length] = '\0';
    return 0;
}

static void close_link(void *context, uint8_t link_id)
{
    (void)context;
    (void)link_id;
    ++close_count;
}

int main(void)
{
    OtaBootRequest_t request;
    OtaMetadata_t metadata;
    uint32_t metadata_address;
    uint32_t index;
    HttpTransport_t transport = {0, send_data, close_link};
    HttpOtaHandlers_t handlers;
    static const char update_request[] = "POST /update HTTP/1.1\r\nContent-Length: 0\r\n\r\n";

    MockInternalFlash_Reset();
    CHECK(OtaBootRequestStore_Load(&request) == OTA_REQUEST_STORE_NOT_FOUND);
    CHECK(OtaBootRequestStore_Commit(PLATFORM_METADATA_PRIMARY_ADDR,
                                     UINT32_C(0x11111111), &request) == OTA_REQUEST_STORE_OK);
    CHECK(request.sequence == 1U);
    CHECK(OtaBootRequestStore_Commit(PLATFORM_METADATA_BACKUP_ADDR,
                                     UINT32_C(0x22222222), &request) == OTA_REQUEST_STORE_OK);
    CHECK(request.sequence == 2U);
    MockInternalFlash_CorruptByte(PLATFORM_FLAG_ADDR + OTA_BOOT_REQUEST_SIZE +
                                  (uint32_t)offsetof(OtaBootRequest_t, request_crc32), 1U);
    CHECK(OtaBootRequestStore_Load(&request) == OTA_REQUEST_STORE_OK);
    CHECK(request.sequence == 1U);

    MockInternalFlash_Reset();
    for (index = 0U; index < (PLATFORM_FLAG_SIZE / OTA_BOOT_REQUEST_SIZE + 1U); ++index) {
        CHECK(OtaBootRequestStore_Commit(PLATFORM_METADATA_PRIMARY_ADDR,
              index, &request) == OTA_REQUEST_STORE_OK);
    }
    CHECK(request.sequence == 33U);
    MockInternalFlash_SetWriteFailure(1);
    CHECK(OtaBootRequestStore_Commit(PLATFORM_METADATA_PRIMARY_ADDR,
          99U, 0) == OTA_REQUEST_STORE_ERROR_IO);
    MockInternalFlash_SetWriteFailure(0);
    CHECK(OtaBootRequestStore_Load(&request) == OTA_REQUEST_STORE_OK);
    CHECK(request.sequence == 33U);
    CHECK(OtaBootRequestStore_Clear() == OTA_REQUEST_STORE_OK);
    CHECK(OtaBootRequestStore_Load(&request) == OTA_REQUEST_STORE_NOT_FOUND);

    MockInternalFlash_Reset();
    MockSpi1_Reset();
    OtaUpdate_Init();
    CHECK(OtaUpdate_Request() == OTA_UPDATE_ERROR_CONFLICT);
    CHECK(MockPlatform_GetHeaterOffCount() == 0U);
    CHECK(OtaMetadataStore_BeginReceive() == OTA_METADATA_STORE_OK);
    CHECK(OtaMetadataStore_Commit(OTA_STATE_READY, 1024U, UINT32_C(0xA1B2C3D4),
                                  1U, OTA_ERROR_NONE, &metadata,
                                  &metadata_address) == OTA_METADATA_STORE_OK);

    response_length = 0U;
    close_count = 0U;
    HttpServer_Init(&transport);
    OtaHttp_GetHandlers(&handlers);
    HttpServer_SetOtaHandlers(&handlers);
    CHECK(HttpServer_Feed(0U, (const uint8_t *)update_request,
          (uint16_t)(sizeof(update_request) - 1U)) == HTTP_SERVER_OK);
    CHECK(strstr(response, "HTTP/1.1 200 OK") != NULL);
    CHECK(strstr(response, "update scheduled") != NULL);
    CHECK(close_count == 1U);
    CHECK(OtaUpdate_IsArmed());
    CHECK(MockPlatform_GetHeaterOffCount() == 1U);
    CHECK(OtaBootRequestStore_Load(&request) == OTA_REQUEST_STORE_OK);
    CHECK(request.metadata_address == metadata_address);
    CHECK(request.image_crc32 == metadata.image_crc32);

    MockPlatform_AdvanceMs(249U);
    OtaUpdate_Poll();
    CHECK(MockPlatform_GetResetCount() == 0U);
    CHECK(MockPlatform_GetHeaterOffCount() == 2U);
    MockPlatform_AdvanceMs(1U);
    OtaUpdate_Poll();
    CHECK(MockPlatform_GetResetCount() == 1U);
    CHECK(MockPlatform_GetHeaterOffCount() == 3U);

    MockInternalFlash_Reset();
    OtaUpdate_Init();
    MockInternalFlash_SetWriteFailure(1);
    CHECK(OtaUpdate_Request() == OTA_UPDATE_ERROR_STORAGE);
    CHECK(!OtaUpdate_IsArmed());
    CHECK(MockPlatform_GetResetCount() == 0U);
    MockInternalFlash_SetWriteFailure(0);

    CHECK(OtaMetadataStore_BeginReceive() == OTA_METADATA_STORE_OK);
    OtaUpdate_Init();
    CHECK(OtaUpdate_Request() == OTA_UPDATE_ERROR_CONFLICT);

    puts("PASS: append-only boot requests, READY gate, heater-off and delayed reset");
    return 0;
}
