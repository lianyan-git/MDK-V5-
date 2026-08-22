#include "system_config.h"
#include "ui_manager.h"
#include "bsp_tft_st7789.h"
#include "bsp_encoder.h"
#include "bsp_buzzer.h"
#include "board.h"
#include "system_time.h"
#include "pin_config.h"
#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>

void System_SaveParams(void);

static void time_commit(void)
{
    uint32_t sec = 0;
    sec += (uint32_t)g_sys.time_digits[0] * 36000U;
    sec += (uint32_t)g_sys.time_digits[1] * 3600U;
    sec += (uint32_t)g_sys.time_digits[2] * 600U;
    sec += (uint32_t)g_sys.time_digits[3] * 60U;
    sec += (uint32_t)g_sys.time_digits[4] * 10U;
    sec += (uint32_t)g_sys.time_digits[5];
    g_sys.params.dry_time_sec = sec;
    System_SaveParams();
}

static void param_defaults(void)
{
    g_sys.params.dry_time_sec = 60;
    g_sys.params.target_temp = 50;
    g_sys.params.ptc_max_temp = 80;
    g_sys.params.ptc_cooling_temp = 40;
    g_sys.params.motor_enabled = 1;
    g_sys.params.motor_direction = 0;
    g_sys.params.motor_speed = 5;
    g_sys.params.motor_oscillate = 0;
    g_sys.params.motor_oscillate_angle = 30;
    g_sys.params.motor_driver = MOTOR_DRIVER_A4988;
    g_sys.params.motor_current = 2;
    g_sys.params.motor_stealthchop = 0;
    g_sys.weight_g = 0.0f;
    g_sys.current_screen = SCREEN_TIME_ADJUST;
    g_sys.selected_item = 0;
    g_sys.time_cursor = 0;
    g_sys.run_state = STATE_IDLE;
    g_sys.safety_state = SAFETY_NONE;
    for (uint8_t i = 0; i < 6; i++) g_sys.time_digits[i] = 0;
    uint32_t sec = 60;
    g_sys.time_digits[0] = (uint8_t)(sec / 36000); sec %= 36000;
    g_sys.time_digits[1] = (uint8_t)(sec / 3600);  sec %= 3600;
    g_sys.time_digits[2] = (uint8_t)(sec / 600);   sec %= 600;
    g_sys.time_digits[3] = (uint8_t)(sec / 60);    sec %= 60;
    g_sys.time_digits[4] = (uint8_t)(sec / 10);
    g_sys.time_digits[5] = (uint8_t)(sec % 10);
}

int main(void)
{
    SystemInit();
    SystemTime_Init();
    TFT_Init();
    Backlight_Init();
    g_sys.backlight = 100;
    TFT_SetBrightness(100);
    g_sys.buzzer_vol = 5;
    Buzzer_Init();
    System_LoadParams();
    param_defaults();
    TFT_FillScreen(TFT_COLOR(0xE8, 0xEA, 0xED));
    UI_DrawTimeAdjust();

    while (1) {
        Watchdog_Kick();
        Encoder_Process();
        UI_Update();
    }
}