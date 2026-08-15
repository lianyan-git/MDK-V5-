#ifndef OTA_DISPLAY_H
#define OTA_DISPLAY_H

#include "bsp_tft_st7789.h"

#include <stdint.h>

TftStatus_t OtaDisplay_Init(void);
TftStatus_t OtaDisplay_ShowNetwork(const char *current_version,
                                   const char *target_version,
                                   const char *ssid,
                                   const char *ip_address);
TftStatus_t OtaDisplay_ShowStatus(const char *status);
TftStatus_t OtaDisplay_ShowProgress(uint8_t percent);
TftStatus_t OtaDisplay_ShowError(const char *error);

#endif /* OTA_DISPLAY_H */
