#include "bsp_esp_uart.h"
#include "mock_esp_uart.h"
#include "system_time.h"

#include <string.h>

#define MOCK_RX_CAPACITY 4096U
#define MOCK_WRITES      32U
#define MOCK_WRITE_SIZE  96U

static uint8_t rx[MOCK_RX_CAPACITY];
static uint16_t rx_head;
static uint16_t rx_tail;
static char writes[MOCK_WRITES][MOCK_WRITE_SIZE];
static uint32_t write_count;
static uint32_t now_ms;
static int enabled;
static int overflowed;
static int write_failure;

void MockEspUart_Reset(void)
{
    rx_head = 0U;
    rx_tail = 0U;
    write_count = 0U;
    now_ms = 0U;
    enabled = 0;
    overflowed = 0;
    write_failure = 0;
    memset(writes, 0, sizeof(writes));
}

void MockEspUart_AdvanceMs(uint32_t milliseconds) { now_ms += milliseconds; }
void MockEspUart_SetOverflow(int overflow) { overflowed = overflow; }
void MockEspUart_SetWriteFailure(int fail) { write_failure = fail; }
uint32_t MockEspUart_GetWriteCount(void) { return write_count; }
const char *MockEspUart_GetWrite(uint32_t index) { return writes[index]; }
int MockEspUart_GetEnabled(void) { return enabled; }

void MockEspUart_Inject(const char *text)
{
    while (*text != '\0') {
        rx[rx_head] = (uint8_t)*text++;
        rx_head = (uint16_t)((rx_head + 1U) % MOCK_RX_CAPACITY);
    }
}

void EspUart_Init(void) { rx_head = rx_tail = 0U; overflowed = 0; }
void EspUart_SetEnabled(int value) { enabled = value; }

EspUartStatus_t EspUart_Write(const uint8_t *data,
                              uint16_t length, uint32_t timeout_ms)
{
    uint16_t copy_length;
    (void)timeout_ms;
    if (write_failure) return ESP_UART_ERROR_TIMEOUT;
    if (write_count >= MOCK_WRITES) return ESP_UART_ERROR_TIMEOUT;
    copy_length = length;
    if (copy_length >= MOCK_WRITE_SIZE) copy_length = MOCK_WRITE_SIZE - 1U;
    memcpy(writes[write_count], data, copy_length);
    writes[write_count][copy_length] = '\0';
    ++write_count;
    return ESP_UART_OK;
}

int EspUart_ReadByte(uint8_t *byte)
{
    if ((byte == 0) || (rx_head == rx_tail)) return 0;
    *byte = rx[rx_tail];
    rx_tail = (uint16_t)((rx_tail + 1U) % MOCK_RX_CAPACITY);
    return 1;
}

int EspUart_HasOverflow(void) { return overflowed; }
void EspUart_ClearRx(void) { rx_tail = rx_head; overflowed = 0; }
void EspUart_RxIrqHandler(void) { }
uint32_t SystemTime_Millis(void) { return now_ms; }
