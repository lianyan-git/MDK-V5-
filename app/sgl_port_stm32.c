/* sgl_port_stm32.c - SGL port for STM32F103C8T6 + ST7789 240x135 */
#include <sgl.h>
#include "bsp_tft_st7789.h"
#include <string.h>

#define SGL_PANEL_WIDTH   240U
#define SGL_PANEL_HEIGHT  135U
#define SGL_BUFFER_LINES  10U

static sgl_color_t fb0[SGL_PANEL_WIDTH * SGL_BUFFER_LINES];
static sgl_color_t fb1[SGL_PANEL_WIDTH * SGL_BUFFER_LINES];

static void flush_area(sgl_area_t *area, sgl_color_t *src)
{
    int16_t w = area->x2 - area->x1 + 1;
    int16_t h = area->y2 - area->y1 + 1;
    TFT_FlushArea((uint16_t)area->x1, (uint16_t)area->y1,
                  (uint16_t)w, (uint16_t)h, src);
    sgl_fbdev_flush_ready();
}

void sgl_port_init(void)
{
    sgl_fbinfo_t fbinfo = {
        .xres = SGL_PANEL_WIDTH,
        .yres = SGL_PANEL_HEIGHT,
        .flush_area = flush_area,
        .buffer[0] = fb0,
        .buffer[1] = fb1,
        .buffer_size = sizeof(fb0),
    };
    sgl_fbdev_register(&fbinfo);
    sgl_init();
}

void sgl_port_tick(void)
{
    sgl_tick_inc(1);
}