#ifndef BOOTLOADER_BUILD
#include "esp_http_bridge.h"

#include "esp_at.h"
#include "system_time.h"

#include <stdio.h>
#include <string.h>

#define BRIDGE_TX_TIMEOUT_MS 3000U

typedef struct {
    int used;
    int started;
    int close_after_send;
    uint8_t link_id;
    uint16_t length;
    uint16_t offset;
    uint8_t data[ESP_HTTP_TX_CAPACITY];
} TxSlot_t;

typedef enum {
    RX_SEARCH_PREFIX = 0,
    RX_LINK_ID,
    RX_LENGTH,
    RX_PAYLOAD
} RxState_t;

typedef enum {
    TX_IDLE = 0,
    TX_WAIT_PROMPT,
    TX_WAIT_SEND_OK,
    TX_WAIT_CLOSE_OK
} TxState_t;

static TxSlot_t tx_slots[ESP_HTTP_TX_SLOT_COUNT];
static int active_slot;
static uint16_t active_chunk;
static TxState_t tx_state;
static uint32_t tx_deadline;
static RxState_t rx_state;
static uint8_t prefix_index;
static uint8_t rx_link_id;
static uint32_t rx_length;
static uint32_t rx_remaining;
static char response_window[40];
static uint8_t response_length;
static uint32_t error_count;

static int transport_send(void *context, uint8_t link_id,
                          const uint8_t *data, uint16_t length)
{
    uint8_t index;
    TxSlot_t *slot = 0;
    (void)context;
    if ((data == 0) && (length != 0U)) return -1;
    for (index = 0U; index < ESP_HTTP_TX_SLOT_COUNT; ++index) {
        if (tx_slots[index].used && (tx_slots[index].link_id == link_id)) {
            slot = &tx_slots[index];
            break;
        }
    }
    if (slot == 0) {
        for (index = 0U; index < ESP_HTTP_TX_SLOT_COUNT; ++index) {
            if (!tx_slots[index].used) {
                slot = &tx_slots[index];
                memset(slot, 0, sizeof(*slot));
                slot->used = 1;
                slot->link_id = link_id;
                break;
            }
        }
    }
    if ((slot == 0) || slot->started ||
        (length > ESP_HTTP_TX_CAPACITY - slot->length)) {
        if ((slot != 0) && !slot->started) memset(slot, 0, sizeof(*slot));
        return -1;
    }
    memcpy(&slot->data[slot->length], data, length);
    slot->length = (uint16_t)(slot->length + length);
    return 0;
}

static void transport_close(void *context, uint8_t link_id)
{
    uint8_t index;
    TxSlot_t *slot = 0;
    (void)context;
    for (index = 0U; index < ESP_HTTP_TX_SLOT_COUNT; ++index) {
        if (tx_slots[index].used && (tx_slots[index].link_id == link_id)) {
            slot = &tx_slots[index];
            break;
        }
    }
    if (slot == 0) {
        for (index = 0U; index < ESP_HTTP_TX_SLOT_COUNT; ++index) {
            if (!tx_slots[index].used) {
                slot = &tx_slots[index];
                memset(slot, 0, sizeof(*slot));
                slot->used = 1;
                slot->link_id = link_id;
                break;
            }
        }
    }
    if (slot != 0) slot->close_after_send = 1;
}

void EspHttpBridge_GetTransport(HttpTransport_t *transport)
{
    if (transport == 0) return;
    transport->context = 0;
    transport->send = transport_send;
    transport->close = transport_close;
}

void EspHttpBridge_Init(void)
{
    memset(tx_slots, 0, sizeof(tx_slots));
    active_slot = -1;
    active_chunk = 0U;
    tx_state = TX_IDLE;
    rx_state = RX_SEARCH_PREFIX;
    prefix_index = 0U;
    rx_link_id = 0U;
    rx_length = 0U;
    rx_remaining = 0U;
    response_length = 0U;
    response_window[0] = '\0';
    error_count = 0U;
}

static void clear_response(void)
{
    response_length = 0U;
    response_window[0] = '\0';
}

static void release_active_slot(void)
{
    if ((active_slot >= 0) && (active_slot < (int)ESP_HTTP_TX_SLOT_COUNT)) {
        memset(&tx_slots[active_slot], 0, sizeof(tx_slots[active_slot]));
    }
    active_slot = -1;
    active_chunk = 0U;
    tx_state = TX_IDLE;
    clear_response();
}

static int send_close_command(TxSlot_t *slot)
{
    char command[24];
    int length = snprintf(command, sizeof(command),
                          "AT+CIPCLOSE=%u\r\n", (unsigned int)slot->link_id);
    if ((length <= 0) || ((size_t)length >= sizeof(command)) ||
        (EspAt_WriteRaw((const uint8_t *)command, (uint16_t)length) != 0)) return 0;
    clear_response();
    tx_state = TX_WAIT_CLOSE_OK;
    tx_deadline = SystemTime_Millis() + BRIDGE_TX_TIMEOUT_MS;
    return 1;
}

static int send_next_chunk(TxSlot_t *slot)
{
    char command[32];
    uint16_t remaining = (uint16_t)(slot->length - slot->offset);
    int length;

    if (remaining == 0U) {
        if (slot->close_after_send) return send_close_command(slot);
        release_active_slot();
        return 1;
    }
    active_chunk = (remaining > ESP_HTTP_TX_CHUNK) ? ESP_HTTP_TX_CHUNK : remaining;
    length = snprintf(command, sizeof(command), "AT+CIPSEND=%u,%u\r\n",
                      (unsigned int)slot->link_id, (unsigned int)active_chunk);
    if ((length <= 0) || ((size_t)length >= sizeof(command)) ||
        (EspAt_WriteRaw((const uint8_t *)command, (uint16_t)length) != 0)) return 0;
    clear_response();
    tx_state = TX_WAIT_PROMPT;
    tx_deadline = SystemTime_Millis() + BRIDGE_TX_TIMEOUT_MS;
    return 1;
}

static void start_pending_tx(void)
{
    uint8_t index;
    if ((tx_state != TX_IDLE) || (EspAt_GetState() != ESP_AT_STATE_READY)) return;
    for (index = 0U; index < ESP_HTTP_TX_SLOT_COUNT; ++index) {
        if (tx_slots[index].used && tx_slots[index].close_after_send) {
            active_slot = (int)index;
            tx_slots[index].started = 1;
            if (!send_next_chunk(&tx_slots[index])) {
                ++error_count;
                release_active_slot();
            }
            return;
        }
    }
}

static void append_response(uint8_t byte)
{
    TxSlot_t *slot;
    char *closed;
    uint8_t index;
    if (response_length < sizeof(response_window) - 1U) {
        response_window[response_length++] = (char)byte;
    } else {
        memmove(response_window, &response_window[1], sizeof(response_window) - 2U);
        response_window[sizeof(response_window) - 2U] = (char)byte;
        response_length = sizeof(response_window) - 1U;
    }
    response_window[response_length] = '\0';
    closed = strstr(response_window, ",CLOSED");
    if ((closed != 0) && (closed > response_window) &&
        ((uint8_t)closed[-1] >= (uint8_t)'0') &&
        ((uint8_t)closed[-1] <= (uint8_t)('0' + HTTP_MAX_LINK_ID))) {
        uint8_t closed_link = (uint8_t)(closed[-1] - '0');
        HttpServer_Disconnect(closed_link);
        for (index = 0U; index < ESP_HTTP_TX_SLOT_COUNT; ++index) {
            if (tx_slots[index].used && (tx_slots[index].link_id == closed_link)) {
                if (active_slot == (int)index) release_active_slot();
                else memset(&tx_slots[index], 0, sizeof(tx_slots[index]));
            }
        }
        clear_response();
        return;
    }
    if ((active_slot < 0) || (active_slot >= (int)ESP_HTTP_TX_SLOT_COUNT)) return;
    slot = &tx_slots[active_slot];

    if ((tx_state == TX_WAIT_PROMPT) && (byte == '>')) {
        clear_response();
        if (EspAt_WriteRaw(&slot->data[slot->offset], active_chunk) != 0) {
            ++error_count;
            release_active_slot();
            return;
        }
        tx_state = TX_WAIT_SEND_OK;
        tx_deadline = SystemTime_Millis() + BRIDGE_TX_TIMEOUT_MS;
    } else if ((tx_state == TX_WAIT_SEND_OK) &&
               (strstr(response_window, "SEND OK") != 0)) {
        slot->offset = (uint16_t)(slot->offset + active_chunk);
        if (!send_next_chunk(slot)) {
            ++error_count;
            release_active_slot();
        }
    } else if ((tx_state == TX_WAIT_CLOSE_OK) &&
               ((strstr(response_window, "OK") != 0) ||
                (strstr(response_window, "ERROR") != 0))) {
        release_active_slot();
    }
}

static void parse_ready_byte(uint8_t byte)
{
    static const char prefix[] = "+IPD,";

    if (rx_state == RX_PAYLOAD) {
        (void)HttpServer_Feed(rx_link_id, &byte, 1U);
        if (--rx_remaining == 0U) rx_state = RX_SEARCH_PREFIX;
        return;
    }
    append_response(byte);
    if (rx_state == RX_SEARCH_PREFIX) {
        if (byte == (uint8_t)prefix[prefix_index]) {
            ++prefix_index;
            if (prefix_index == sizeof(prefix) - 1U) {
                prefix_index = 0U;
                rx_link_id = 0U;
                rx_state = RX_LINK_ID;
            }
        } else {
            prefix_index = (byte == (uint8_t)prefix[0]) ? 1U : 0U;
        }
    } else if (rx_state == RX_LINK_ID) {
        if ((byte >= '0') && (byte <= '9')) {
            rx_link_id = (uint8_t)(rx_link_id * 10U + (byte - '0'));
            if (rx_link_id > HTTP_MAX_LINK_ID) rx_state = RX_SEARCH_PREFIX;
        } else if (byte == ',') {
            rx_length = 0U;
            rx_state = RX_LENGTH;
        } else {
            rx_state = RX_SEARCH_PREFIX;
        }
    } else if (rx_state == RX_LENGTH) {
        if ((byte >= '0') && (byte <= '9')) {
            rx_length = rx_length * 10U + (uint32_t)(byte - '0');
            if (rx_length > UINT32_C(65535)) rx_state = RX_SEARCH_PREFIX;
        } else if ((byte == ':') && (rx_length != 0U)) {
            rx_remaining = rx_length;
            rx_state = RX_PAYLOAD;
        } else {
            rx_state = RX_SEARCH_PREFIX;
        }
    }
}

void EspHttpBridge_Poll(void)
{
    uint8_t byte;
    uint16_t budget = 512U;

    if (EspAt_GetState() != ESP_AT_STATE_READY) return;
    while ((budget-- != 0U) && EspAt_ReadReadyByte(&byte)) parse_ready_byte(byte);
    if ((tx_state != TX_IDLE) &&
        ((int32_t)(SystemTime_Millis() - tx_deadline) >= 0)) {
        ++error_count;
        release_active_slot();
    }
    start_pending_tx();
}

uint32_t EspHttpBridge_GetErrorCount(void)
{
    return error_count;
}

#endif /* BOOTLOADER_BUILD */

