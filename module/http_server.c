#ifndef BOOTLOADER_BUILD
#include "http_server.h"

#include <stdio.h>
#include <string.h>

typedef struct {
    int active;
    uint8_t link_id;
    uint16_t header_length;
    uint32_t body_remaining;
    int upload_started;
    char header[HTTP_MAX_HEADER_BYTES + 1U];
} HttpConnection_t;

static HttpTransport_t active_transport;
static HttpConnection_t connections[HTTP_MAX_CONNECTIONS];
static HttpApiData_t api_data;
static HttpOtaHandlers_t ota_handlers;

const char HttpUpgradePage[] =
    "<!doctype html><html><head><meta charset=utf-8>"
    "<meta name=viewport content=\"width=device-width,initial-scale=1\">"
    "<title>Dryer OTA</title><style>"
    "body{margin:0;background:#17191c;color:#f5f5f5;font:15px sans-serif}"
    "main{max-width:680px;margin:auto;padding:24px}h1{font-size:24px}"
    ".row{display:flex;justify-content:space-between;padding:8px 0;border-bottom:1px solid #444}"
    "input{width:100%;margin:24px 0}button{padding:10px 14px;margin-right:8px}"
    "progress{width:100%;height:18px;margin:16px 0}.error{color:#ff7777}"
    "</style></head><body><main><h1>Dryer OTA</h1>"
    "<div class=row><span>APP</span><b id=app>...</b></div>"
    "<div class=row><span>Bootloader</span><b id=boot>...</b></div>"
    "<div class=row><span>Status</span><b id=state>...</b></div>"
    "<input id=file type=file accept=.bin,application/octet-stream>"
    "<button id=upload>Upload</button><button id=update disabled>Update</button>"
    "<progress id=progress max=100 value=0></progress><div id=message></div>"
    "<script>const q=x=>document.querySelector(x),m=q('#message');"
    "async function refresh(){let d=await fetch('/api/data').then(r=>r.json());"
    "q('#app').textContent=d.app_version;q('#boot').textContent=d.bootloader_version;"
    "q('#state').textContent=d.ota_state;q('#update').disabled=d.ota_state!='READY'}"
    "q('#upload').onclick=()=>{let f=q('#file').files[0];if(!f)return;"
    "let x=new XMLHttpRequest(),d=new FormData();d.append('firmware',f);"
    "x.upload.onprogress=e=>q('#progress').value=e.lengthComputable?e.loaded*100/e.total:0;"
    "x.onload=()=>{m.textContent=x.responseText;refresh()};x.open('POST','/upload');x.send(d)};"
    "q('#update').onclick=async()=>{let r=await fetch('/update',{method:'POST'});"
    "m.textContent=await r.text();refresh()};refresh();</script></main></body></html>";

static const char *ota_state_name(OtaState_t state)
{
    switch (state) {
    case OTA_STATE_IDLE: return "IDLE";
    case OTA_STATE_RECEIVING: return "RECEIVING";
    case OTA_STATE_READY: return "READY";
    case OTA_STATE_APPLYING: return "APPLYING";
    case OTA_STATE_APPLIED: return "APPLIED";
    case OTA_STATE_FAILED: return "FAILED";
    default: return "UNKNOWN";
    }
}

static HttpConnection_t *find_connection(uint8_t link_id)
{
    uint8_t index;
    for (index = 0U; index < HTTP_MAX_CONNECTIONS; ++index) {
        if (connections[index].active && connections[index].link_id == link_id) {
            return &connections[index];
        }
    }
    return NULL;
}

static HttpConnection_t *open_connection(uint8_t link_id)
{
    uint8_t index;
    HttpConnection_t *connection = find_connection(link_id);
    if (connection != NULL) return connection;
    for (index = 0U; index < HTTP_MAX_CONNECTIONS; ++index) {
        if (!connections[index].active) {
            connections[index].active = 1;
            connections[index].link_id = link_id;
            connections[index].header_length = 0U;
            connections[index].header[0] = '\0';
            return &connections[index];
        }
    }
    return NULL;
}

void HttpServer_Disconnect(uint8_t link_id)
{
    HttpConnection_t *connection = find_connection(link_id);
    if (connection != NULL) {
        if (connection->upload_started && (ota_handlers.abort != NULL)) {
            ota_handlers.abort(ota_handlers.context);
        }
        memset(connection, 0, sizeof(*connection));
    }
}

static HttpServerStatus_t send_response(uint8_t link_id, unsigned int status,
                                        const char *reason, const char *content_type,
                                        const char *body)
{
    char header[192];
    int header_length;
    uint16_t body_length = (uint16_t)strlen(body);
    int failed = 0;

    header_length = snprintf(header, sizeof(header),
        "HTTP/1.1 %u %s\r\nContent-Type: %s\r\nContent-Length: %u\r\n"
        "Connection: close\r\nCache-Control: no-store\r\n\r\n",
        status, reason, content_type, (unsigned int)body_length);
    if ((header_length <= 0) || ((size_t)header_length >= sizeof(header))) failed = 1;
    if (!failed && active_transport.send(active_transport.context, link_id,
            (const uint8_t *)header, (uint16_t)header_length) != 0) failed = 1;
    if (!failed && active_transport.send(active_transport.context, link_id,
            (const uint8_t *)body, body_length) != 0) failed = 1;
    active_transport.close(active_transport.context, link_id);
    HttpServer_Disconnect(link_id);
    return failed ? HTTP_SERVER_ERROR_TRANSPORT : HTTP_SERVER_OK;
}

static HttpServerStatus_t send_api_data(uint8_t link_id)
{
    char json[256];
    char crc[16];
    int length;

    if (api_data.staged_crc_valid) {
        (void)snprintf(crc, sizeof(crc), "%lu", (unsigned long)api_data.staged_crc32);
    } else {
        strcpy(crc, "null");
    }
    length = snprintf(json, sizeof(json),
        "{\"app_version\":\"%s\",\"bootloader_version\":\"%s\","
        "\"wifi_mode\":\"AP\",\"ip\":\"192.168.99.100\","
        "\"ota_state\":\"%s\",\"staged_size\":%lu,\"staged_crc32\":%s}",
        api_data.app_version, api_data.bootloader_version,
        ota_state_name(api_data.ota_state),
        (unsigned long)api_data.staged_size, crc);
    if ((length <= 0) || ((size_t)length >= sizeof(json))) {
        return send_response(link_id, 500U, "Internal Server Error",
                             "text/plain", "response overflow");
    }
    return send_response(link_id, 200U, "OK", "application/json", json);
}

static HttpServerStatus_t send_ota_result(uint8_t link_id, HttpOtaResult_t result,
                                          const char *success_text)
{
    switch (result) {
    case HTTP_OTA_RESULT_OK:
        return send_response(link_id, 200U, "OK", "text/plain", success_text);
    case HTTP_OTA_RESULT_BAD_REQUEST:
        return send_response(link_id, 400U, "Bad Request", "text/plain", "bad upload");
    case HTTP_OTA_RESULT_CONFLICT:
        return send_response(link_id, 409U, "Conflict", "text/plain", "ota state conflict");
    case HTTP_OTA_RESULT_TOO_LARGE:
        return send_response(link_id, 413U, "Content Too Large", "text/plain", "firmware too large");
    case HTTP_OTA_RESULT_UNPROCESSABLE:
        return send_response(link_id, 422U, "Unprocessable Content", "text/plain", "invalid firmware");
    default:
        return send_response(link_id, 500U, "Internal Server Error", "text/plain", "storage error");
    }
}

static int header_name_matches(const char *text, const char *name)
{
    while (*name != '\0') {
        char left = *text++;
        char right = *name++;
        if ((left >= 'A') && (left <= 'Z')) left = (char)(left + ('a' - 'A'));
        if ((right >= 'A') && (right <= 'Z')) right = (char)(right + ('a' - 'A'));
        if (left != right) return 0;
    }
    return 1;
}

static const char *find_header(const char *header, const char *name)
{
    const char *cursor = strstr(header, "\r\n");
    size_t name_length = strlen(name);
    if (cursor == NULL) return NULL;
    cursor += 2;
    while (*cursor != '\0') {
        if (header_name_matches(cursor, name) && cursor[name_length] == ':') {
            cursor += name_length + 1U;
            while ((*cursor == ' ') || (*cursor == '\t')) ++cursor;
            return cursor;
        }
        cursor = strstr(cursor, "\r\n");
        if (cursor == NULL) return NULL;
        cursor += 2;
        if ((cursor[0] == '\r') && (cursor[1] == '\n')) return NULL;
    }
    return NULL;
}

static int parse_content_length(const char *header, uint32_t *value)
{
    const char *cursor = find_header(header, "Content-Length");
    uint32_t parsed = 0U;
    int digits = 0;
    if (cursor == NULL) return 0;
    while ((*cursor >= '0') && (*cursor <= '9')) {
        uint32_t digit = (uint32_t)(*cursor++ - '0');
        if (parsed > (UINT32_MAX - digit) / 10U) return 0;
        parsed = parsed * 10U + digit;
        digits = 1;
    }
    if (!digits || ((cursor[0] != '\r') || (cursor[1] != '\n'))) return 0;
    *value = parsed;
    return 1;
}

static int parse_boundary(const char *header, char boundary[71])
{
    const char *content_type = find_header(header, "Content-Type");
    const char *cursor;
    const char *line_end;
    uint8_t length = 0U;
    int quoted = 0;
    if ((content_type == NULL) ||
        !header_name_matches(content_type, "multipart/form-data")) return 0;
    line_end = strstr(content_type, "\r\n");
    if (line_end == NULL) return 0;
    cursor = strstr(content_type, "boundary=");
    if ((cursor == NULL) || (cursor > line_end)) return 0;
    cursor += 9;
    if (*cursor == '"') { quoted = 1; ++cursor; }
    while ((*cursor != '\0') && (*cursor != '\r') && (*cursor != ';') &&
           (!quoted || (*cursor != '"'))) {
        if (length >= 70U) return 0;
        boundary[length++] = *cursor++;
    }
    if ((length == 0U) || (quoted && (*cursor != '"'))) return 0;
    boundary[length] = '\0';
    return 1;
}

static HttpServerStatus_t dispatch_request(HttpConnection_t *connection)
{
    char *line_end = strstr(connection->header, "\r\n");
    char *method_end;
    char *path_end;
    char method[8];
    char path[64];
    char boundary[71];
    uint32_t content_length;
    HttpOtaResult_t ota_result;
    size_t method_length;
    size_t path_length;

    if (line_end == NULL) {
        return send_response(connection->link_id, 400U, "Bad Request",
                             "text/plain", "bad request");
    }
    method_end = strchr(connection->header, ' ');
    if ((method_end == NULL) || (method_end > line_end)) {
        return send_response(connection->link_id, 400U, "Bad Request",
                             "text/plain", "bad request");
    }
    path_end = strchr(method_end + 1, ' ');
    if ((path_end == NULL) || (path_end > line_end) ||
        (strncmp(path_end + 1, "HTTP/1.", 7U) != 0)) {
        return send_response(connection->link_id, 400U, "Bad Request",
                             "text/plain", "bad request");
    }
    method_length = (size_t)(method_end - connection->header);
    path_length = (size_t)(path_end - method_end - 1);
    if ((method_length == 0U) || (method_length >= sizeof(method)) ||
        (path_length == 0U) || (path_length >= sizeof(path))) {
        return send_response(connection->link_id, 400U, "Bad Request",
                             "text/plain", "bad request");
    }
    memcpy(method, connection->header, method_length);
    method[method_length] = '\0';
    memcpy(path, method_end + 1, path_length);
    path[path_length] = '\0';

    if ((strcmp(method, "POST") == 0) && (strcmp(path, "/upload") == 0)) {
        if ((ota_handlers.begin == NULL) || (ota_handlers.data == NULL) ||
            (ota_handlers.finish == NULL) || (ota_handlers.abort == NULL)) {
            return send_response(connection->link_id, 503U, "Service Unavailable",
                                 "text/plain", "upload unavailable");
        }
        if (!parse_content_length(connection->header, &content_length) ||
            !parse_boundary(connection->header, boundary)) {
            return send_response(connection->link_id, 400U, "Bad Request",
                                 "text/plain", "invalid multipart headers");
        }
        ota_result = ota_handlers.begin(ota_handlers.context, boundary, content_length);
        if (ota_result != HTTP_OTA_RESULT_OK) {
            return send_ota_result(connection->link_id, ota_result, "upload started");
        }
        connection->body_remaining = content_length;
        connection->upload_started = 1;
        return HTTP_SERVER_NEED_MORE;
    }
    if ((strcmp(method, "POST") == 0) && (strcmp(path, "/update") == 0)) {
        if (ota_handlers.update == NULL) {
            return send_response(connection->link_id, 503U, "Service Unavailable",
                                 "text/plain", "update unavailable");
        }
        ota_result = ota_handlers.update(ota_handlers.context);
        return send_ota_result(connection->link_id, ota_result, "update scheduled");
    }
    if (strcmp(method, "GET") != 0) {
        return send_response(connection->link_id, 405U, "Method Not Allowed",
                             "text/plain", "method not allowed");
    }
    if (strcmp(path, "/") == 0) {
        return send_response(connection->link_id, 200U, "OK",
                             "text/html; charset=utf-8", HttpUpgradePage);
    }
    if (strcmp(path, "/api/data") == 0) return send_api_data(connection->link_id);
    return send_response(connection->link_id, 404U, "Not Found",
                         "text/plain", "not found");
}

void HttpServer_Init(const HttpTransport_t *transport)
{
    memset(connections, 0, sizeof(connections));
    memset(&api_data, 0, sizeof(api_data));
    memset(&ota_handlers, 0, sizeof(ota_handlers));
    strcpy(api_data.app_version, "0.1.0");
    strcpy(api_data.bootloader_version, "0.1.0");
    if (transport != NULL) active_transport = *transport;
    else memset(&active_transport, 0, sizeof(active_transport));
}

void HttpServer_SetOtaHandlers(const HttpOtaHandlers_t *handlers)
{
    if (handlers != NULL) ota_handlers = *handlers;
    else memset(&ota_handlers, 0, sizeof(ota_handlers));
}

void HttpServer_SetApiData(const HttpApiData_t *data)
{
    if (data != NULL) api_data = *data;
}

HttpServerStatus_t HttpServer_Feed(uint8_t link_id,
                                   const uint8_t *data, uint16_t length)
{
    HttpConnection_t *connection;
    uint16_t index;

    if ((link_id > HTTP_MAX_LINK_ID) || ((data == NULL) && (length != 0U)) ||
        (active_transport.send == NULL) || (active_transport.close == NULL)) {
        return HTTP_SERVER_ERROR_ARGUMENT;
    }
    connection = open_connection(link_id);
    if (connection == NULL) {
        (void)send_response(link_id, 503U, "Service Unavailable",
                            "text/plain", "connection limit");
        return HTTP_SERVER_ERROR_CAPACITY;
    }
    for (index = 0U; index < length; ++index) {
        if (connection->upload_started) {
            HttpOtaResult_t ota_result;
            if (connection->body_remaining == 0U) {
                return send_response(link_id, 400U, "Bad Request",
                                     "text/plain", "body exceeds content length");
            }
            ota_result = ota_handlers.data(ota_handlers.context, &data[index], 1U);
            if (ota_result != HTTP_OTA_RESULT_OK) {
                return send_ota_result(link_id, ota_result, "upload complete");
            }
            --connection->body_remaining;
            if (connection->body_remaining == 0U) {
                ota_result = ota_handlers.finish(ota_handlers.context);
                connection->upload_started = 0;
                return send_ota_result(link_id, ota_result, "upload complete");
            }
            continue;
        }
        if ((data[index] == 0U) || (connection->header_length >= HTTP_MAX_HEADER_BYTES)) {
            return send_response(link_id, 431U, "Request Header Fields Too Large",
                                 "text/plain", "header too large");
        }
        connection->header[connection->header_length++] = (char)data[index];
        connection->header[connection->header_length] = '\0';
        if ((connection->header_length >= 4U) &&
            (memcmp(&connection->header[connection->header_length - 4U],
                    "\r\n\r\n", 4U) == 0)) {
            HttpServerStatus_t result = dispatch_request(connection);
            if (result != HTTP_SERVER_NEED_MORE) return result;
        }
    }
    return HTTP_SERVER_NEED_MORE;
}

uint8_t HttpServer_GetActiveConnections(void)
{
    uint8_t index;
    uint8_t count = 0U;
    for (index = 0U; index < HTTP_MAX_CONNECTIONS; ++index) {
        if (connections[index].active) ++count;
    }
    return count;
}

#endif /* BOOTLOADER_BUILD */

