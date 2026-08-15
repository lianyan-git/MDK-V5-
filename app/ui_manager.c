#ifndef BOOTLOADER_BUILD
#include "ui_manager.h"
#include "system_config.h"
#include "bsp_tft_st7789.h"
#include "stm32f10x.h"
#include <stdio.h>

#define CARD_H         30
#define CARD_GAP       4
#define ACCENT_W       3
#define SEL_FRAME_W    2
#define BTN_Y          218
#define BTN_H          18
#define BTN_GAP        4

static void draw_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
static void Delay_ms(uint16_t ms);

static void draw_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    TFT_FillRect(x, y, w, SEL_FRAME_W, color);
    TFT_FillRect(x, y + h - SEL_FRAME_W, w, SEL_FRAME_W, color);
    TFT_FillRect(x, y, SEL_FRAME_W, h, color);
    TFT_FillRect(x + w - SEL_FRAME_W, y, SEL_FRAME_W, h, color);
}

static void draw_btn(uint16_t y, const char *label, uint16_t color, uint8_t selected)
{
    TFT_FillRect(5, y, 125, BTN_H, COLOR_DARKGRAY);
    TFT_DrawString(10, y + 3, label, color, COLOR_DARKGRAY, 1);
    if (selected) draw_frame(5, y, 125, BTN_H, COLOR_WHITE);
}

static void draw_icon_temp(uint16_t x, uint16_t y)
{
    TFT_FillRect(x + 2, y, 2, 16, COLOR_ORANGE);
    TFT_FillRect(x, y + 16, 6, 4, COLOR_ORANGE);
}

static void draw_icon_weight(uint16_t x, uint16_t y)
{
    TFT_FillRect(x + 1, y, 5, 2, COLOR_MAGENTA);
    TFT_FillRect(x + 3, y + 2, 1, 9, COLOR_MAGENTA);
    TFT_FillRect(x, y + 11, 7, 3, COLOR_MAGENTA);
    TFT_FillRect(x + 1, y + 8, 1, 3, COLOR_MAGENTA);
    TFT_FillRect(x + 5, y + 8, 1, 3, COLOR_MAGENTA);
}

static void draw_icon_clock(uint16_t x, uint16_t y)
{
    TFT_FillRect(x + 1, y, 5, 1, COLOR_CYAN);
    TFT_FillRect(x, y + 1, 7, 1, COLOR_CYAN);
    TFT_FillRect(x, y + 2, 1, 12, COLOR_CYAN);
    TFT_FillRect(x + 6, y + 2, 1, 12, COLOR_CYAN);
    TFT_FillRect(x + 1, y + 14, 5, 1, COLOR_CYAN);
    TFT_FillRect(x + 2, y + 15, 3, 1, COLOR_CYAN);
    TFT_FillRect(x + 3, y + 5, 1, 5, COLOR_CYAN);
    TFT_FillRect(x + 4, y + 8, 2, 1, COLOR_CYAN);
}

static void draw_icon_ptc(uint16_t x, uint16_t y)
{
    TFT_FillRect(x + 2, y, 3, 6, COLOR_RED);
    TFT_FillRect(x + 1, y + 6, 5, 4, COLOR_RED);
    TFT_FillRect(x, y + 10, 7, 4, COLOR_RED);
    TFT_FillRect(x + 1, y + 14, 5, 3, COLOR_RED);
    TFT_FillRect(x + 2, y + 17, 3, 2, COLOR_RED);
}

static void draw_icon_menu(void)
{
    TFT_FillRect(10, 56, 20, 3, COLOR_WHITE);
    TFT_FillRect(10, 63, 20, 3, COLOR_WHITE);
    TFT_FillRect(10, 70, 20, 3, COLOR_WHITE);
}

static void draw_icon_info(void)
{
    TFT_FillRect(14, 56, 12, 12, COLOR_CYAN);
    TFT_FillRect(12, 58, 16, 8, COLOR_CYAN);
    TFT_FillRect(18, 61, 4, 4, COLOR_BLACK);
    TFT_FillRect(19, 60, 2, 1, COLOR_CYAN);
}

static void draw_icon_gear(void)
{
    TFT_FillRect(15, 57, 6, 10, COLOR_YELLOW);
    TFT_FillRect(12, 60, 12, 4, COLOR_YELLOW);
    TFT_FillRect(17, 55, 2, 14, COLOR_YELLOW);
}

static void draw_icon_motor(void)
{
    TFT_FillRect(12, 58, 12, 3, COLOR_GREEN);
    TFT_FillRect(10, 61, 16, 3, COLOR_GREEN);
    TFT_FillRect(12, 64, 12, 3, COLOR_GREEN);
    TFT_FillRect(17, 56, 2, 12, COLOR_WHITE);
}

static void draw_icon_start(void)
{
    TFT_FillRect(12, 56, 4, 12, COLOR_GREEN);
    TFT_FillRect(16, 59, 8, 6, COLOR_GREEN);
}

static void draw_degree(uint16_t x, uint16_t y, uint16_t color, uint8_t scale)
{
    uint8_t s = scale;
    TFT_FillRect(x, y, s * 3, s, color);
    TFT_FillRect(x, y + s, s, s * 2, color);
    TFT_FillRect(x + s * 2, y + s, s, s * 2, color);
    TFT_FillRect(x, y + s * 3, s * 3, s, color);
}

static void draw_degree(uint16_t x, uint16_t y, uint16_t color, uint8_t scale);

static void draw_card_bg(uint16_t y, uint16_t accent_color)
{
    TFT_FillRect(0, y, ACCENT_W, CARD_H, accent_color);
    TFT_FillRect(ACCENT_W, y, TFT_WIDTH - ACCENT_W, CARD_H, COLOR_BLACK);
}

void UI_ShowBootScreen(void)
{
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(22, 90, "DRYER", COLOR_WHITE, COLOR_BLACK, 3);
    TFT_DrawString(30, 130, "System Boot...", COLOR_GRAY, COLOR_BLACK, 1);
    TFT_FillRect(15, 160, 105, 1, COLOR_WHITE);
    TFT_FillRect(15, 169, 105, 1, COLOR_WHITE);
    TFT_FillRect(15, 160, 1, 10, COLOR_WHITE);
    TFT_FillRect(119, 160, 1, 10, COLOR_WHITE);
    for (int i = 0; i < 103; i += 2) {
        TFT_FillRect(16 + i, 161, 2, 8, COLOR_GREEN);
        Delay_ms(8);
    }
}

void UI_DrawMainScreen(void)
{
    char buf[32];
    uint16_t cy;
    uint32_t h, m, s;
    const char *state_str[] = {"IDLE", "HEAT", "DRY", "COOL", "DONE"};
    uint16_t state_color[] = {COLOR_GRAY, COLOR_RED, COLOR_ORANGE, COLOR_CYAN, COLOR_GREEN};

    TFT_FillScreen(COLOR_BLACK);

    TFT_FillRect(0, 0, TFT_WIDTH, 22, COLOR_DARKGRAY);
    if (g_sys.drying_active) TFT_DrawString(5, 4, "RUNNING", COLOR_GREEN, COLOR_DARKGRAY, 1);
    else TFT_DrawString(5, 4, "DRYER", COLOR_WHITE, COLOR_DARKGRAY, 1);
    if (g_sys.wifi_enabled)
        TFT_DrawString(80, 4, g_sys.wifi_connected ? "W" : "w",
                       g_sys.wifi_connected ? COLOR_GREEN : COLOR_GRAY, COLOR_DARKGRAY, 1);
    TFT_DrawString(100, 4, state_str[g_sys.run_state],
                   state_color[g_sys.run_state], COLOR_DARKGRAY, 1);

    cy = 25;
    draw_card_bg(cy, COLOR_ORANGE);
    draw_icon_temp(8, cy + 6);
    TFT_DrawString(18, cy + 2, "TEMP", COLOR_GRAY, COLOR_BLACK, 1);
    sprintf(buf, "%.1f", g_sys.current_temp);
    TFT_DrawString(60, cy, buf, COLOR_WHITE, COLOR_BLACK, 2);
    draw_degree(104, cy + 2, COLOR_ORANGE, 1);
    sprintf(buf, "%d", g_sys.params.target_temp);
    TFT_DrawString(60, cy + 16, buf, COLOR_ORANGE, COLOR_BLACK, 1);
    draw_degree(72, cy + 16, COLOR_ORANGE, 1);
    if (g_sys.selected_item == 0) draw_frame(0, cy, TFT_WIDTH, CARD_H, COLOR_WHITE);

    cy = 25 + CARD_H + CARD_GAP;
    draw_card_bg(cy, COLOR_MAGENTA);
    draw_icon_weight(8, cy + 6);
    TFT_DrawString(18, cy + 2, "WEIGHT", COLOR_GRAY, COLOR_BLACK, 1);
    sprintf(buf, "%.1f", g_sys.weight_g);
    TFT_DrawString(60, cy + 4, buf, COLOR_MAGENTA, COLOR_BLACK, 2);
    TFT_DrawString(104, cy + 6, "g", COLOR_MAGENTA, COLOR_BLACK, 1);
    if (g_sys.selected_item == 1) draw_frame(0, cy, TFT_WIDTH, CARD_H, COLOR_WHITE);

    cy = 25 + (CARD_H + CARD_GAP) * 2;
    draw_card_bg(cy, COLOR_CYAN);
    draw_icon_clock(8, cy + 6);
    TFT_DrawString(18, cy + 2, "TIME", COLOR_GRAY, COLOR_BLACK, 1);
    h = g_sys.params.dry_time_sec / 3600;
    m = (g_sys.params.dry_time_sec % 3600) / 60;
    s = g_sys.params.dry_time_sec % 60;
    sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
    TFT_DrawString(60, cy + 4, buf, COLOR_CYAN, COLOR_BLACK, 2);
    if (g_sys.run_state == STATE_DRYING) {
        h = g_sys.remaining_sec / 3600;
        m = (g_sys.remaining_sec % 3600) / 60;
        s = g_sys.remaining_sec % 60;
        sprintf(buf, "REM %02lu:%02lu:%02lu", h, m, s);
        TFT_DrawString(60, cy + 18, buf, COLOR_GREEN, COLOR_BLACK, 1);
    }
    if (g_sys.selected_item == 2) draw_frame(0, cy, TFT_WIDTH, CARD_H, COLOR_WHITE);

    cy = 25 + (CARD_H + CARD_GAP) * 3;
    draw_card_bg(cy, COLOR_RED);
    draw_icon_ptc(8, cy + 6);
    TFT_DrawString(18, cy + 2, "PTC", COLOR_GRAY, COLOR_BLACK, 1);
    sprintf(buf, "%.1f", g_sys.ptc_temp);
    TFT_DrawString(60, cy + 4, buf, COLOR_RED, COLOR_BLACK, 2);
    draw_degree(104, cy + 6, COLOR_RED, 1);
    if (g_sys.selected_item == 3) draw_frame(0, cy, TFT_WIDTH, CARD_H, COLOR_WHITE);

    if (g_sys.run_state != STATE_IDLE) {
        uint8_t pct = (g_sys.run_state == STATE_DRYING && g_sys.params.dry_time_sec > 0)
            ? (uint8_t)(100 - g_sys.remaining_sec * 100U / g_sys.params.dry_time_sec) : 0;
        TFT_FillRect(5, 218, 125, 1, COLOR_WHITE);
        TFT_FillRect(5, 227, 125, 1, COLOR_WHITE);
        TFT_FillRect(5, 218, 1, 10, COLOR_WHITE);
        TFT_FillRect(129, 218, 1, 10, COLOR_WHITE);
        TFT_FillRect(6, 219, (uint16_t)(123U * pct / 100U), 8, COLOR_GREEN);
    }
}

void UI_DrawWeightScreen(void)
{
    char buf[32];
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(20, 20, "WEIGHT TOOL", COLOR_WHITE, COLOR_BLACK, 2);
    sprintf(buf, "Current: %.1fg", g_sys.weight_g);
    TFT_DrawString(10, 60, buf, COLOR_MAGENTA, COLOR_BLACK, 2);
    draw_btn(90, "> Tare", COLOR_YELLOW, g_sys.selected_item == 0);
    draw_btn(114, "  Exit", COLOR_GRAY, g_sys.selected_item == 1);
    TFT_DrawString(10, 160, "Click: Select", COLOR_GRAY, COLOR_BLACK, 1);
}

void UI_DrawTempAdjust(void)
{
    char buf[16];
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(15, 20, "TEMP SETTINGS", COLOR_WHITE, COLOR_BLACK, 2);
    TFT_DrawString(10, 60, "> Set Temp", COLOR_GRAY, COLOR_BLACK, 1);
    sprintf(buf, "%d", g_sys.params.target_temp);
    TFT_DrawString(90, 60, buf, COLOR_ORANGE, COLOR_BLACK, 1);
    draw_degree(102, 60, COLOR_ORANGE, 1);
    TFT_DrawString(10, 90, "  PID Autotune", COLOR_GRAY, COLOR_BLACK, 1);
    draw_btn(160, "  Exit", COLOR_GRAY, g_sys.selected_item == 2);
    if (g_sys.selected_item == 0) draw_frame(8, 57, 120, 18, COLOR_WHITE);
    if (g_sys.selected_item == 1) draw_frame(8, 87, 120, 18, COLOR_WHITE);
    if (g_sys.selected_item == 2) draw_frame(5, 160, 125, BTN_H, COLOR_WHITE);
    TFT_DrawString(10, 130, "Rotate: Select", COLOR_GRAY, COLOR_BLACK, 1);
    TFT_DrawString(10, 145, "Click: Enter", COLOR_GRAY, COLOR_BLACK, 1);
}

void UI_DrawTempEdit(void)
{
    char buf[16];
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(5, 20, "SET TARGET TEMP", COLOR_WHITE, COLOR_BLACK, 2);
    sprintf(buf, "%d", g_sys.params.target_temp);
    TFT_DrawString(30, 70, buf, COLOR_ORANGE, COLOR_BLACK, 4);
    draw_degree(90, 72, COLOR_ORANGE, 2);
    draw_btn(160, "  Save & Exit", COLOR_GREEN, g_sys.selected_item == 0);
    draw_btn(184, "  Cancel", COLOR_GRAY, g_sys.selected_item == 1);
    TFT_DrawString(10, 140, "Rotate: Adj", COLOR_GRAY, COLOR_BLACK, 1);
}

void UI_DrawTempPid(void)
{
    char buf[32];
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(5, 10, "TEMP PID TUNE", COLOR_WHITE, COLOR_BLACK, 2);
    sprintf(buf, "Cal: %d", g_sys.params.target_temp);
    TFT_DrawString(10, 40, buf, COLOR_ORANGE, COLOR_BLACK, 1);
    draw_degree(76, 40, COLOR_ORANGE, 1);
    if (g_sys.temp_pid_running) {
        TFT_DrawString(10, 65, "Running...", COLOR_YELLOW, COLOR_BLACK, 2);
        TFT_FillRect(10, 95, 115, 1, COLOR_WHITE);
        TFT_FillRect(10, 104, 115, 1, COLOR_WHITE);
        TFT_FillRect(10, 95, 1, 10, COLOR_WHITE);
        TFT_FillRect(124, 95, 1, 10, COLOR_WHITE);
        uint8_t pct = g_sys.temp_pid_progress;
        TFT_FillRect(11, 96, (uint16_t)(113U * pct / 100U), 8, COLOR_GREEN);
        sprintf(buf, "%d%%", pct);
        TFT_DrawString(50, 110, buf, COLOR_WHITE, COLOR_BLACK, 1);
    } else {
        TFT_DrawString(10, 65, "Complete!", COLOR_GREEN, COLOR_BLACK, 2);
        sprintf(buf, "KP:%.2f KI:%.2f KD:%.2f", g_sys.params.pid_kp, g_sys.params.pid_ki, g_sys.params.pid_kd);
        TFT_DrawString(10, 95, buf, COLOR_CYAN, COLOR_BLACK, 1);
    }
    draw_btn(160, "  Exit", COLOR_GRAY, g_sys.selected_item == 0);
    if (g_sys.selected_item == 0) draw_frame(5, 160, 125, BTN_H, COLOR_WHITE);
}

void UI_DrawTimeAdjust(void)
{
    char buf[16];
    uint32_t h = g_sys.params.dry_time_sec / 3600;
    uint32_t m = (g_sys.params.dry_time_sec % 3600) / 60;
    uint32_t s = g_sys.params.dry_time_sec % 60;
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(25, 10, "SET TIME", COLOR_WHITE, COLOR_BLACK, 2);
    sprintf(buf, "%02lu", h);
    TFT_DrawString(12, 50, buf, COLOR_CYAN, COLOR_BLACK, 3);
    TFT_DrawString(48, 50, "h", COLOR_GRAY, COLOR_BLACK, 2);
    sprintf(buf, "%02lu", m);
    TFT_DrawString(60, 50, buf, COLOR_CYAN, COLOR_BLACK, 3);
    TFT_DrawString(96, 50, "m", COLOR_GRAY, COLOR_BLACK, 2);
    sprintf(buf, "%02lu", s);
    TFT_DrawString(108, 50, buf, COLOR_CYAN, COLOR_BLACK, 3);
    TFT_DrawString(120, 50, "s", COLOR_GRAY, COLOR_BLACK, 1);
    TFT_DrawString(48, 53, ":", COLOR_WHITE, COLOR_BLACK, 2);
    TFT_DrawString(96, 53, ":", COLOR_WHITE, COLOR_BLACK, 2);
    if (g_sys.time_cursor == TIME_FIELD_HOUR) draw_frame(10, 48, 42, 28, COLOR_GRAY);
    if (g_sys.time_cursor == TIME_FIELD_MIN) draw_frame(58, 48, 42, 28, COLOR_GRAY);
    if (g_sys.time_cursor == TIME_FIELD_SEC) draw_frame(106, 48, 20, 28, COLOR_GRAY);
    draw_btn(160, "  Edit", COLOR_YELLOW, g_sys.selected_item == 0);
    draw_btn(184, "  Exit", COLOR_GRAY, g_sys.selected_item == 1);
    TFT_DrawString(10, 140, "Rotate: Cursor", COLOR_GRAY, COLOR_BLACK, 1);
}

void UI_DrawTimeEdit(void)
{
    char buf[16];
    uint32_t h = g_sys.params.dry_time_sec / 3600;
    uint32_t m = (g_sys.params.dry_time_sec % 3600) / 60;
    uint32_t s = g_sys.params.dry_time_sec % 60;
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(5, 10, "EDIT TIME", COLOR_YELLOW, COLOR_BLACK, 2);
    sprintf(buf, "%02lu", h);
    TFT_DrawString(12, 50, buf, COLOR_WHITE, COLOR_BLACK, 3);
    TFT_DrawString(48, 50, "h", COLOR_GRAY, COLOR_BLACK, 2);
    sprintf(buf, "%02lu", m);
    TFT_DrawString(60, 50, buf, COLOR_WHITE, COLOR_BLACK, 3);
    TFT_DrawString(96, 50, "m", COLOR_GRAY, COLOR_BLACK, 2);
    sprintf(buf, "%02lu", s);
    TFT_DrawString(108, 50, buf, COLOR_WHITE, COLOR_BLACK, 3);
    TFT_DrawString(120, 50, "s", COLOR_GRAY, COLOR_BLACK, 1);
    TFT_DrawString(48, 53, ":", COLOR_WHITE, COLOR_BLACK, 2);
    TFT_DrawString(96, 53, ":", COLOR_WHITE, COLOR_BLACK, 2);
    if (g_sys.time_cursor == TIME_FIELD_HOUR) draw_frame(10, 48, 42, 28, COLOR_YELLOW);
    if (g_sys.time_cursor == TIME_FIELD_MIN) draw_frame(58, 48, 42, 28, COLOR_YELLOW);
    if (g_sys.time_cursor == TIME_FIELD_SEC) draw_frame(106, 48, 20, 28, COLOR_YELLOW);
    draw_btn(160, "  Save & Exit", COLOR_GREEN, g_sys.selected_item == 0);
    draw_btn(184, "  Cancel", COLOR_GRAY, g_sys.selected_item == 1);
    TFT_DrawString(10, 140, "Rotate: Adj", COLOR_GRAY, COLOR_BLACK, 1);
}

void UI_DrawPtcAdjust(void)
{
    char buf[16];
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(15, 20, "PTC SETTINGS", COLOR_WHITE, COLOR_BLACK, 2);
    TFT_DrawString(10, 60, "> Max Temp", COLOR_GRAY, COLOR_BLACK, 1);
    sprintf(buf, "%d", g_sys.params.ptc_max_temp);
    TFT_DrawString(90, 60, buf, COLOR_RED, COLOR_BLACK, 1);
    draw_degree(102, 60, COLOR_RED, 1);
    TFT_DrawString(10, 85, "  Cooling Temp", COLOR_GRAY, COLOR_BLACK, 1);
    sprintf(buf, "%d", g_sys.params.ptc_cooling_temp);
    TFT_DrawString(90, 85, buf, COLOR_CYAN, COLOR_BLACK, 1);
    draw_degree(102, 85, COLOR_CYAN, 1);
    TFT_DrawString(10, 110, "  PID Autotune", COLOR_GRAY, COLOR_BLACK, 1);
    draw_btn(160, "  Exit", COLOR_GRAY, 0);
    if (g_sys.selected_item == 0) draw_frame(8, 57, 120, 18, COLOR_WHITE);
    if (g_sys.selected_item == 1) draw_frame(8, 82, 120, 18, COLOR_WHITE);
    if (g_sys.selected_item == 2) draw_frame(8, 107, 120, 18, COLOR_WHITE);
    TFT_DrawString(10, 140, "Rotate: Select", COLOR_GRAY, COLOR_BLACK, 1);
    TFT_DrawString(10, 155, "Click: Enter", COLOR_GRAY, COLOR_BLACK, 1);
}

void UI_DrawPtcEdit(void)
{
    char buf[16];
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(5, 20, "MAX PTC TEMP", COLOR_WHITE, COLOR_BLACK, 2);
    sprintf(buf, "%d", g_sys.params.ptc_max_temp);
    TFT_DrawString(30, 70, buf, COLOR_RED, COLOR_BLACK, 4);
    draw_degree(90, 72, COLOR_RED, 2);
    draw_btn(160, "  Save & Exit", COLOR_GREEN, g_sys.selected_item == 0);
    draw_btn(184, "  Cancel", COLOR_GRAY, g_sys.selected_item == 1);
    TFT_DrawString(10, 140, "Rotate: Adj", COLOR_GRAY, COLOR_BLACK, 1);
}

void UI_DrawPtcCoolingEdit(void)
{
    char buf[16];
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(5, 20, "COOLING TEMP", COLOR_WHITE, COLOR_BLACK, 2);
    sprintf(buf, "%d", g_sys.params.ptc_cooling_temp);
    TFT_DrawString(30, 70, buf, COLOR_CYAN, COLOR_BLACK, 4);
    draw_degree(90, 72, COLOR_CYAN, 2);
    draw_btn(160, "  Save & Exit", COLOR_GREEN, g_sys.selected_item == 0);
    draw_btn(184, "  Cancel", COLOR_GRAY, g_sys.selected_item == 1);
    TFT_DrawString(10, 140, "Rotate: Adj", COLOR_GRAY, COLOR_BLACK, 1);
}

void UI_DrawPidAutotune(void)
{
    char buf[32];
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(5, 10, "PID AUTOTUNE", COLOR_WHITE, COLOR_BLACK, 2);
    TFT_DrawString(10, 40, "Status:", COLOR_GRAY, COLOR_BLACK, 1);
    if (g_sys.pid_autotune_running) {
        TFT_DrawString(10, 60, "Running...", COLOR_YELLOW, COLOR_BLACK, 2);
        TFT_FillRect(10, 90, 115, 1, COLOR_WHITE);
        TFT_FillRect(10, 99, 115, 1, COLOR_WHITE);
        TFT_FillRect(10, 90, 1, 10, COLOR_WHITE);
        TFT_FillRect(124, 90, 1, 10, COLOR_WHITE);
        uint8_t pct = g_sys.pid_autotune_progress;
        TFT_FillRect(11, 91, (uint16_t)(113U * pct / 100U), 8, COLOR_GREEN);
        sprintf(buf, "%d%%", pct);
        TFT_DrawString(50, 105, buf, COLOR_WHITE, COLOR_BLACK, 1);
    } else {
        TFT_DrawString(10, 60, "Complete!", COLOR_GREEN, COLOR_BLACK, 2);
        sprintf(buf, "KP:%.2f KI:%.2f KD:%.2f", g_sys.params.pid_kp, g_sys.params.pid_ki, g_sys.params.pid_kd);
        TFT_DrawString(10, 90, buf, COLOR_CYAN, COLOR_BLACK, 1);
    }
    draw_btn(160, "  Exit", COLOR_GRAY, g_sys.selected_item == 0);
    if (g_sys.selected_item == 0) draw_frame(5, 160, 125, BTN_H, COLOR_WHITE);
}

void UI_DrawMenu(void)
{
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(40, 10, "MENU", COLOR_WHITE, COLOR_BLACK, 2);

    draw_icon_start();
    if (g_sys.drying_active) TFT_DrawString(40, 55, "Stop", COLOR_GREEN, COLOR_BLACK, 2);
    else TFT_DrawString(40, 55, "Start", COLOR_GREEN, COLOR_BLACK, 2);
    if (g_sys.selected_item == 0) draw_frame(5, 50, 125, 25, COLOR_WHITE);

    draw_icon_menu();
    TFT_DrawString(40, 85, "WiFi", COLOR_WHITE, COLOR_BLACK, 2);
    if (g_sys.selected_item == 1) draw_frame(5, 80, 125, 25, COLOR_WHITE);

    draw_icon_motor();
    TFT_DrawString(40, 115, "Motor", COLOR_WHITE, COLOR_BLACK, 2);
    if (g_sys.selected_item == 2) draw_frame(5, 110, 125, 25, COLOR_WHITE);

    draw_icon_info();
    TFT_DrawString(40, 145, "About", COLOR_WHITE, COLOR_BLACK, 2);
    if (g_sys.selected_item == 3) draw_frame(5, 140, 125, 25, COLOR_WHITE);

    draw_icon_gear();
    TFT_DrawString(40, 175, "Reset", COLOR_WHITE, COLOR_BLACK, 2);
    if (g_sys.selected_item == 4) draw_frame(5, 170, 125, 25, COLOR_WHITE);

    draw_btn(210, "  Exit", COLOR_GRAY, g_sys.selected_item == 5);
    if (g_sys.selected_item == 5) draw_frame(5, 210, 125, BTN_H, COLOR_WHITE);
}

void UI_DrawMotorAdjust(void)
{
    char buf[32];
    uint8_t is_tmc = (g_sys.params.motor_driver == MOTOR_DRIVER_TMC2209);
    uint8_t count = is_tmc ? 9 : 6;

    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(15, 5, "MOTOR SETTINGS", COLOR_WHITE, COLOR_BLACK, 2);

    sprintf(buf, "> Linkage: %s", g_sys.params.motor_enabled ? "ON" : "OFF");
    TFT_DrawString(5, 30, buf, g_sys.selected_item == 0 ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 1);

    sprintf(buf, "  Dir: %s", g_sys.params.motor_direction ? "CCW" : "CW");
    TFT_DrawString(5, 45, buf, g_sys.selected_item == 1 ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 1);

    sprintf(buf, "  Speed: %d rpm", g_sys.params.motor_speed);
    TFT_DrawString(5, 60, buf, g_sys.selected_item == 2 ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 1);

    sprintf(buf, "  Oscillate: %s", g_sys.params.motor_oscillate ? "ON" : "OFF");
    TFT_DrawString(5, 75, buf, g_sys.selected_item == 3 ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 1);

    sprintf(buf, "  Angle: %d deg", g_sys.params.motor_oscillate_angle);
    TFT_DrawString(5, 90, buf, g_sys.selected_item == 4 ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 1);

    sprintf(buf, "  Driver: %s", is_tmc ? "TMC2209" : "A4988");
    TFT_DrawString(5, 105, buf, g_sys.selected_item == 5 ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 1);

    if (is_tmc) {
        sprintf(buf, "  Current: %.1fA", (float)g_sys.params.motor_current / 10.0f);
        TFT_DrawString(5, 120, buf, g_sys.selected_item == 6 ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 1);
        sprintf(buf, "  Stealth: %s", g_sys.params.motor_stealthchop ? "ON" : "OFF");
        TFT_DrawString(5, 135, buf, g_sys.selected_item == 7 ? COLOR_YELLOW : COLOR_GRAY, COLOR_BLACK, 1);
    }

    draw_btn(is_tmc ? 210 : 195, "  Exit", COLOR_GRAY, g_sys.selected_item == count - 1);
    TFT_DrawString(5, is_tmc ? 195 : 180, "Click: Enter", COLOR_GRAY, COLOR_BLACK, 1);
}

void UI_DrawMotorEdit(void)
{
    char buf[32];
    int idx = g_sys.submenu_active;
    uint8_t is_tmc = (g_sys.params.motor_driver == MOTOR_DRIVER_TMC2209);

    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(5, 10, "EDIT PARAM", COLOR_WHITE, COLOR_BLACK, 2);

    switch (idx) {
    case 0: sprintf(buf, "Linkage: %s", g_sys.params.motor_enabled ? "ON" : "OFF"); break;
    case 1: sprintf(buf, "Direction: %s", g_sys.params.motor_direction ? "CCW" : "CW"); break;
    case 2: sprintf(buf, "Speed: %d rpm", g_sys.params.motor_speed); break;
    case 3: sprintf(buf, "Oscillate: %s", g_sys.params.motor_oscillate ? "ON" : "OFF"); break;
    case 4: sprintf(buf, "Angle: %d deg", g_sys.params.motor_oscillate_angle); break;
    case 5: sprintf(buf, "Driver: %s", is_tmc ? "TMC2209" : "A4988"); break;
    case 6: sprintf(buf, "Current: %.1fA", (float)g_sys.params.motor_current / 10.0f); break;
    case 7: sprintf(buf, "Stealth: %s", g_sys.params.motor_stealthchop ? "ON" : "OFF"); break;
    default: break;
    }
    TFT_DrawString(10, 60, buf, COLOR_YELLOW, COLOR_BLACK, 2);
    TFT_DrawString(10, 100, "Rotate: Adj", COLOR_GRAY, COLOR_BLACK, 1);
    draw_btn(160, "  Save & Exit", COLOR_GREEN, g_sys.selected_item == 0);
    draw_btn(184, "  Cancel", COLOR_GRAY, g_sys.selected_item == 1);
}

void UI_DrawAbout(void)
{
    char buf[64];
    uint32_t id = g_sys.device_id;
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(5, 10, "About", COLOR_WHITE, COLOR_BLACK, 2);
    TFT_DrawString(5, 40, "DRYER V1.0", COLOR_YELLOW, COLOR_BLACK, 3);
    sprintf(buf, "Ver: %s", APP_VERSION);
    TFT_DrawString(5, 75, buf, COLOR_CYAN, COLOR_BLACK, 1);
    sprintf(buf, "Dev: %s", DEV_NAME);
    TFT_DrawString(5, 95, buf, COLOR_GREEN, COLOR_BLACK, 1);
    sprintf(buf, "Shell: %s", DEV_SHELL);
    TFT_DrawString(5, 115, buf, COLOR_GREEN, COLOR_BLACK, 1);
    sprintf(buf, "SN: %08lX", id);
    TFT_DrawString(5, 135, buf, COLOR_ORANGE, COLOR_BLACK, 1);
    draw_btn(184, "  Exit", COLOR_GRAY, g_sys.selected_item == 0);
    if (g_sys.selected_item == 0) draw_frame(5, 184, 125, BTN_H, COLOR_WHITE);
}

void UI_DrawWiFiScreen(void)
{
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(35, 10, "WIFI", COLOR_WHITE, COLOR_BLACK, 2);
    if (!g_sys.wifi_enabled) {
        TFT_DrawString(10, 60, "WiFi: OFF", COLOR_GRAY, COLOR_BLACK, 2);
        TFT_DrawString(10, 100, "Click to ON", COLOR_YELLOW, COLOR_BLACK, 1);
    } else {
        TFT_DrawString(10, 60, "WiFi: ON", COLOR_GREEN, COLOR_BLACK, 2);
        if (g_sys.wifi_ap_mode) {
            TFT_DrawString(10, 90, "Mode: AP", COLOR_CYAN, COLOR_BLACK, 1);
            TFT_DrawString(10, 110, WIFI_AP_SSID, COLOR_WHITE, COLOR_BLACK, 1);
            TFT_DrawString(10, 130, "IP:", COLOR_GRAY, COLOR_BLACK, 1);
            TFT_DrawString(10, 150, g_sys.wifi_ip, COLOR_YELLOW, COLOR_BLACK, 1);
        } else {
            TFT_DrawString(10, 90, "Mode: STA", COLOR_CYAN, COLOR_BLACK, 1);
            TFT_DrawString(10, 110, g_sys.wifi_ip, COLOR_WHITE, COLOR_BLACK, 1);
        }
        TFT_DrawString(10, 180, "Connect to AP", COLOR_GRAY, COLOR_BLACK, 1);
        TFT_DrawString(10, 200, "Open browser", COLOR_GRAY, COLOR_BLACK, 1);
    }
    draw_btn(184, "  Exit", COLOR_GRAY, g_sys.selected_item == 0);
    if (g_sys.selected_item == 0) draw_frame(5, 184, 125, BTN_H, COLOR_WHITE);
}

void UI_DrawOTAScreen(void)
{
    TFT_FillScreen(COLOR_BLACK);
    TFT_DrawString(25, 10, "FIRMWARE", COLOR_WHITE, COLOR_BLACK, 2);
    if (g_sys.ota_downloading) {
        TFT_DrawString(10, 60, "Downloading...", COLOR_YELLOW, COLOR_BLACK, 1);
        TFT_FillRect(10, 90, 115, 1, COLOR_WHITE);
        TFT_FillRect(10, 99, 115, 1, COLOR_WHITE);
        TFT_FillRect(10, 90, 1, 10, COLOR_WHITE);
        TFT_FillRect(124, 90, 1, 10, COLOR_WHITE);
        TFT_FillRect(11, 91, (113 * g_sys.ota_progress) / 100, 8, COLOR_GREEN);
        char buf[32];
        sprintf(buf, "%d%% (%lu/%lu)", g_sys.ota_progress, g_sys.ota_received_size, g_sys.ota_total_size);
        TFT_DrawString(10, 115, buf, COLOR_GRAY, COLOR_BLACK, 1);
    } else if (g_sys.ota_download_done) {
        TFT_DrawString(10, 60, "Download OK!", COLOR_GREEN, COLOR_BLACK, 2);
        TFT_DrawString(10, 100, "Click to Update", COLOR_YELLOW, COLOR_BLACK, 1);
        TFT_DrawString(10, 120, "Device will restart", COLOR_GRAY, COLOR_BLACK, 1);
    } else {
        TFT_DrawString(10, 60, "Drag firmware", COLOR_GRAY, COLOR_BLACK, 1);
        TFT_DrawString(10, 80, "to web page", COLOR_GRAY, COLOR_BLACK, 1);
        TFT_DrawString(10, 110, "and click Upload", COLOR_GRAY, COLOR_BLACK, 1);
    }
    draw_btn(184, "  Exit", COLOR_GRAY, g_sys.selected_item == 0);
    if (g_sys.selected_item == 0) draw_frame(5, 184, 125, BTN_H, COLOR_WHITE);
}

void UI_DrawSafetyAlert(void)
{
    TFT_FillScreen(COLOR_RED);
    TFT_DrawString(5, 30, "SAFETY ALERT", COLOR_WHITE, COLOR_RED, 2);

    if (g_sys.safety_state == SAFETY_BOX_BROKEN) {
        TFT_DrawString(5, 70, "BOX BROKEN DETECTED", COLOR_WHITE, COLOR_RED, 2);
        TFT_DrawString(5, 100, "DRYING STOPPED", COLOR_WHITE, COLOR_RED, 2);
    } else if (g_sys.safety_state == SAFETY_LID_OPEN) {
        TFT_DrawString(5, 70, "LID OPEN DETECTED", COLOR_WHITE, COLOR_RED, 2);
        TFT_DrawString(5, 100, "DRYING STOPPED", COLOR_WHITE, COLOR_RED, 2);
    }

    draw_btn(160, "  Confirm", COLOR_GREEN, 1);
    draw_frame(5, 160, 125, BTN_H, COLOR_WHITE);
}

static Screen_t last_screen = (Screen_t)255;

void UI_Update(void)
{
    if (g_sys.current_screen != last_screen) {
        last_screen = g_sys.current_screen;
        switch (g_sys.current_screen) {
            case SCREEN_MAIN:          UI_DrawMainScreen(); break;
            case SCREEN_WEIGHT:        UI_DrawWeightScreen(); break;
            case SCREEN_TEMP_ADJUST:   UI_DrawTempAdjust(); break;
            case SCREEN_TEMP_EDIT:     UI_DrawTempEdit(); break;
            case SCREEN_TEMP_PID:      UI_DrawTempPid(); break;
            case SCREEN_TIME_ADJUST:   UI_DrawTimeAdjust(); break;
            case SCREEN_TIME_EDIT:     UI_DrawTimeEdit(); break;
            case SCREEN_PTC_ADJUST:    UI_DrawPtcAdjust(); break;
            case SCREEN_PTC_EDIT:      UI_DrawPtcEdit(); break;
            case SCREEN_PTC_COOLING_EDIT: UI_DrawPtcCoolingEdit(); break;
            case SCREEN_PID_AUTOTUNE:  UI_DrawPidAutotune(); break;
            case SCREEN_MENU:          UI_DrawMenu(); break;
            case SCREEN_MOTOR_ADJUST:  UI_DrawMotorAdjust(); break;
            case SCREEN_MOTOR_EDIT:    UI_DrawMotorEdit(); break;
            case SCREEN_ABOUT:         UI_DrawAbout(); break;
            case SCREEN_WIFI:          UI_DrawWiFiScreen(); break;
            case SCREEN_OTA:           UI_DrawOTAScreen(); break;
            case SCREEN_SAFETY_ALERT:  UI_DrawSafetyAlert(); break;
        }
    } else {
        switch (g_sys.current_screen) {
            case SCREEN_MAIN:
                UI_UpdateMainDynamic();
                break;
            case SCREEN_TEMP_PID:
                UI_DrawTempPid();
                break;
            case SCREEN_PID_AUTOTUNE:
                UI_DrawPidAutotune();
                break;
            case SCREEN_OTA:
                if (g_sys.ota_downloading || g_sys.ota_download_done) UI_DrawOTAScreen();
                break;
            default:
                break;
        }
    }
}

void UI_UpdateMainDynamic(void)
{
    char buf[32];
    uint32_t h, m, s;

    sprintf(buf, "%.1f", g_sys.current_temp);
    TFT_DrawString(60, 25, buf, COLOR_WHITE, COLOR_BLACK, 2);
    sprintf(buf, "%.1f", g_sys.weight_g);
    TFT_DrawString(60, 59, buf, COLOR_MAGENTA, COLOR_BLACK, 2);
    h = g_sys.params.dry_time_sec / 3600;
    m = (g_sys.params.dry_time_sec % 3600) / 60;
    s = g_sys.params.dry_time_sec % 60;
    sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
    TFT_DrawString(60, 93, buf, COLOR_CYAN, COLOR_BLACK, 2);
    if (g_sys.run_state == STATE_DRYING) {
        h = g_sys.remaining_sec / 3600;
        m = (g_sys.remaining_sec % 3600) / 60;
        s = g_sys.remaining_sec % 60;
        sprintf(buf, "REM %02lu:%02lu:%02lu", h, m, s);
        TFT_DrawString(60, 107, buf, COLOR_GREEN, COLOR_BLACK, 1);
    }
    sprintf(buf, "%.1f", g_sys.ptc_temp);
    TFT_DrawString(60, 127, buf, COLOR_RED, COLOR_BLACK, 2);
    if (g_sys.run_state != STATE_IDLE) {
        uint8_t pct = (g_sys.run_state == STATE_DRYING && g_sys.params.dry_time_sec > 0)
            ? (uint8_t)(100 - g_sys.remaining_sec * 100U / g_sys.params.dry_time_sec) : 0;
        TFT_FillRect(6, 219, 123, 8, COLOR_BLACK);
        TFT_FillRect(6, 219, (uint16_t)(123U * pct / 100U), 8, COLOR_GREEN);
    }
}

static void Delay_ms(uint16_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 7200; i++);
}
#endif /* BOOTLOADER_BUILD */

