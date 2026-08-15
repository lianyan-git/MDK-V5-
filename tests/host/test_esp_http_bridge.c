#include "esp_http_bridge.h"
#include "http_server.h"
#include "mock_esp_bridge.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static uint32_t upload_abort_count;

static HttpOtaResult_t upload_begin(void *context, const char *boundary,
                                    uint32_t content_length)
{
    (void)context;
    (void)boundary;
    (void)content_length;
    return HTTP_OTA_RESULT_OK;
}

static HttpOtaResult_t upload_data(void *context, const uint8_t *data,
                                   uint16_t length)
{
    (void)context;
    (void)data;
    (void)length;
    return HTTP_OTA_RESULT_OK;
}

static HttpOtaResult_t upload_finish(void *context)
{
    (void)context;
    return HTTP_OTA_RESULT_OK;
}

static void upload_abort(void *context)
{
    (void)context;
    ++upload_abort_count;
}

static int write_contains(uint32_t index, const char *text)
{
    size_t length = strlen(text);
    uint16_t write_length = MockEspBridge_GetWriteLength(index);
    uint16_t offset;
    if (length > write_length) return 0;
    for (offset = 0U; offset <= write_length - length; ++offset) {
        if (memcmp(&MockEspBridge_GetWrite(index)[offset], text, length) == 0) return 1;
    }
    return 0;
}

static void init_server(void)
{
    HttpTransport_t transport;
    EspHttpBridge_Init();
    EspHttpBridge_GetTransport(&transport);
    HttpServer_Init(&transport);
}

int main(void)
{
    static const char request[] = "GET /api/data HTTP/1.1\r\nHost:x\r\n\r\n";
    static const char partial_upload[] =
        "POST /upload HTTP/1.1\r\n"
        "Content-Type: multipart/form-data; boundary=x\r\n"
        "Content-Length: 100\r\n\r\nabc";
    char ipd[128];
    int length;
    HttpOtaHandlers_t handlers;

    MockEspBridge_Reset();
    init_server();
    length = snprintf(ipd, sizeof(ipd), "+IPD,0,%u:%s",
                      (unsigned int)(sizeof(request) - 1U), request);
    CHECK(length > 0 && (size_t)length < sizeof(ipd));
    MockEspBridge_Inject("noise\r\n+IP");
    EspHttpBridge_Poll();
    MockEspBridge_Inject(&ipd[3]);
    EspHttpBridge_Poll();
    CHECK(MockEspBridge_GetWriteCount() == 1U);
    CHECK(write_contains(0U, "AT+CIPSEND=0,"));

    MockEspBridge_Inject(">");
    EspHttpBridge_Poll();
    CHECK(MockEspBridge_GetWriteCount() == 2U);
    CHECK(write_contains(1U, "HTTP/1.1 200 OK"));
    CHECK(write_contains(1U, "\"ip\":\"192.168.99.100\""));
    MockEspBridge_Inject("\r\nSEND OK\r\n");
    EspHttpBridge_Poll();
    CHECK(MockEspBridge_GetWriteCount() == 3U);
    CHECK(write_contains(2U, "AT+CIPCLOSE=0"));
    MockEspBridge_Inject("OK\r\n");
    EspHttpBridge_Poll();
    CHECK(EspHttpBridge_GetErrorCount() == 0U);

    MockEspBridge_Reset();
    init_server();
    length = snprintf(ipd, sizeof(ipd), "+IPD,1,%u:%s",
                      (unsigned int)(sizeof(request) - 1U), request);
    CHECK(length > 0 && (size_t)length < sizeof(ipd));
    MockEspBridge_Inject(ipd);
    EspHttpBridge_Poll();
    CHECK(MockEspBridge_GetWriteCount() == 1U);
    MockEspBridge_AdvanceMs(3000U);
    EspHttpBridge_Poll();
    CHECK(EspHttpBridge_GetErrorCount() == 1U);

    MockEspBridge_Reset();
    init_server();
    memset(&handlers, 0, sizeof(handlers));
    handlers.begin = upload_begin;
    handlers.data = upload_data;
    handlers.finish = upload_finish;
    handlers.abort = upload_abort;
    HttpServer_SetOtaHandlers(&handlers);
    upload_abort_count = 0U;
    length = snprintf(ipd, sizeof(ipd), "+IPD,0,%u:%s",
                      (unsigned int)(sizeof(partial_upload) - 1U), partial_upload);
    CHECK(length > 0 && (size_t)length < sizeof(ipd));
    MockEspBridge_Inject(ipd);
    EspHttpBridge_Poll();
    CHECK(HttpServer_GetActiveConnections() == 1U);
    MockEspBridge_Inject("0,CLOSED\r\n");
    EspHttpBridge_Poll();
    CHECK(upload_abort_count == 1U);
    CHECK(HttpServer_GetActiveConnections() == 0U);

    CHECK(strlen(HttpUpgradePage) + 192U < ESP_HTTP_TX_CAPACITY);
    puts("PASS: ESP +IPD fragmentation, queued HTTP response, close and timeout");
    return 0;
}
