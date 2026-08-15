#ifndef MOCK_ESP_BRIDGE_H
#define MOCK_ESP_BRIDGE_H

#include <stdint.h>

void MockEspBridge_Reset(void);
void MockEspBridge_Inject(const char *text);
void MockEspBridge_AdvanceMs(uint32_t milliseconds);
uint32_t MockEspBridge_GetWriteCount(void);
const uint8_t *MockEspBridge_GetWrite(uint32_t index);
uint16_t MockEspBridge_GetWriteLength(uint32_t index);

#endif /* MOCK_ESP_BRIDGE_H */
