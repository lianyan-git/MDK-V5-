#ifndef BOOTLOADER_BUILD
#include "esp_at.h"

#include "bsp_esp_uart.h"
#include "system_time.h"

#include <stddef.h>
#include <string.h>

#define ESP_AT_POWER_DELAY_MS    1000U
#define ESP_AT_COMMAND_TIMEOUT_MS 2000U
#define ESP_AT_TX_TIMEOUT_MS       100U
#define ESP_AT_MAX_RETRIES           2U
#define ESP_AT_POLL_BYTE_BUDGET     64U

static const char *const commands[] = {
    "AT\r\n",
    "ATE0\r\n",
    "AT+CWMODE=2\r\n",
    "AT+CIPAP=\"192.168.99.100\"\r\n",
    "AT+CWSAP=\"Dryer_AP\",\"\",6,0\r\n",
    "AT+CIPMUX=1\r\n",
    "AT+CIPSERVER=1,80\r\n"
};

static EspAtState_t state;
static EspAtError_t error_code;
static uint8_t command_index;
static uint8_t retries;
static uint32_t deadline;
static char response[160];
static uint16_t response_length;
static uint8_t ready_rx[ESP_AT_READY_RX_CAPACITY];
static uint16_t ready_head;
static uint16_t ready_tail;
static int ready_overflow;

static int deadline_reached(uint32_t now)
{
    return ((int32_t)(now - deadline) >= 0);
}

static void clear_response(void)
{
    response_length = 0U;
    response[0] = '\0';
}

static int response_has(const char *token)
{
    return strstr(response, token) != NULL;
}

static int send_current_command(void)
{
    const char *command = commands[command_index];
    clear_response();
    if (EspUart_Write((const uint8_t *)command,
                      (uint16_t)strlen(command),
                      ESP_AT_TX_TIMEOUT_MS) != ESP_UART_OK) {
        error_code = ESP_AT_ERROR_TX;
        state = ESP_AT_STATE_ERROR;
        return 0;
    }
    deadline = SystemTime_Millis() + ESP_AT_COMMAND_TIMEOUT_MS;
    state = ESP_AT_STATE_COMMAND_WAIT;
    return 1;
}

static void retry_or_fail(EspAtError_t reason)
{
    if (retries < ESP_AT_MAX_RETRIES) {
        ++retries;
        (void)send_current_command();
    } else {
        error_code = reason;
        state = ESP_AT_STATE_ERROR;
    }
}

static void append_response(uint8_t byte)
{
    if (response_length < (uint16_t)(sizeof(response) - 1U)) {
        response[response_length++] = (char)byte;
    } else {
        memmove(response, &response[1], sizeof(response) - 2U);
        response[sizeof(response) - 2U] = (char)byte;
        response_length = (uint16_t)(sizeof(response) - 1U);
    }
    response[response_length] = '\0';
}

static void append_ready(uint8_t byte)
{
    uint16_t next = (uint16_t)((ready_head + 1U) % ESP_AT_READY_RX_CAPACITY);
    if (next == ready_tail) {
        ready_overflow = 1;
        return;
    }
    ready_rx[ready_head] = byte;
    ready_head = next;
}

void EspAt_Init(void)
{
    EspUart_Init();
    EspUart_ClearRx();
    EspUart_SetEnabled(1);
    state = ESP_AT_STATE_POWER_WAIT;
    error_code = ESP_AT_ERROR_NONE;
    command_index = 0U;
    retries = 0U;
    ready_head = 0U;
    ready_tail = 0U;
    ready_overflow = 0;
    clear_response();
    deadline = SystemTime_Millis() + ESP_AT_POWER_DELAY_MS;
}

void EspAt_Poll(void)
{
    uint8_t byte;
    uint8_t budget = ESP_AT_POLL_BYTE_BUDGET;

    if ((state == ESP_AT_STATE_OFF) || (state == ESP_AT_STATE_ERROR)) return;
    if (EspUart_HasOverflow()) {
        error_code = ESP_AT_ERROR_RX_OVERFLOW;
        state = ESP_AT_STATE_ERROR;
        return;
    }
    while ((budget-- != 0U) && EspUart_ReadByte(&byte)) {
        if (state == ESP_AT_STATE_READY) append_ready(byte);
        else append_response(byte);
    }

    if (state == ESP_AT_STATE_POWER_WAIT) {
        if (deadline_reached(SystemTime_Millis())) (void)send_current_command();
        return;
    }
    if (state != ESP_AT_STATE_COMMAND_WAIT) return;

    if (response_has("ERROR") || response_has("FAIL") || response_has("busy")) {
        retry_or_fail(ESP_AT_ERROR_RESPONSE);
    } else if (response_has("OK\r\n")) {
        retries = 0U;
        ++command_index;
        if (command_index >= (uint8_t)(sizeof(commands) / sizeof(commands[0]))) {
            clear_response();
            state = ESP_AT_STATE_READY;
        } else {
            (void)send_current_command();
        }
    } else if (deadline_reached(SystemTime_Millis())) {
        retry_or_fail(ESP_AT_ERROR_TIMEOUT);
    }
}

void EspAt_Retry(void)
{
    EspUart_SetEnabled(0);
    EspUart_ClearRx();
    EspUart_SetEnabled(1);
    state = ESP_AT_STATE_POWER_WAIT;
    error_code = ESP_AT_ERROR_NONE;
    command_index = 0U;
    retries = 0U;
    ready_head = 0U;
    ready_tail = 0U;
    ready_overflow = 0;
    clear_response();
    deadline = SystemTime_Millis() + ESP_AT_POWER_DELAY_MS;
}

void EspAt_Stop(void)
{
    EspUart_SetEnabled(0);
    state = ESP_AT_STATE_OFF;
}

EspAtState_t EspAt_GetState(void) { return state; }
EspAtError_t EspAt_GetError(void) { return error_code; }
const char *EspAt_GetIpAddress(void) { return ESP_AT_AP_IP; }
uint8_t EspAt_GetCommandIndex(void) { return command_index; }

int EspAt_ReadReadyByte(uint8_t *byte)
{
    if ((byte == NULL) || (ready_tail == ready_head)) return 0;
    *byte = ready_rx[ready_tail];
    ready_tail = (uint16_t)((ready_tail + 1U) % ESP_AT_READY_RX_CAPACITY);
    return 1;
}

int EspAt_ReadyRxOverflowed(void)
{
    return ready_overflow;
}

int EspAt_WriteRaw(const uint8_t *data, uint16_t length)
{
    if ((state != ESP_AT_STATE_READY) || ((data == NULL) && (length != 0U))) return -1;
    return (EspUart_Write(data, length, ESP_AT_TX_TIMEOUT_MS) == ESP_UART_OK) ? 0 : -1;
}

#endif /* BOOTLOADER_BUILD */

