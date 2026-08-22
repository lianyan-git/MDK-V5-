#ifndef BOOTLOADER_BUILD
#include "ui_manager.h"
#include "system_config.h"
#include "bsp_tft_st7789.h"
#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>

#define TFT_COLOR(r,g,b)  ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))

static uint16_t UI_BG        = TFT_COLOR(0xE8, 0xEA, 0xED);
static uint16_t UI_CARD      = TFT_COLOR(0xFF, 0xFF, 0xFF);
static uint16_t UI_TITLE_BG  = TFT_COLOR(0x00, 0x00, 0x00);
static uint16_t UI_CARD_EDGE = TFT_COLOR(0xDB, 0xDB, 0xDB);
static uint16_t UI_ACCENT    = TFT_COLOR(0x19, 0x76, 0xD2);
static uint16_t UI_ACCENT2   = TFT_COLOR(0xD3, 0x2F, 0x2F);
static uint16_t UI_TEXT_DIM  = TFT_COLOR(0x60, 0x60, 0x60);
static uint16_t UI_TEXT      = TFT_COLOR(0xFF, 0xFF, 0xFF);
static uint16_t COLOR_YELLOW = TFT_COLOR(0xFF, 0xEB, 0x3B);
static uint16_t COLOR_CYAN   = TFT_COLOR(0x00, 0xBC, 0xD4);
static uint16_t COLOR_GREEN  = TFT_COLOR(0x4C, 0xAF, 0x50);

#define BTN_H  18

static void draw_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    TFT_FillRect(x, y, w, 1, color);
    TFT_FillRect(x, (uint16_t)(y + h - 1U), w, 1, color);
    TFT_FillRect(x, y, 1, h, color);
    TFT_FillRect((uint16_t)(x + w - 1U), y, 1, h, color);
}

static void draw_page_title(const char *title, uint16_t accent)
{
    TFT_FillRect(0, 0, 240, 24, UI_TITLE_BG);
    TFT_FillRect(0, 24, 240, 1, UI_CARD_EDGE);
    TFT_DrawString((240U - (uint16_t)(strlen(title) * 12U)) / 2U, 6, title, accent, UI_TITLE_BG, 2);
}

static void draw_btn(uint16_t y, const char *label, uint16_t color, uint8_t selected)
{
    TFT_FillRect(5, y, 125, BTN_H, UI_CARD);
    TFT_DrawString(10, y + 1, label, color, UI_CARD, 2);
    if (selected) draw_frame(5, y, 125, BTN_H, UI_ACCENT);
}

static void draw_degree(uint16_t x, uint16_t y, uint16_t color, uint8_t size)
{
    uint8_t r = size;
    for (uint8_t i = 0; i < 6; i++) {
        TFT_FillRect((uint16_t)(x + i), y, size, size, color);
        TFT_FillRect((uint16_t)(x + i), (uint16_t)(y + 6 * size - size), size, size, color);
    }
    for (uint8_t i = 0; i < 6; i++) {
        TFT_FillRect(x, (uint16_t)(y + i), size, size, color);
        TFT_FillRect((uint16_t)(x + 6 * size - size), (uint16_t)(y + i), size, size, color);
    }
}

static const uint8_t time_digit_x[6] = {36, 56, 72, 92, 108, 128};

static uint8_t ota_screen_drawn = 0;

void UI_ResetOTAScreen(void) { ota_screen_drawn = 0; }

void UI_DrawTimeAdjust(void)
{
    char buf[8];
    uint8_t i;
    TFT_FillScreen(UI_BG);
    draw_page_title("SET TIME", UI_ACCENT);
    for (i = 0; i < 6; i++) {
        sprintf(buf, "%d", g_sys.time_digits[i]);
        TFT_DrawString(time_digit_x[i], 56, buf, COLOR_CYAN, UI_BG, 4);
        if (g_sys.time_cursor == i) draw_frame(time_digit_x[i] - 1, 55, 22, 30, UI_ACCENT2);
    }
    TFT_FillRect(78, 60, 8, 8, UI_ACCENT2);
    TFT_FillRect(78, 72, 8, 8, UI_ACCENT2);
    TFT_FillRect(152, 60, 8, 8, UI_ACCENT2);
    TFT_FillRect(152, 72, 8, 8, UI_ACCENT2);
}

void UI_DrawTimeEdit(void)
{
    char buf[8];
    uint8_t i;
    TFT_FillScreen(UI_BG);
    draw_page_title("EDIT TIME", COLOR_YELLOW);
    for (i = 0; i < 6; i++) {
        sprintf(buf, "%d", g_sys.time_digits[i]);
        TFT_DrawString(time_digit_x[i], 56, buf, UI_TEXT, UI_BG, 4);
        if (g_sys.time_cursor == i) draw_frame(time_digit_x[i] - 1, 55, 22, 30, COLOR_YELLOW);
    }
    TFT_FillRect(78, 60, 8, 8, UI_ACCENT2);
    TFT_FillRect(78, 72, 8, 8, UI_ACCENT2);
    TFT_FillRect(152, 60, 8, 8, UI_ACCENT2);
    TFT_FillRect(152, 72, 8, 8, UI_ACCENT2);
    draw_btn(160, "  Save & Exit", COLOR_GREEN, g_sys.selected_item == 0);
    draw_btn(184, "  Cancel", UI_TEXT_DIM, g_sys.selected_item == 1);
}

void UI_Update(void)
{
    static Screen_t last_screen = (Screen_t)0xFF;
    if (g_sys.current_screen != last_screen) {
        last_screen = g_sys.current_screen;
        switch (g_sys.current_screen) {
        case SCREEN_TIME_ADJUST: UI_DrawTimeAdjust(); break;
        case SCREEN_TIME_EDIT:   UI_DrawTimeEdit();   break;
        default: break;
        }
    }
}
#endif /* BOOTLOADER_BUILD */