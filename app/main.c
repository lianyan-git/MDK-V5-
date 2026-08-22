#include "system_config.h"
#include "bsp_tft_st7789.h"
#include "bsp_encoder.h"
#include "board.h"
#include "system_time.h"
#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>

extern void sgl_port_init(void);
extern void sgl_port_tick(void);
extern void ui_init(void);

int main(void)
{
    SystemInit();
    SystemTime_Init();
    TFT_Init();
    Backlight_Init();
    g_sys.backlight = 100;
    TFT_SetBrightness(100);
    Buzzer_Init();
    System_LoadParams();

    /* SGL 初始化 */
    sgl_port_init();
    ui_init();

    uint32_t last_tick = 0;
    while (1) {
        Watchdog_Kick();
        Encoder_Process();
        sgl_task_handler();

        uint32_t now = SystemTime_Millis();
        if (now - last_tick >= 10) {
            last_tick = now;
            sgl_port_tick();
        }
    }
}