#ifndef OTA_UPDATE_CONTROLLER_H
#define OTA_UPDATE_CONTROLLER_H

typedef enum {
    OTA_UPDATE_OK = 0,
    OTA_UPDATE_ERROR_CONFLICT = -1,
    OTA_UPDATE_ERROR_STORAGE = -2
} OtaUpdateStatus_t;

void OtaUpdate_Init(void);
OtaUpdateStatus_t OtaUpdate_Request(void);
void OtaUpdate_Poll(void);
int OtaUpdate_IsArmed(void);

#endif /* OTA_UPDATE_CONTROLLER_H */
