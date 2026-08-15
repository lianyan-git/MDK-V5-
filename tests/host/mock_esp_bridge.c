#include "esp_at.h"
#include "mock_esp_bridge.h"
#include "system_time.h"

#include <string.h>

#define MOCK_BRIDGE_RX_CAPACITY 8192U
#define MOCK_BRIDGE_WRITES 32U
#define MOCK_BRIDGE_WRITE_CAPACITY 1100U

static uint8_t rx[MOCK_BRIDGE_RX_CAPACITY];
static uint16_t rx_head;
static uint16_t rx_tail;
static uint8_t writes[MOCK_BRIDGE_WRITES][MOCK_BRIDGE_WRITE_CAPACITY];
static uint16_t write_lengths[MOCK_BRIDGE_WRITES];
static uint32_t write_count;
static uint32_t now_ms;

void MockEspBridge_Reset(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    write_count = 0U;
    now_ms = 0U;
    memset(write_lengths, 0, sizeof(write_lengths));
}

void MockEspBridge_Inject(const char *text)
{
    while (*text != '\0') {
        rx[rx_head] = (uint8_t)*text++;
        rx_head = (uint16_t)((rx_head + 1U) % MOCK_BRIDGE_RX_CAPACITY);
    }
}

void MockEspBridge_AdvanceMs(uint32_t milliseconds) { now_ms += milliseconds; }
uint32_t MockEspBridge_GetWriteCount(void) { return write_count; }
const uint8_t *MockEspBridge_GetWrite(uint32_t index) { return writes[index]; }
uint16_t MockEspBridge_GetWriteLength(uint32_t index) { return write_lengths[index]; }

EspAtState_t EspAt_GetState(void) { return ESP_AT_STATE_READY; }
int EspAt_ReadReadyByte(uint8_t *byte)
{
    if ((byte == 0) || (rx_head == rx_tail)) return 0;
    *byte = rx[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % MOCK_BRIDGE_RX_CAPACITY);
    return 1;
}

int EspAt_WriteRaw(const uint8_t *data, uint16_t length)
{
    if ((write_count >= MOCK_BRIDGE_WRITES) ||
        (length > MOCK_BRIDGE_WRITE_CAPACITY)) return -1;
    memcpy(writes[write_count], data, length);
    write_lengths[write_count] = length;
    ++write_count;
    return 0;
}

uint32_t SystemTime_Millis(void) { return now_ms; }
