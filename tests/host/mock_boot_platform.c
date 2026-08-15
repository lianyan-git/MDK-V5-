#include "boot_platform.h"
#include "mock_boot_platform.h"
#include "platform_contract.h"

#define EVENT_DISABLE 1U
#define EVENT_SYSTICK 2U
#define EVENT_CLEAR   3U
#define EVENT_VTOR    4U
#define EVENT_MSP     5U
#define EVENT_JUMP    6U

static uint32_t initial_sp;
static uint32_t reset_vector;
static uint32_t events[8];
static uint32_t event_count;
static uint32_t vector_table;
static uint32_t main_stack;
static uint32_t jump_address;

void MockBootPlatform_Reset(void)
{
    initial_sp = UINT32_C(0xFFFFFFFF);
    reset_vector = UINT32_C(0xFFFFFFFF);
    event_count = 0U;
    vector_table = 0U;
    main_stack = 0U;
    jump_address = 0U;
}

void MockBootPlatform_SetVectors(uint32_t stack, uint32_t reset)
{
    initial_sp = stack;
    reset_vector = reset;
}

uint32_t MockBootPlatform_GetEventCount(void) { return event_count; }
uint32_t MockBootPlatform_GetEvent(uint32_t index) { return events[index]; }
uint32_t MockBootPlatform_GetVectorTable(void) { return vector_table; }
uint32_t MockBootPlatform_GetMainStack(void) { return main_stack; }
uint32_t MockBootPlatform_GetJumpAddress(void) { return jump_address; }

uint32_t BootPlatform_ReadWord(uint32_t address)
{
    return (address == PLATFORM_APP_ADDR) ? initial_sp : reset_vector;
}

void BootPlatform_DisableInterrupts(void) { events[event_count++] = EVENT_DISABLE; }
void BootPlatform_StopSysTick(void) { events[event_count++] = EVENT_SYSTICK; }
void BootPlatform_ClearInterrupts(void) { events[event_count++] = EVENT_CLEAR; }
void BootPlatform_SetVectorTable(uint32_t address)
{
    vector_table = address;
    events[event_count++] = EVENT_VTOR;
}
void BootPlatform_Jump(uint32_t stack, uint32_t address)
{
    main_stack = stack;
    events[event_count++] = EVENT_MSP;
    jump_address = address;
    events[event_count++] = EVENT_JUMP;
}
