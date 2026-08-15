#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "bsp_w25q128.h"
#include "mock_spi1_bus.h"
#include "platform_contract.h"

static int failures;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #c); ++failures; } } while (0)

int main(void)
{
    uint8_t input[300];
    uint8_t output[300];
    uint32_t index;

    MockSpi1_Reset();
    CHECK(W25Q128_Init() == W25Q128_OK);
    for (index = 0U; index < sizeof(input); ++index) input[index] = (uint8_t)index;
    CHECK(W25Q128_Write(UINT32_C(0x0000FE), input, sizeof(input)) == W25Q128_OK);
    CHECK(MockSpi1_GetProgramCount() == 3U);
    CHECK(MockSpi1_GetProgramAddress(0U) == UINT32_C(0x0000FE));
    CHECK(MockSpi1_GetProgramLength(0U) == 2U);
    CHECK(MockSpi1_GetProgramAddress(1U) == UINT32_C(0x000100));
    CHECK(MockSpi1_GetProgramLength(1U) == 256U);
    CHECK(MockSpi1_GetProgramAddress(2U) == UINT32_C(0x000200));
    CHECK(MockSpi1_GetProgramLength(2U) == 42U);
    memset(output, 0, sizeof(output));
    CHECK(W25Q128_Read(UINT32_C(0x0000FE), output, sizeof(output)) == W25Q128_OK);
    CHECK(memcmp(input, output, sizeof(input)) == 0);

    CHECK(W25Q128_EraseRange(0U, 8192U) == W25Q128_OK);
    CHECK(MockSpi1_GetEraseCount() == 2U);
    CHECK(W25Q128_EraseSector(1U) == W25Q128_ERROR_RANGE);
    CHECK(W25Q128_Read(PLATFORM_EXT_FLASH_SIZE - 2U, output, 3U) == W25Q128_ERROR_RANGE);

    MockSpi1_SetAcquireBusy(1);
    CHECK(W25Q128_Read(0U, output, 1U) == W25Q128_ERROR_BUS);
    MockSpi1_SetAcquireBusy(0);
    MockSpi1_SetJedecId(0U);
    CHECK(W25Q128_Init() == W25Q128_ERROR_ID);

    if (failures != 0) return 1;
    puts("PASS: W25Q128 JEDEC, cross-page IO, erase, range and bus errors");
    return 0;
}
