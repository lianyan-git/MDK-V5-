#include "bsp_spi1_bus.h"
#include "bsp_tft_st7789.h"
#include "mock_spi1_bus.h"
#include "mock_tft_port.h"
#include "ota_display.h"

#include <stdio.h>

#define CHECK(condition) do { \
    if (!(condition)) { \
        fprintf(stderr, "FAIL:%s:%d: %s\n", __FILE__, __LINE__, #condition); \
        return 1; \
    } \
} while (0)

int main(void)
{
    uint16_t x = 130U;
    uint16_t y = 235U;
    uint16_t width = 20U;
    uint16_t height = 20U;
    uint32_t before;

    MockSpi1_Reset();
    MockTftPort_Reset();
    CHECK(TFT_ClipRect(&x, &y, &width, &height) == 1);
    CHECK(x == 130U && y == 235U && width == 5U && height == 5U);
    x = TFT_WIDTH;
    CHECK(TFT_ClipRect(&x, &y, &width, &height) == 0);

    CHECK(OtaDisplay_Init() == TFT_OK);
    CHECK(MockTftPort_GetReset() == 1);
    CHECK(MockTftPort_GetDelayMs() == 260U);
    CHECK(MockTftPort_GetBacklight() == 1);
    CHECK(Spi1Bus_GetOwner() == SPI1_BUS_OWNER_NONE);
    CHECK(MockSpi1_GetTftSelectCount() > 0U);
    CHECK(MockSpi1_GetTftMaxTransfer() <= 64U);

    before = MockSpi1_GetTftTransferBytes();
    CHECK(OtaDisplay_ShowNetwork("V0.1", "V0.2", "DRYER-OTA", "192.168.4.1") == TFT_OK);
    CHECK(OtaDisplay_ShowStatus("WAIT UPLOAD") == TFT_OK);
    CHECK(OtaDisplay_ShowProgress(42U) == TFT_OK);
    CHECK(OtaDisplay_ShowProgress(255U) == TFT_OK);
    CHECK(OtaDisplay_ShowError("BAD IMAGE") == TFT_OK);
    CHECK(MockSpi1_GetTftTransferBytes() > before);
    CHECK(Spi1Bus_GetOwner() == SPI1_BUS_OWNER_NONE);

    MockSpi1_SetAcquireBusy(1);
    CHECK(TFT_DrawPixel(0U, 0U, TFT_COLOR_WHITE) == TFT_ERROR_BUS);
    CHECK(Spi1Bus_GetOwner() == SPI1_BUS_OWNER_NONE);
    MockSpi1_SetAcquireBusy(0);

    puts("PASS: TFT clipping, shared display states and SPI ownership");
    return 0;
}
