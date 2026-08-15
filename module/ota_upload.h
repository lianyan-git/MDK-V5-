#ifndef OTA_UPLOAD_H
#define OTA_UPLOAD_H

#include <stdint.h>

#define OTA_UPLOAD_BOUNDARY_MAX 70U
#define OTA_UPLOAD_CONTROL_MAX 512U
#define OTA_UPLOAD_BODY_OVERHEAD_MAX 1024U

typedef enum {
    OTA_UPLOAD_STATE_IDLE = 0,
    OTA_UPLOAD_STATE_PART_HEADERS,
    OTA_UPLOAD_STATE_FIRMWARE,
    OTA_UPLOAD_STATE_TRAILER,
    OTA_UPLOAD_STATE_COMPLETE,
    OTA_UPLOAD_STATE_ABORTED,
    OTA_UPLOAD_STATE_ERROR
} OtaUploadState_t;

typedef enum {
    OTA_UPLOAD_OK = 0,
    OTA_UPLOAD_NEED_MORE = 1,
    OTA_UPLOAD_ERROR_ARGUMENT = -1,
    OTA_UPLOAD_ERROR_STATE = -2,
    OTA_UPLOAD_ERROR_BODY_SIZE = -3,
    OTA_UPLOAD_ERROR_FORMAT = -4,
    OTA_UPLOAD_ERROR_IMAGE_TOO_LARGE = -5,
    OTA_UPLOAD_ERROR_EMPTY_IMAGE = -6,
    OTA_UPLOAD_ERROR_STORAGE = -7,
    OTA_UPLOAD_ERROR_INCOMPLETE = -8,
    OTA_UPLOAD_ERROR_INTERRUPTED = -9,
    OTA_UPLOAD_ERROR_IMAGE_INVALID = -10
} OtaUploadStatus_t;

OtaUploadStatus_t OtaUpload_Begin(const char *boundary,
                                  uint32_t content_length);
OtaUploadStatus_t OtaUpload_PrepareStorage(void);
OtaUploadStatus_t OtaUpload_Feed(const uint8_t *data, uint16_t length);
OtaUploadStatus_t OtaUpload_Finish(void);
void OtaUpload_Abort(void);
void OtaUpload_Reset(void);
OtaUploadState_t OtaUpload_GetState(void);
OtaUploadStatus_t OtaUpload_GetError(void);
uint32_t OtaUpload_GetImageSize(void);
uint32_t OtaUpload_GetImageCrc32(void);
int OtaUpload_IsStoragePrepared(void);

#endif /* OTA_UPLOAD_H */
