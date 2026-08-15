#include "mock_spi1_bus.h"
#include "http_server.h"
#include "ota_contract.h"
#include "ota_http.h"
#include "ota_upload.h"
#include "platform_contract.h"
#include "bsp_w25q128.h"

#include <stdio.h>
#include <string.h>

#define BODY_CAPACITY (PLATFORM_FIRMWARE_MAX_SIZE + OTA_UPLOAD_BODY_OVERHEAD_MAX)
#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static uint8_t body[BODY_CAPACITY];
static uint8_t firmware[PLATFORM_FIRMWARE_MAX_SIZE + 1U];
static char http_response[512];
static uint16_t http_response_length;
static uint8_t http_close_count;

static int http_send(void *context, uint8_t link_id,
                     const uint8_t *data, uint16_t length)
{
    (void)context;
    (void)link_id;
    if (length > sizeof(http_response) - 1U - http_response_length) return -1;
    memcpy(&http_response[http_response_length], data, length);
    http_response_length = (uint16_t)(http_response_length + length);
    http_response[http_response_length] = '\0';
    return 0;
}

static void http_close(void *context, uint8_t link_id)
{
    (void)context;
    (void)link_id;
    ++http_close_count;
}

static void init_http_upload(void)
{
    HttpTransport_t transport = {0, http_send, http_close};
    HttpOtaHandlers_t handlers;
    http_response_length = 0U;
    http_response[0] = '\0';
    http_close_count = 0U;
    HttpServer_Init(&transport);
    OtaHttp_GetHandlers(&handlers);
    HttpServer_SetOtaHandlers(&handlers);
}

static uint32_t append_text(uint32_t offset, const char *text)
{
    size_t length = strlen(text);
    memcpy(&body[offset], text, length);
    return offset + (uint32_t)length;
}

static uint32_t build_body(const char *boundary,
                           const uint8_t *image, uint32_t image_size)
{
    uint32_t length = 0U;
    length = append_text(length, "--");
    length = append_text(length, boundary);
    length = append_text(length,
        "\r\nContent-Disposition: form-data; name=\"firmware\"; filename=\"app.bin\"\r\n"
        "Content-Type: application/octet-stream\r\n\r\n");
    memcpy(&body[length], image, image_size);
    length += image_size;
    length = append_text(length, "\r\n--");
    length = append_text(length, boundary);
    length = append_text(length, "--\r\n");
    return length;
}

static OtaUploadStatus_t feed_pattern(uint32_t length)
{
    static const uint16_t chunks[] = {1U, 2U, 7U, 3U, 31U, 5U};
    uint32_t offset = 0U;
    uint32_t index = 0U;
    OtaUploadStatus_t result = OTA_UPLOAD_NEED_MORE;
    while (offset < length) {
        uint16_t count = chunks[index++ % (sizeof(chunks) / sizeof(chunks[0]))];
        if (count > length - offset) count = (uint16_t)(length - offset);
        result = OtaUpload_Feed(&body[offset], count);
        if (result < 0) return result;
        offset += count;
    }
    return result;
}

int main(void)
{
    static const char boundary[] = "----DryerBoundary7MA4YWxk";
    uint8_t readback[600];
    uint32_t body_length;
    uint32_t index;
    char request_header[256];
    int request_header_length;

    for (index = 0U; index < 600U; ++index) firmware[index] = (uint8_t)(index * 37U);
    firmware[0] = 0x00U;
    firmware[1] = 0x50U;
    firmware[2] = 0x00U;
    firmware[3] = 0x20U;
    firmware[4] = 0x01U;
    firmware[5] = 0x49U;
    firmware[6] = 0x00U;
    firmware[7] = 0x08U;
    {
        static const char near_boundary[] = "\r\n------DryerBoundary7MA4YWxX";
        memcpy(&firmware[250], near_boundary, sizeof(near_boundary) - 1U);
    }

    MockSpi1_Reset();
    body_length = build_body(boundary, firmware, 600U);
    CHECK(OtaUpload_Begin(boundary, body_length) == OTA_UPLOAD_NEED_MORE);
    CHECK(MockSpi1_GetEraseCount() ==
          1U + PLATFORM_FIRMWARE_ERASE_SIZE / W25Q128_SECTOR_SIZE);
    CHECK(feed_pattern(body_length) == OTA_UPLOAD_NEED_MORE);
    CHECK(OtaUpload_Finish() == OTA_UPLOAD_OK);
    CHECK(OtaUpload_GetState() == OTA_UPLOAD_STATE_COMPLETE);
    CHECK(OtaUpload_GetImageSize() == 600U);
    CHECK(OtaUpload_GetImageCrc32() == OtaCrc32_Calculate(firmware, 600U));
    CHECK(W25Q128_Read(PLATFORM_FIRMWARE_ADDR, readback, sizeof(readback)) == W25Q128_OK);
    CHECK(memcmp(readback, firmware, sizeof(readback)) == 0);

    MockSpi1_Reset();
    init_http_upload();
    request_header_length = snprintf(request_header, sizeof(request_header),
        "POST /upload HTTP/1.1\r\nContent-Type: multipart/form-data; boundary=%s\r\n"
        "Content-Length: %lu\r\n\r\n", boundary, (unsigned long)body_length);
    CHECK(request_header_length > 0 && (size_t)request_header_length < sizeof(request_header));
    CHECK(HttpServer_Feed(0U, (const uint8_t *)request_header,
                          (uint16_t)request_header_length) == HTTP_SERVER_NEED_MORE);
    for (index = 0U; index < body_length; ++index) {
        HttpServerStatus_t result = HttpServer_Feed(0U, &body[index], 1U);
        if (index + 1U == body_length) CHECK(result == HTTP_SERVER_OK);
        else CHECK(result == HTTP_SERVER_NEED_MORE);
    }
    CHECK(strstr(http_response, "HTTP/1.1 200 OK") != NULL);
    CHECK(strstr(http_response, "upload complete") != NULL);
    CHECK(http_close_count == 1U);
    CHECK(OtaUpload_GetImageSize() == 600U);

    MockSpi1_Reset();
    init_http_upload();
    CHECK(HttpServer_Feed(0U, (const uint8_t *)request_header,
                          (uint16_t)request_header_length) == HTTP_SERVER_NEED_MORE);
    CHECK(HttpServer_Feed(0U, body, 80U) == HTTP_SERVER_NEED_MORE);
    HttpServer_Disconnect(0U);
    CHECK(OtaUpload_GetState() == OTA_UPLOAD_STATE_ABORTED);

    MockSpi1_Reset();
    body_length = build_body(boundary, firmware, 0U);
    CHECK(OtaUpload_Begin(boundary, body_length) == OTA_UPLOAD_NEED_MORE);
    CHECK(feed_pattern(body_length) == OTA_UPLOAD_NEED_MORE);
    CHECK(OtaUpload_Finish() == OTA_UPLOAD_ERROR_EMPTY_IMAGE);

    MockSpi1_Reset();
    for (index = 0U; index < sizeof(firmware); ++index) firmware[index] = (uint8_t)index;
    body_length = build_body(boundary, firmware, sizeof(firmware));
    CHECK(body_length <= BODY_CAPACITY);
    CHECK(OtaUpload_Begin(boundary, body_length) == OTA_UPLOAD_NEED_MORE);
    CHECK(feed_pattern(body_length) == OTA_UPLOAD_ERROR_IMAGE_TOO_LARGE);
    CHECK(OtaUpload_GetState() == OTA_UPLOAD_STATE_ERROR);

    MockSpi1_Reset();
    body_length = build_body(boundary, firmware, 300U);
    CHECK(OtaUpload_Begin(boundary, body_length) == OTA_UPLOAD_NEED_MORE);
    CHECK(OtaUpload_Feed(body, (uint16_t)(body_length / 2U)) == OTA_UPLOAD_NEED_MORE);
    OtaUpload_Abort();
    CHECK(OtaUpload_GetState() == OTA_UPLOAD_STATE_ABORTED);
    CHECK(OtaUpload_Finish() == OTA_UPLOAD_ERROR_INTERRUPTED);

    MockSpi1_Reset();
    MockSpi1_SetAcquireBusy(1);
    CHECK(OtaUpload_Begin(boundary, body_length) == OTA_UPLOAD_ERROR_STORAGE);
    MockSpi1_SetAcquireBusy(0);

    MockSpi1_Reset();
    CHECK(OtaUpload_Begin(boundary, body_length + 1U) == OTA_UPLOAD_NEED_MORE);
    CHECK(feed_pattern(body_length) == OTA_UPLOAD_NEED_MORE);
    CHECK(OtaUpload_Finish() == OTA_UPLOAD_ERROR_INCOMPLETE);
    CHECK(OtaUpload_Begin("", 20U) == OTA_UPLOAD_ERROR_ARGUMENT);
    CHECK(OtaUpload_Begin(boundary,
          PLATFORM_FIRMWARE_MAX_SIZE + OTA_UPLOAD_BODY_OVERHEAD_MAX + 1U) ==
          OTA_UPLOAD_ERROR_BODY_SIZE);

    puts("PASS: multipart boundary splits, W25 streaming, limits and interruption");
    return 0;
}
