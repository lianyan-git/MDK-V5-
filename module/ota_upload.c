#ifndef BOOTLOADER_BUILD
#include "ota_upload.h"

#include "bsp_w25q128.h"
#include "ota_contract.h"
#include "ota_metadata_store.h"
#include "ota_staging.h"
#include "platform_contract.h"

#include <stddef.h>
#include <string.h>

static OtaUploadState_t upload_state;
static OtaUploadStatus_t upload_error;
static uint32_t declared_body_size;
static uint32_t received_body_size;
static uint32_t image_size;
static uint32_t image_crc;
static char first_boundary[OTA_UPLOAD_BOUNDARY_MAX + 5U];
static uint8_t first_boundary_length;
static uint8_t delimiter[OTA_UPLOAD_BOUNDARY_MAX + 5U];
static uint8_t delimiter_length;
static uint8_t pending[OTA_UPLOAD_BOUNDARY_MAX + 5U];
static uint8_t pending_length;
static char control[OTA_UPLOAD_CONTROL_MAX + 1U];
static uint16_t control_length;
static uint8_t write_buffer[W25Q128_PAGE_SIZE];
static uint16_t write_length;
static uint8_t trailer_length;
static int storage_prepared;

static OtaUploadStatus_t fail(OtaUploadStatus_t error)
{
    upload_error = error;
    upload_state = OTA_UPLOAD_STATE_ERROR;
    return error;
}

void OtaUpload_Reset(void)
{
    upload_state = OTA_UPLOAD_STATE_IDLE;
    upload_error = OTA_UPLOAD_OK;
    declared_body_size = 0U;
    received_body_size = 0U;
    image_size = 0U;
    image_crc = OtaCrc32_Begin();
    first_boundary_length = 0U;
    delimiter_length = 0U;
    pending_length = 0U;
    control_length = 0U;
    write_length = 0U;
    trailer_length = 0U;
}

static OtaUploadStatus_t flush_write_buffer(void)
{
    if (write_length == 0U) return OTA_UPLOAD_NEED_MORE;
    if (W25Q128_Write(PLATFORM_FIRMWARE_ADDR + image_size,
                      write_buffer, write_length) != W25Q128_OK) {
        return fail(OTA_UPLOAD_ERROR_STORAGE);
    }
    image_crc = OtaCrc32_Update(image_crc, write_buffer, write_length);
    image_size += write_length;
    write_length = 0U;
    return OTA_UPLOAD_NEED_MORE;
}

static OtaUploadStatus_t store_firmware_byte(uint8_t byte)
{
    if ((image_size + write_length) >= PLATFORM_FIRMWARE_MAX_SIZE) {
        return fail(OTA_UPLOAD_ERROR_IMAGE_TOO_LARGE);
    }
    write_buffer[write_length++] = byte;
    if (write_length == sizeof(write_buffer)) return flush_write_buffer();
    return OTA_UPLOAD_NEED_MORE;
}

static int is_delimiter_prefix(void)
{
    return (pending_length <= delimiter_length) &&
           (memcmp(pending, delimiter, pending_length) == 0);
}

static OtaUploadStatus_t feed_firmware_byte(uint8_t byte)
{
    OtaUploadStatus_t result;

    pending[pending_length++] = byte;
    while ((pending_length != 0U) && !is_delimiter_prefix()) {
        result = store_firmware_byte(pending[0]);
        if (result < 0) return result;
        --pending_length;
        if (pending_length != 0U) memmove(pending, &pending[1], pending_length);
    }
    if (pending_length == delimiter_length) {
        pending_length = 0U;
        result = flush_write_buffer();
        if (result < 0) return result;
        upload_state = OTA_UPLOAD_STATE_TRAILER;
        trailer_length = 0U;
    }
    return OTA_UPLOAD_NEED_MORE;
}

static OtaUploadStatus_t validate_part_headers(void)
{
    if ((control_length < first_boundary_length + 4U) ||
        (memcmp(control, first_boundary, first_boundary_length) != 0) ||
        (strstr(control, "Content-Disposition: form-data") == NULL) ||
        (strstr(control, "name=\"firmware\"") == NULL) ||
        (strstr(control, "filename=\"") == NULL)) {
        return fail(OTA_UPLOAD_ERROR_FORMAT);
    }
    upload_state = OTA_UPLOAD_STATE_FIRMWARE;
    return OTA_UPLOAD_NEED_MORE;
}

static OtaUploadStatus_t feed_byte(uint8_t byte)
{
    if (upload_state == OTA_UPLOAD_STATE_PART_HEADERS) {
        if (control_length >= OTA_UPLOAD_CONTROL_MAX) {
            return fail(OTA_UPLOAD_ERROR_FORMAT);
        }
        control[control_length++] = (char)byte;
        control[control_length] = '\0';
        if ((control_length >= 4U) &&
            (memcmp(&control[control_length - 4U], "\r\n\r\n", 4U) == 0)) {
            return validate_part_headers();
        }
        return OTA_UPLOAD_NEED_MORE;
    }
    if (upload_state == OTA_UPLOAD_STATE_FIRMWARE) {
        return feed_firmware_byte(byte);
    }
    if (upload_state == OTA_UPLOAD_STATE_TRAILER) {
        if ((trailer_length == 0U) && (byte == '-')) trailer_length = 1U;
        else if ((trailer_length == 1U) && (byte == '-')) {
            trailer_length = 2U;
            upload_state = OTA_UPLOAD_STATE_COMPLETE;
        } else return fail(OTA_UPLOAD_ERROR_FORMAT);
        return OTA_UPLOAD_NEED_MORE;
    }
    if (upload_state == OTA_UPLOAD_STATE_COMPLETE) {
        if ((byte != '\r') && (byte != '\n')) return fail(OTA_UPLOAD_ERROR_FORMAT);
        return OTA_UPLOAD_NEED_MORE;
    }
    return fail(OTA_UPLOAD_ERROR_STATE);
}

OtaUploadStatus_t OtaUpload_Begin(const char *boundary,
                                  uint32_t content_length)
{
    size_t boundary_length;

    OtaUpload_Reset();
    if (boundary == NULL) return fail(OTA_UPLOAD_ERROR_ARGUMENT);
    boundary_length = strlen(boundary);
    if ((boundary_length == 0U) || (boundary_length > OTA_UPLOAD_BOUNDARY_MAX)) {
        return fail(OTA_UPLOAD_ERROR_ARGUMENT);
    }
    if ((content_length == 0U) ||
        (content_length > PLATFORM_FIRMWARE_MAX_SIZE + OTA_UPLOAD_BODY_OVERHEAD_MAX)) {
        return fail(OTA_UPLOAD_ERROR_BODY_SIZE);
    }
    if (!storage_prepared && (OtaUpload_PrepareStorage() < 0)) return upload_error;
    first_boundary[0] = '-';
    first_boundary[1] = '-';
    memcpy(&first_boundary[2], boundary, boundary_length);
    first_boundary[boundary_length + 2U] = '\r';
    first_boundary[boundary_length + 3U] = '\n';
    first_boundary_length = (uint8_t)(boundary_length + 4U);

    delimiter[0] = '\r';
    delimiter[1] = '\n';
    delimiter[2] = '-';
    delimiter[3] = '-';
    memcpy(&delimiter[4], boundary, boundary_length);
    delimiter_length = (uint8_t)(boundary_length + 4U);
    declared_body_size = content_length;

    storage_prepared = 0;
    upload_state = OTA_UPLOAD_STATE_PART_HEADERS;
    return OTA_UPLOAD_NEED_MORE;
}

OtaUploadStatus_t OtaUpload_PrepareStorage(void)
{
    OtaUpload_Reset();
    storage_prepared = 0;
    if (OtaMetadataStore_BeginReceive() != OTA_METADATA_STORE_OK) {
        return fail(OTA_UPLOAD_ERROR_STORAGE);
    }
    if (W25Q128_EraseRange(PLATFORM_FIRMWARE_ADDR,
                           PLATFORM_FIRMWARE_ERASE_SIZE) != W25Q128_OK) {
        return fail(OTA_UPLOAD_ERROR_STORAGE);
    }
    storage_prepared = 1;
    upload_state = OTA_UPLOAD_STATE_IDLE;
    upload_error = OTA_UPLOAD_OK;
    return OTA_UPLOAD_OK;
}

OtaUploadStatus_t OtaUpload_Feed(const uint8_t *data, uint16_t length)
{
    uint16_t index;
    OtaUploadStatus_t result = OTA_UPLOAD_NEED_MORE;

    if ((data == NULL) && (length != 0U)) return fail(OTA_UPLOAD_ERROR_ARGUMENT);
    if ((upload_state == OTA_UPLOAD_STATE_IDLE) ||
        (upload_state == OTA_UPLOAD_STATE_ABORTED) ||
        (upload_state == OTA_UPLOAD_STATE_ERROR)) return OTA_UPLOAD_ERROR_STATE;
    if (length > declared_body_size - received_body_size) {
        return fail(OTA_UPLOAD_ERROR_BODY_SIZE);
    }
    for (index = 0U; index < length; ++index) {
        ++received_body_size;
        result = feed_byte(data[index]);
        if (result < 0) return result;
    }
    return result;
}

OtaUploadStatus_t OtaUpload_Finish(void)
{
    OtaUploadStatus_t result;

    if (upload_state == OTA_UPLOAD_STATE_ERROR) return upload_error;
    if (upload_state == OTA_UPLOAD_STATE_ABORTED) return OTA_UPLOAD_ERROR_INTERRUPTED;
    if ((received_body_size != declared_body_size) ||
        (upload_state != OTA_UPLOAD_STATE_COMPLETE)) {
        return fail(OTA_UPLOAD_ERROR_INCOMPLETE);
    }
    result = flush_write_buffer();
    if (result < 0) return result;
    if (image_size == 0U) return fail(OTA_UPLOAD_ERROR_EMPTY_IMAGE);
    image_crc = OtaCrc32_End(image_crc);
    if (OtaStaging_ValidateAndCommit(image_size, image_crc, 0U, 0, 0) != OTA_ERROR_NONE) {
        return fail(OTA_UPLOAD_ERROR_IMAGE_INVALID);
    }
    upload_error = OTA_UPLOAD_OK;
    return OTA_UPLOAD_OK;
}

void OtaUpload_Abort(void)
{
    if ((upload_state != OTA_UPLOAD_STATE_IDLE) &&
        (upload_state != OTA_UPLOAD_STATE_COMPLETE)) {
        upload_state = OTA_UPLOAD_STATE_ABORTED;
        upload_error = OTA_UPLOAD_ERROR_INTERRUPTED;
        write_length = 0U;
        pending_length = 0U;
    }
}

OtaUploadState_t OtaUpload_GetState(void) { return upload_state; }
OtaUploadStatus_t OtaUpload_GetError(void) { return upload_error; }
uint32_t OtaUpload_GetImageSize(void) { return image_size; }
uint32_t OtaUpload_GetImageCrc32(void) { return image_crc; }
int OtaUpload_IsStoragePrepared(void) { return storage_prepared; }

#endif /* BOOTLOADER_BUILD */

