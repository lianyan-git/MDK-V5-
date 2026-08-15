#include "http_server.h"

#include <stdio.h>
#include <string.h>

#define RESPONSE_CAPACITY 8192U
#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static char response[HTTP_MAX_LINK_ID + 1U][RESPONSE_CAPACITY];
static uint16_t response_length[HTTP_MAX_LINK_ID + 1U];
static uint8_t close_count[HTTP_MAX_LINK_ID + 1U];
static int fail_send;

static int mock_send(void *context, uint8_t link_id,
                     const uint8_t *data, uint16_t length)
{
    uint16_t available;
    (void)context;
    if (fail_send) return -1;
    available = (uint16_t)(RESPONSE_CAPACITY - 1U - response_length[link_id]);
    if (length > available) return -1;
    memcpy(&response[link_id][response_length[link_id]], data, length);
    response_length[link_id] = (uint16_t)(response_length[link_id] + length);
    response[link_id][response_length[link_id]] = '\0';
    return 0;
}

static void mock_close(void *context, uint8_t link_id)
{
    (void)context;
    ++close_count[link_id];
}

static void reset_server(void)
{
    HttpTransport_t transport = {NULL, mock_send, mock_close};
    memset(response, 0, sizeof(response));
    memset(response_length, 0, sizeof(response_length));
    memset(close_count, 0, sizeof(close_count));
    fail_send = 0;
    HttpServer_Init(&transport);
}

static int feed_fragmented(uint8_t link_id, const char *request, uint16_t chunk)
{
    size_t offset = 0U;
    size_t length = strlen(request);
    HttpServerStatus_t result = HTTP_SERVER_NEED_MORE;
    while (offset < length) {
        uint16_t count = chunk;
        if ((size_t)count > length - offset) count = (uint16_t)(length - offset);
        result = HttpServer_Feed(link_id, (const uint8_t *)&request[offset], count);
        offset += count;
    }
    return result;
}

int main(void)
{
    HttpApiData_t data;
    uint8_t oversized[HTTP_MAX_HEADER_BYTES + 1U];

    reset_server();
    memset(&data, 0, sizeof(data));
    strcpy(data.app_version, "0.1.7");
    strcpy(data.bootloader_version, "0.1.2");
    data.ota_state = OTA_STATE_READY;
    data.staged_size = 12345U;
    data.staged_crc32 = UINT32_C(305419896);
    data.staged_crc_valid = 1;
    HttpServer_SetApiData(&data);
    CHECK(feed_fragmented(0U, "GET /api/data HTTP/1.1\r\nHost: x\r\n\r\n", 1U) == HTTP_SERVER_OK);
    CHECK(strstr(response[0], "HTTP/1.1 200 OK") != NULL);
    CHECK(strstr(response[0], "\"app_version\":\"0.1.7\"") != NULL);
    CHECK(strstr(response[0], "\"ota_state\":\"READY\"") != NULL);
    CHECK(strstr(response[0], "\"ip\":\"192.168.99.100\"") != NULL);
    CHECK(strstr(response[0], "\"staged_crc32\":305419896") != NULL);
    CHECK(close_count[0] == 1U && HttpServer_GetActiveConnections() == 0U);

    reset_server();
    CHECK(feed_fragmented(1U, "GET / HTTP/1.1\r\n\r\n", 7U) == HTTP_SERVER_OK);
    CHECK(strstr(response[1], "Dryer OTA") != NULL);
    CHECK(strstr(response[1], "x.open('POST','/upload')") != NULL);
    CHECK(strstr(response[1], "fetch('/update'") != NULL);

    reset_server();
    CHECK(feed_fragmented(0U, "GET /missing HTTP/1.1\r\n\r\n", 64U) == HTTP_SERVER_OK);
    CHECK(strstr(response[0], "404 Not Found") != NULL);
    reset_server();
    CHECK(feed_fragmented(0U, "DELETE / HTTP/1.1\r\n\r\n", 64U) == HTTP_SERVER_OK);
    CHECK(strstr(response[0], "405 Method Not Allowed") != NULL);
    reset_server();
    CHECK(feed_fragmented(0U, "GET / NOPE\r\n\r\n", 64U) == HTTP_SERVER_OK);
    CHECK(strstr(response[0], "400 Bad Request") != NULL);

    reset_server();
    memset(oversized, 'A', sizeof(oversized));
    CHECK(HttpServer_Feed(0U, oversized, sizeof(oversized)) == HTTP_SERVER_OK);
    CHECK(strstr(response[0], "431 Request Header Fields Too Large") != NULL);
    CHECK(close_count[0] == 1U);

    reset_server();
    CHECK(HttpServer_Feed(0U, (const uint8_t *)"GET / HTTP/1.1\r\n", 16U) == HTTP_SERVER_NEED_MORE);
    CHECK(HttpServer_Feed(1U, (const uint8_t *)"GET / HTTP/1.1\r\n", 16U) == HTTP_SERVER_NEED_MORE);
    CHECK(HttpServer_GetActiveConnections() == HTTP_MAX_CONNECTIONS);
    CHECK(HttpServer_Feed(2U, (const uint8_t *)"G", 1U) == HTTP_SERVER_ERROR_CAPACITY);
    CHECK(strstr(response[2], "503 Service Unavailable") != NULL);
    HttpServer_Disconnect(0U);
    CHECK(HttpServer_GetActiveConnections() == 1U);

    reset_server();
    fail_send = 1;
    CHECK(feed_fragmented(0U, "GET / HTTP/1.1\r\n\r\n", 64U) == HTTP_SERVER_ERROR_TRANSPORT);
    CHECK(close_count[0] == 1U);

    puts("PASS: HTTP fragmentation, routes, limits, disconnects and API data");
    return 0;
}
