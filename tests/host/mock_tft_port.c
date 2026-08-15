#include "bsp_tft_port.h"
#include "mock_tft_port.h"

static int data_mode;
static int reset_high;
static int backlight_enabled;
static uint32_t delayed_ms;

void MockTftPort_Reset(void)
{
    data_mode = 0;
    reset_high = 1;
    backlight_enabled = 0;
    delayed_ms = 0U;
}

void MockTftPort_Init(void) { MockTftPort_Reset(); }
int MockTftPort_GetBacklight(void) { return backlight_enabled; }
int MockTftPort_GetReset(void) { return reset_high; }
uint32_t MockTftPort_GetDelayMs(void) { return delayed_ms; }

void TftPort_Init(void) { MockTftPort_Init(); }
void TftPort_SetDataMode(int mode) { data_mode = mode; }
void TftPort_SetReset(int high) { reset_high = high; }
void TftPort_SetBacklight(int enabled) { backlight_enabled = enabled; }
void TftPort_DelayMs(uint32_t delay_ms) { delayed_ms += delay_ms; }
