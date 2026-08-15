#ifndef BOOTLOADER_BUILD
#include "ota_http.h"

#include "ota_upload.h"
#include "ota_update_controller.h"
#include "upgrade_flag.h"
#include "shared_defs.h"
#include "system_time.h"
#include "stm32f10x.h"
#include <string.h>

static HttpOtaResult_t map_upload_result(OtaUploadStatus_t result)
{
    switch (result) {
    case OTA_UPLOAD_OK:
    case OTA_UPLOAD_NEED_MORE:
        return HTTP_OTA_RESULT_OK;
    case OTA_UPLOAD_ERROR_BODY_SIZE:
    case OTA_UPLOAD_ERROR_IMAGE_TOO_LARGE:
        return HTTP_OTA_RESULT_TOO_LARGE;
    case OTA_UPLOAD_ERROR_EMPTY_IMAGE:
    case OTA_UPLOAD_ERROR_INCOMPLETE:
    case OTA_UPLOAD_ERROR_IMAGE_INVALID:
        return HTTP_OTA_RESULT_UNPROCESSABLE;
    case OTA_UPLOAD_ERROR_STORAGE:
        return HTTP_OTA_RESULT_INTERNAL_ERROR;
    default:
        return HTTP_OTA_RESULT_BAD_REQUEST;
    }
}

static HttpOtaResult_t begin_upload(void *context, const char *boundary,
                                    uint32_t content_length)
{
    (void)context;
    return map_upload_result(OtaUpload_Begin(boundary, content_length));
}

static HttpOtaResult_t feed_upload(void *context, const uint8_t *data,
                                   uint16_t length)
{
    (void)context;
    return map_upload_result(OtaUpload_Feed(data, length));
}

static HttpOtaResult_t finish_upload(void *context)
{
    (void)context;
    return map_upload_result(OtaUpload_Finish());
}

static void abort_upload(void *context)
{
    (void)context;
    OtaUpload_Abort();
}

static HttpOtaResult_t request_update(void *context)
{
    UpgradeFlag_t flag;
    (void)context;

    memset(&flag, 0, sizeof(flag));
    flag.magic = UPGRADE_MAGIC;
    flag.version = 0x00020000;
    flag.status = UPGRADE_STATUS_DOWNLOADED;
    flag.timestamp = SystemTime_Millis();
    UpgradeFlag_Write(&flag);

    NVIC_SystemReset();
    return HTTP_OTA_RESULT_OK;
}

void OtaHttp_GetHandlers(HttpOtaHandlers_t *handlers)
{
    if (handlers == 0) return;
    handlers->context = 0;
    handlers->begin = begin_upload;
    handlers->data = feed_upload;
    handlers->finish = finish_upload;
    handlers->abort = abort_upload;
    handlers->update = request_update;
}

#endif /* BOOTLOADER_BUILD */

