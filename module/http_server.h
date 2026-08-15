#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

#include "ota_contract.h"

#include <stdint.h>

#define HTTP_MAX_CONNECTIONS  2U
#define HTTP_MAX_HEADER_BYTES 512U
#define HTTP_MAX_LINK_ID       4U

typedef int (*HttpTransportSendFn)(void *context, uint8_t link_id,
                                   const uint8_t *data, uint16_t length);
typedef void (*HttpTransportCloseFn)(void *context, uint8_t link_id);

typedef struct {
    void *context;
    HttpTransportSendFn send;
    HttpTransportCloseFn close;
} HttpTransport_t;

typedef enum {
    HTTP_OTA_RESULT_OK = 0,
    HTTP_OTA_RESULT_BAD_REQUEST,
    HTTP_OTA_RESULT_CONFLICT,
    HTTP_OTA_RESULT_TOO_LARGE,
    HTTP_OTA_RESULT_UNPROCESSABLE,
    HTTP_OTA_RESULT_INTERNAL_ERROR
} HttpOtaResult_t;

typedef HttpOtaResult_t (*HttpUploadBeginFn)(void *context,
                                             const char *boundary,
                                             uint32_t content_length);
typedef HttpOtaResult_t (*HttpUploadDataFn)(void *context,
                                            const uint8_t *data,
                                            uint16_t length);
typedef HttpOtaResult_t (*HttpUploadFinishFn)(void *context);
typedef void (*HttpUploadAbortFn)(void *context);
typedef HttpOtaResult_t (*HttpUpdateRequestFn)(void *context);

typedef struct {
    void *context;
    HttpUploadBeginFn begin;
    HttpUploadDataFn data;
    HttpUploadFinishFn finish;
    HttpUploadAbortFn abort;
    HttpUpdateRequestFn update;
} HttpOtaHandlers_t;

typedef struct {
    char app_version[16];
    char bootloader_version[16];
    OtaState_t ota_state;
    uint32_t staged_size;
    uint32_t staged_crc32;
    int staged_crc_valid;
} HttpApiData_t;

typedef enum {
    HTTP_SERVER_OK = 0,
    HTTP_SERVER_NEED_MORE = 1,
    HTTP_SERVER_ERROR_ARGUMENT = -1,
    HTTP_SERVER_ERROR_TRANSPORT = -2,
    HTTP_SERVER_ERROR_CAPACITY = -3
} HttpServerStatus_t;

void HttpServer_Init(const HttpTransport_t *transport);
void HttpServer_SetApiData(const HttpApiData_t *data);
void HttpServer_SetOtaHandlers(const HttpOtaHandlers_t *handlers);
HttpServerStatus_t HttpServer_Feed(uint8_t link_id,
                                   const uint8_t *data, uint16_t length);
void HttpServer_Disconnect(uint8_t link_id);
uint8_t HttpServer_GetActiveConnections(void);

extern const char HttpUpgradePage[];

#endif /* HTTP_SERVER_H */
