#ifndef ESP_AT_H
#define ESP_AT_H

#include <stdint.h>

#define ESP_AT_AP_SSID "Dryer_AP"
#define ESP_AT_AP_IP   "192.168.99.100"
#define ESP_AT_READY_RX_CAPACITY 1024U

typedef enum {
    ESP_AT_STATE_OFF = 0,
    ESP_AT_STATE_POWER_WAIT,
    ESP_AT_STATE_COMMAND_WAIT,
    ESP_AT_STATE_READY,
    ESP_AT_STATE_ERROR
} EspAtState_t;

typedef enum {
    ESP_AT_ERROR_NONE = 0,
    ESP_AT_ERROR_TX,
    ESP_AT_ERROR_RESPONSE,
    ESP_AT_ERROR_TIMEOUT,
    ESP_AT_ERROR_RX_OVERFLOW
} EspAtError_t;

void EspAt_Init(void);
void EspAt_Poll(void);
void EspAt_Retry(void);
void EspAt_Stop(void);
EspAtState_t EspAt_GetState(void);
EspAtError_t EspAt_GetError(void);
const char *EspAt_GetIpAddress(void);
uint8_t EspAt_GetCommandIndex(void);
int EspAt_ReadReadyByte(uint8_t *byte);
int EspAt_ReadyRxOverflowed(void);
int EspAt_WriteRaw(const uint8_t *data, uint16_t length);

#endif /* ESP_AT_H */
