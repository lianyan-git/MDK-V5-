#ifndef ESP_HTTP_BRIDGE_H
#define ESP_HTTP_BRIDGE_H

#include "http_server.h"

#include <stdint.h>

#define ESP_HTTP_TX_SLOT_COUNT 2U
#define ESP_HTTP_TX_CAPACITY   3072U
#define ESP_HTTP_TX_CHUNK      1024U

void EspHttpBridge_Init(void);
void EspHttpBridge_GetTransport(HttpTransport_t *transport);
void EspHttpBridge_Poll(void);
uint32_t EspHttpBridge_GetErrorCount(void);

#endif /* ESP_HTTP_BRIDGE_H */
