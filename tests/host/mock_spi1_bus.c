#include "bsp_spi1_bus.h"
#include "mock_spi1_bus.h"

#include <string.h>

#define MOCK_FLASH_SIZE UINT32_C(0x01000000)
#define MOCK_MAX_PROGRAMS 32U

static uint8_t flash_data[MOCK_FLASH_SIZE];
static uint32_t jedec = UINT32_C(0xEF4018);
static int acquire_busy;
static Spi1BusOwner_t owner;
static int selected;
static uint8_t command;
static uint32_t command_offset;
static uint32_t address;
static int write_enabled;
static uint32_t program_count;
static uint32_t program_addresses[MOCK_MAX_PROGRAMS];
static uint32_t program_lengths[MOCK_MAX_PROGRAMS];
static uint32_t erase_count;
static uint32_t tft_select_count;
static uint32_t tft_transfer_bytes;
static uint32_t tft_max_transfer;
static int program_failure;

void MockSpi1_Reset(void)
{
    memset(flash_data, 0xFF, sizeof(flash_data));
    jedec = UINT32_C(0xEF4018);
    acquire_busy = 0;
    owner = SPI1_BUS_OWNER_NONE;
    selected = 0;
    command = 0U;
    command_offset = 0U;
    address = 0U;
    write_enabled = 0;
    program_count = 0U;
    erase_count = 0U;
    tft_select_count = 0U;
    tft_transfer_bytes = 0U;
    tft_max_transfer = 0U;
    program_failure = 0;
}

void MockSpi1_SetJedecId(uint32_t value) { jedec = value; }
void MockSpi1_SetAcquireBusy(int busy) { acquire_busy = busy; }
uint32_t MockSpi1_GetProgramCount(void) { return program_count; }
uint32_t MockSpi1_GetProgramAddress(uint32_t index) { return program_addresses[index]; }
uint32_t MockSpi1_GetProgramLength(uint32_t index) { return program_lengths[index]; }
uint32_t MockSpi1_GetEraseCount(void) { return erase_count; }
uint32_t MockSpi1_GetTftSelectCount(void) { return tft_select_count; }
uint32_t MockSpi1_GetTftTransferBytes(void) { return tft_transfer_bytes; }
uint32_t MockSpi1_GetTftMaxTransfer(void) { return tft_max_transfer; }
void MockSpi1_SetProgramFailure(int fail) { program_failure = fail; }
void MockSpi1_CorruptByte(uint32_t address, uint8_t xor_mask)
{
    if (address < MOCK_FLASH_SIZE) flash_data[address] ^= xor_mask;
}

Spi1BusStatus_t Spi1Bus_Init(void)
{
    owner = SPI1_BUS_OWNER_NONE;
    return SPI1_BUS_OK;
}

Spi1BusStatus_t Spi1Bus_Acquire(Spi1BusOwner_t requested, uint32_t timeout_ms)
{
    (void)timeout_ms;
    if (acquire_busy || owner != SPI1_BUS_OWNER_NONE) return SPI1_BUS_ERROR_BUSY;
    owner = requested;
    return SPI1_BUS_OK;
}

Spi1BusStatus_t Spi1Bus_Select(Spi1BusOwner_t requested)
{
    if (owner != requested || selected) return SPI1_BUS_ERROR_STATE;
    selected = 1;
    command = 0U;
    command_offset = 0U;
    address = 0U;
    if (requested == SPI1_BUS_OWNER_TFT) ++tft_select_count;
    return SPI1_BUS_OK;
}

static uint8_t process_byte(uint8_t tx)
{
    uint8_t result = UINT8_C(0xFF);

    if (command_offset == 0U) {
        command = tx;
        command_offset = 1U;
        if (command == UINT8_C(0x06)) write_enabled = 1;
        return result;
    }
    if (command == UINT8_C(0x9F)) {
        result = (uint8_t)(jedec >> (8U * (3U - command_offset)));
    } else if (command == UINT8_C(0x05)) {
        result = 0U;
    } else if ((command == UINT8_C(0x03)) || (command == UINT8_C(0x02)) ||
               (command == UINT8_C(0x20))) {
        if (command_offset <= 3U) {
            address = (address << 8) | tx;
        } else if (command == UINT8_C(0x03)) {
            result = flash_data[address++];
        } else if ((command == UINT8_C(0x02)) && write_enabled) {
            flash_data[address++] &= tx;
            if (program_lengths[program_count - 1U] != UINT32_MAX) {
                ++program_lengths[program_count - 1U];
            }
        }
    }
    ++command_offset;
    return result;
}

Spi1BusStatus_t Spi1Bus_Transfer(const uint8_t *tx, uint8_t *rx,
                                 uint32_t length, uint32_t timeout_ms)
{
    uint32_t index;
    (void)timeout_ms;
    if (!selected) return SPI1_BUS_ERROR_STATE;
    if (owner == SPI1_BUS_OWNER_TFT) {
        tft_transfer_bytes += length;
        if (length > tft_max_transfer) tft_max_transfer = length;
        if (rx != 0) memset(rx, 0xFF, length);
        return SPI1_BUS_OK;
    }
    if (program_failure && (command_offset == 0U) && (tx != 0) &&
        (length != 0U) && (tx[0] == UINT8_C(0x02))) {
        return SPI1_BUS_ERROR_STATE;
    }
    for (index = 0U; index < length; ++index) {
        uint8_t input = (tx != 0) ? tx[index] : UINT8_C(0xFF);
        uint8_t output = process_byte(input);
        if (rx != 0) rx[index] = output;
        if ((command == UINT8_C(0x02)) && (command_offset == 4U) &&
            (program_count < MOCK_MAX_PROGRAMS)) {
            program_addresses[program_count] = address;
            program_lengths[program_count] = 0U;
            ++program_count;
        }
    }
    return SPI1_BUS_OK;
}

void Spi1Bus_Deselect(Spi1BusOwner_t requested)
{
    if (owner != requested || !selected) return;
    if ((owner == SPI1_BUS_OWNER_W25Q128) &&
        (command == UINT8_C(0x20)) && write_enabled) {
        uint32_t base = address & ~UINT32_C(0xFFF);
        memset(&flash_data[base], 0xFF, 4096U);
        ++erase_count;
    }
    if ((owner == SPI1_BUS_OWNER_W25Q128) &&
        ((command == UINT8_C(0x02)) || (command == UINT8_C(0x20)))) {
        write_enabled = 0;
    }
    selected = 0;
}

void Spi1Bus_Release(Spi1BusOwner_t requested)
{
    if (owner == requested) {
        Spi1Bus_Deselect(requested);
        owner = SPI1_BUS_OWNER_NONE;
    }
}

Spi1BusOwner_t Spi1Bus_GetOwner(void) { return owner; }
