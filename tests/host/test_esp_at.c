#include "esp_at.h"
#include "mock_esp_uart.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

static const char *const expected_commands[] = {
    "AT\r\n",
    "ATE0\r\n",
    "AT+CWMODE=2\r\n",
    "AT+CIPAP=\"192.168.99.100\"\r\n",
    "AT+CWSAP=\"Dryer_AP\",\"\",6,0\r\n",
    "AT+CIPMUX=1\r\n",
    "AT+CIPSERVER=1,80\r\n"
};

static int reach_ready(void)
{
    uint32_t index;

    EspAt_Init();
    CHECK(MockEspUart_GetEnabled() == 1);
    CHECK(EspAt_GetState() == ESP_AT_STATE_POWER_WAIT);
    MockEspUart_AdvanceMs(999U);
    EspAt_Poll();
    CHECK(MockEspUart_GetWriteCount() == 0U);
    MockEspUart_AdvanceMs(1U);
    EspAt_Poll();

    for (index = 0U; index < 7U; ++index) {
        CHECK(MockEspUart_GetWriteCount() == index + 1U);
        CHECK(strcmp(MockEspUart_GetWrite(index), expected_commands[index]) == 0);
        MockEspUart_Inject("\r\nO");
        EspAt_Poll();
        CHECK(EspAt_GetState() == ESP_AT_STATE_COMMAND_WAIT);
        MockEspUart_Inject("K\r\n");
        EspAt_Poll();
    }
    CHECK(EspAt_GetState() == ESP_AT_STATE_READY);
    CHECK(strcmp(EspAt_GetIpAddress(), "192.168.99.100") == 0);
    return 0;
}

int main(void)
{
    uint8_t byte;
    const char ready_data[] = "+IPD,0,4:PING";
    unsigned int index;

    MockEspUart_Reset();
    CHECK(reach_ready() == 0);
    MockEspUart_Inject(ready_data);
    EspAt_Poll();
    for (index = 0U; index < sizeof(ready_data) - 1U; ++index) {
        CHECK(EspAt_ReadReadyByte(&byte) == 1);
        CHECK(byte == (uint8_t)ready_data[index]);
    }
    CHECK(EspAt_ReadReadyByte(&byte) == 0);

    MockEspUart_Reset();
    EspAt_Init();
    MockEspUart_AdvanceMs(1000U);
    EspAt_Poll();
    MockEspUart_Inject("ERROR\r\n"); EspAt_Poll();
    MockEspUart_Inject("FAIL\r\n"); EspAt_Poll();
    MockEspUart_Inject("busy p...\r\n"); EspAt_Poll();
    CHECK(EspAt_GetState() == ESP_AT_STATE_ERROR);
    CHECK(EspAt_GetError() == ESP_AT_ERROR_RESPONSE);
    CHECK(MockEspUart_GetWriteCount() == 3U);

    MockEspUart_Reset();
    EspAt_Init();
    MockEspUart_AdvanceMs(1000U); EspAt_Poll();
    MockEspUart_AdvanceMs(2000U); EspAt_Poll();
    MockEspUart_AdvanceMs(2000U); EspAt_Poll();
    MockEspUart_AdvanceMs(2000U); EspAt_Poll();
    CHECK(EspAt_GetState() == ESP_AT_STATE_ERROR);
    CHECK(EspAt_GetError() == ESP_AT_ERROR_TIMEOUT);

    MockEspUart_Reset();
    EspAt_Init();
    MockEspUart_SetOverflow(1);
    EspAt_Poll();
    CHECK(EspAt_GetError() == ESP_AT_ERROR_RX_OVERFLOW);
    EspAt_Retry();
    CHECK(EspAt_GetState() == ESP_AT_STATE_POWER_WAIT);
    EspAt_Stop();
    CHECK(EspAt_GetState() == ESP_AT_STATE_OFF);
    CHECK(MockEspUart_GetEnabled() == 0);

    MockEspUart_Reset();
    EspAt_Init();
    MockEspUart_SetWriteFailure(1);
    MockEspUart_AdvanceMs(1000U);
    EspAt_Poll();
    CHECK(EspAt_GetError() == ESP_AT_ERROR_TX);

    puts("PASS: ESP-AT fragmented responses, AP sequence, timeout and recovery");
    return 0;
}
