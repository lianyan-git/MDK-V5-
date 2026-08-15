#ifndef MOCK_ESP_UART_H
#define MOCK_ESP_UART_H

#include <stdint.h>

void MockEspUart_Reset(void);
void MockEspUart_AdvanceMs(uint32_t milliseconds);
void MockEspUart_Inject(const char *text);
void MockEspUart_SetOverflow(int overflow);
void MockEspUart_SetWriteFailure(int fail);
uint32_t MockEspUart_GetWriteCount(void);
const char *MockEspUart_GetWrite(uint32_t index);
int MockEspUart_GetEnabled(void);

#endif /* MOCK_ESP_UART_H */
