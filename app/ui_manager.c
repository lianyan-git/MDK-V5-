#ifndef BOOTLOADER_BUILD
#include "ui_manager.h"
#include "system_config.h"
#include "bsp_tft_st7789.h"
#include "stm32f10x.h"
#include <stdio.h>
#include <string.h>

#define CARD_H         30
#define CARD_GAP       4
#define ACCENT_W       3
#define SEL_FRAME_W    2
#define BTN_Y          218
#define BTN_H          18
#define BTN_GAP        4

/* ── 浅色主题（清新彩色风格） ── */
#define UI_BG           TFT_COLOR(0xE8, 0xEA, 0xED)   /* 浅灰背景 */
#define UI_CARD         TFT_COLOR(0xFF, 0xFF, 0xFF)   /* 白卡片 */
#define UI_CARD_HI      TFT_COLOR(0xF0, 0xF4, 0xFF)   /* 选中高亮底 */
#define UI_CARD_EDGE    TFT_COLOR(0xD0, 0xD0, 0xD0)   /* 卡片描边 */
#define UI_TEXT         TFT_COLOR(0x22, 0x22, 0x22)   /* 深色文字 */
#define UI_TEXT_DIM     TFT_COLOR(0x88, 0x88, 0x88)   /* 灰色文字 */
#define UI_TITLE_BG     TFT_COLOR(0xE0, 0xE0, 0xE0)   /* 标题栏底 */
#define UI_ACCENT       TFT_COLOR(0x21, 0x96, 0xF3)   /* 亮蓝 */
#define UI_ACCENT2      TFT_COLOR(0xFF, 0x52, 0x52)   /* 珊瑚红 */
#define UI_OK           TFT_COLOR(0x4C, 0xAF, 0x50)   /* 绿 */
#define UI_WARN         TFT_COLOR(0xFF, 0xC1, 0x07)   /* 琥珀黄 */
#define UI_CYAN         TFT_COLOR(0x00, 0xBC, 0xD4)   /* 青 */
#define UI_PURPLE       TFT_COLOR(0x9C, 0x27, 0xB0)   /* 紫 */

/* 各卡片背景色（浅色淡彩） */
#define CARD_BG_TEMP    TFT_COLOR(0xFF, 0xF3, 0xE0)   /* 暖橙 */
#define CARD_BG_HUMI    TFT_COLOR(0xE1, 0xF5, 0xFE)   /* 冷青 */
#define CARD_BG_WEIGHT  TFT_COLOR(0xF3, 0xE5, 0xF5)   /* 紫 */
#define CARD_BG_PTC     TFT_COLOR(0xFF, 0xEB, 0xEE)   /* 红 */
#define CARD_BG_TIME    TFT_COLOR(0xE8, 0xF5, 0xE9)   /* 浅绿（区别于湿度蓝） */

#define TFT_COLOR(r,g,b)  ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))

static void Delay_ms(uint16_t ms);

/* 普通边框（不保留圆角，用于按钮等） */
static void draw_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    TFT_FillRect(x, y, w, SEL_FRAME_W, color);
    TFT_FillRect(x, y + h - SEL_FRAME_W, w, SEL_FRAME_W, color);
    TFT_FillRect(x, y, SEL_FRAME_W, h, color);
    TFT_FillRect(x + w - SEL_FRAME_W, y, SEL_FRAME_W, h, color);
}

/* 圆角边框：直线段 + 四角圆弧描边（完整跟随圆角轮廓） */
static void draw_frame_rounded(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                               uint16_t color, uint8_t r)
{
    int16_t dx, dy;
    int16_t r_inner, r_outer, r_inner2, r_outer2, dist2;

    if (r == 0U) {
        draw_frame(x, y, w, h, color);
        return;
    }
    /* 直线段 */
    TFT_FillRect(x + r, y, w - 2U * r, SEL_FRAME_W, color);
    TFT_FillRect(x + r, y + h - SEL_FRAME_W, w - 2U * r, SEL_FRAME_W, color);
    TFT_FillRect(x, y + r, SEL_FRAME_W, h - 2U * r, color);
    TFT_FillRect(x + w - SEL_FRAME_W, y + r, SEL_FRAME_W, h - 2U * r, color);

    /* 四角圆弧描边：像素落在 [r-SEL_FRAME_W, r] 圆环带内则画 */
    r_outer = (int16_t)r;
    r_inner = (int16_t)(r - SEL_FRAME_W);
    r_outer2 = r_outer * r_outer;
    r_inner2 = r_inner * r_inner;
    for (dy = 0; dy < r; dy++) {
        for (dx = 0; dx < r; dx++) {
            dist2 = (int16_t)((r - 1 - dx) * (r - 1 - dx) + (r - 1 - dy) * (r - 1 - dy));
            if (dist2 <= r_outer2 && dist2 >= r_inner2) {
                TFT_DrawPixel((uint16_t)(x + dx), (uint16_t)(y + dy), color);                    /* 左上 */
                TFT_DrawPixel((uint16_t)(x + w - 1 - dx), (uint16_t)(y + dy), color);            /* 右上 */
                TFT_DrawPixel((uint16_t)(x + dx), (uint16_t)(y + h - 1 - dy), color);            /* 左下 */
                TFT_DrawPixel((uint16_t)(x + w - 1 - dx), (uint16_t)(y + h - 1 - dy), color);    /* 右下 */
            }
        }
    }
}

/* 真圆角矩形：四角用圆弧像素，中间用 FillRect 高效填充 */
static void fill_round_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            uint16_t color, uint8_t radius)
{
    int16_t dy, dx;
    if (radius == 0U) { TFT_FillRect(x, y, w, h, color); return; }
    if (radius > w / 2U) radius = (uint8_t)(w / 2U);
    if (radius > h / 2U) radius = (uint8_t)(h / 2U);
    TFT_FillRect(x + radius, y, w - 2U * radius, h, color);
    TFT_FillRect(x, y + radius, radius, h - 2U * radius, color);
    TFT_FillRect(x + w - radius, y + radius, radius, h - 2U * radius, color);
    for (dy = 0; dy < radius; dy++) {
        int16_t dy_from_center = (int16_t)(radius - 1 - dy);
        uint16_t arc_w = 0;
        for (dx = 0; dx < radius; dx++) {
            int16_t dx_from_center = (int16_t)(radius - 1 - dx);
            if (dx_from_center * dx_from_center +
                dy_from_center * dy_from_center <= (int16_t)((radius - 1) * (radius - 1))) {
                arc_w = (uint16_t)(radius - dx);
                break;
            }
        }
        if (arc_w > 0U) {
            TFT_FillRect(x + radius - arc_w, y + dy, arc_w, 1, color);
            TFT_FillRect(x + w - radius, y + dy, arc_w, 1, color);
            TFT_FillRect(x + radius - arc_w, y + h - 1 - dy, arc_w, 1, color);
            TFT_FillRect(x + w - radius, y + h - 1 - dy, arc_w, 1, color);
        }
    }
}

/* ── 颜色插值工具（RGB565） ── */
static uint16_t lerp_color(uint16_t c1, uint16_t c2, uint8_t t)
{
    if (t >= 255U) return c2;
    if (t == 0U) return c1;
    uint8_t r1 = (uint8_t)((c1 >> 11) & 0x1F), g1 = (uint8_t)((c1 >> 5) & 0x3F), b1 = (uint8_t)(c1 & 0x1F);
    uint8_t r2 = (uint8_t)((c2 >> 11) & 0x1F), g2 = (uint8_t)((c2 >> 5) & 0x3F), b2 = (uint8_t)(c2 & 0x1F);
    uint8_t r = (uint8_t)(r1 + ((uint16_t)(r2 - r1) * t / 255U));
    uint8_t g = (uint8_t)(g1 + ((uint16_t)(g2 - g1) * t / 255U));
    uint8_t b = (uint8_t)(b1 + ((uint16_t)(b2 - b1) * t / 255U));
    return (uint16_t)((r << 11) | (g << 5) | b);
}

/* 选中卡顶部色条：随时间在 ACCENT 与高亮之间呼吸（平滑动画） */
static uint16_t pulse_color(void)
{
    uint32_t t = SystemTime_Millis() % 1600U;   /* 1.6s 周期 */
    uint8_t phase;
    if (t < 800U) phase = (uint8_t)(t * 255U / 800U);
    else phase = (uint8_t)((1600U - t) * 255U / 800U);
    return lerp_color(UI_ACCENT, UI_TEXT, phase);
}

/* 在卡片顶部画高亮呼吸色条（仅选中卡，小区域局部刷新不闪烁） */
static void draw_card_pulse(uint16_t x, uint16_t y, uint16_t w, uint8_t selected)
{
    if (!selected) return;
    TFT_FillRect(x, y, w, 2, pulse_color());
}

static void draw_btn(uint16_t y, const char *label, uint16_t color, uint8_t selected)
{
    TFT_FillRect(5, y, 125, BTN_H, UI_CARD);
    TFT_DrawString(10, y + 3, label, color, COLOR_DARKGRAY, 1);
    if (selected) draw_frame(5, y, 125, BTN_H, COLOR_WHITE);
}

static const uint16_t icon_temp_bmp[900] = {
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xA0E4, 0x90A3, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xB32C, 0xF81F, 0xF81F, 0xF81F,
    0xE617, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xA0A3, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xA8E4, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xEE17, 0x98A2,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xA0C3, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x98C3, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xFEBA, 0x9924, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0x98A3, 0xF81F, 0xF163, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0x98A3, 0xF81F, 0xF81F, 0x9882, 0xF81F, 0xF81F, 0xE5B6, 0x98C2,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x9082, 0xF81F,
    0xF81F, 0x9061, 0xF81F, 0xF81F, 0xFEDB, 0xAA8A, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x90A3, 0xF81F, 0xF81F, 0xA0C3,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0x9882, 0xF81F, 0xF81F, 0x9882, 0xF81F, 0xF81F,
    0xF679, 0xCA2A, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xA0E4, 0xF81F, 0xF81F, 0xA0C3, 0xF81F, 0xF81F, 0xFE58, 0x98A3,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x9082, 0xF81F,
    0xF81F, 0x90A2, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x98A3, 0xF81F, 0xE964, 0xA0C2,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0x9882, 0xF81F, 0xE965, 0xA0C2, 0xF81F, 0xF81F,
    0xE575, 0x98A2, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0x90A2, 0xF81F, 0xE964, 0x98C2, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xE964, 0x98C2, 0xF81F, 0x9082, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xA8E3, 0xF81F, 0xF164, 0xE964, 0xE164,
    0x9061, 0xF81F, 0x9103, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0x98A2, 0xF81F, 0xE965, 0xF81F, 0xDB2B, 0xE943, 0xD964, 0x8861,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x98A2, 0xF81F,
    0xE964, 0xE944, 0xE944, 0xE985, 0xD124, 0x90A2, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x98C3, 0xF81F, 0xE184, 0xE944,
    0xE964, 0xF164, 0x9082, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xA0C4, 0xF81F, 0xA0C2, 0x98A2, 0x98C2,
    0xA0C3, 0xF81F, 0x88A2, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0x9883, 0xF699, 0xF81F, 0xF81F, 0x8861, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xAA69, 0x9081, 0x9082, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F,
};

static void draw_icon_temp(uint16_t x, uint16_t y, uint16_t bg)
{
    TFT_DrawBitmap(x - 8U, y - 5U, 30, 30, icon_temp_bmp, 0xF81F, bg);
}

/* 重量哑铃图标（形象：配重 + 握杆，约 14x14） */
static void draw_icon_weight(uint16_t x, uint16_t y)
{
    uint16_t c = UI_PURPLE;
    TFT_FillRect(x, y + 5, 3, 7, c);
    TFT_FillRect(x + 11, y + 5, 3, 7, c);
    TFT_FillRect(x + 1, y + 4, 1, 9, c);
    TFT_FillRect(x + 12, y + 4, 1, 9, c);
    TFT_FillRect(x + 3, y + 7, 8, 2, c);
}

/* 湿度水滴图标（30x30 RGB565 位图，透明色 0xF81F） */
static const uint16_t icon_humi_bmp[900] = {

    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0x00E9, 0x0108, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x08E8,
    0x561C, 0x55FC, 0x1147, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x5D7A, 0x561D, 0x561C,
    0x657A, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0x00E8, 0x55FC, 0x561C, 0x55FC, 0x561C, 0x0109,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x00E7,
    0x4DDC, 0x55FC, 0x561C, 0x561C, 0x561C, 0x561C, 0x0108, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x55DC, 0x55FC, 0xA6DD,
    0x55FC, 0x561C, 0x4DFC, 0x55FC, 0x2BF8, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0x00E8, 0x55FC, 0x4DFC, 0x55FC, 0x561C, 0x55FC,
    0x55FC, 0x561C, 0x2C19, 0x0109, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0x4DDC, 0x55FC, 0xFFDE, 0x4DFC, 0x4DFC, 0x561C, 0x55FC, 0x561C,
    0x55FC, 0x2BF8, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x00C9, 0x55FD, 0x4DFC,
    0x55FC, 0x55FC, 0x55FC, 0x55FC, 0x55FC, 0x55FC, 0x4DFC, 0x2BF8,
    0x00E9, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0x0108, 0x55FC, 0x561C, 0x55DC, 0x4DFC, 0x55FC,
    0x55FC, 0x55FC, 0x55FC, 0x561D, 0x561C, 0x23D7, 0x2BF8, 0x0109,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0x0109, 0x55FC, 0xCFBF, 0x65DB, 0x55FC, 0x4DFC, 0x55FC, 0x55FC,
    0x55FC, 0x55FC, 0x55FC, 0x2437, 0x2BF9, 0x00C8, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x4DFC, 0x55FC,
    0xFFFF, 0x55FC, 0x55FC, 0x561C, 0x55FC, 0x561C, 0x4DFC, 0x55FC,
    0x55FC, 0x55FC, 0x2BF9, 0x2BF9, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x55FC, 0x55FC, 0xFFFF, 0x561D,
    0x55FC, 0x561C, 0x561C, 0x55FC, 0x4DFC, 0x55FC, 0x55FC, 0x451A,
    0x2BF9, 0x2C19, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0x561C, 0x55FC, 0xFFFE, 0x561C, 0x4DFC, 0x561C,
    0x561C, 0x55FC, 0x55FC, 0x561D, 0x55FC, 0x2BF9, 0x23F8, 0x2C19,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0x45BB, 0x55FC, 0x55FC, 0x561C, 0x561D, 0x561D, 0x561C, 0x4DFC,
    0x561C, 0x561C, 0x561C, 0x2BF9, 0x2C19, 0x2BF7, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x00E8, 0x4DFC,
    0x55FC, 0xFFFF, 0x561C, 0x561C, 0x561D, 0x561C, 0x55FC, 0x561C,
    0x23D8, 0x2BF9, 0x2BF8, 0x0109, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x00E8, 0x4DFC, 0x561C,
    0x55FC, 0x4DFC, 0x4DFC, 0x55FC, 0x4D7C, 0x2BF9, 0x2BF8, 0x2C19,
    0x0109, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0x0109, 0x2BF9, 0x2C19, 0x2BF9,
    0x2BF9, 0x2BF8, 0x2BF9, 0x2BF9, 0x2BF8, 0x2C19, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0x0108, 0x00E9, 0x2BF8, 0x2C19, 0x2C19,
    0x2C19, 0x0109, 0x00C8, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0x0108, 0x00E8, 0x0108, 0x00E7, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F, 0xF81F,
    0xF81F, 0xF81F, 0xF81F, 0xF81F,
};

static void draw_icon_humi(uint16_t x, uint16_t y, uint16_t bg)
{
    TFT_DrawBitmap(x - 8U, y - 7U, 30, 30, icon_humi_bmp, 0xF81F, bg);
}

/* PTC 加热图标（形象：发热方块 + 热浪，约 12x17） */
static void draw_icon_ptc(uint16_t x, uint16_t y)
{
    uint16_t c = UI_ACCENT2;
    TFT_FillRect(x + 2, y + 2, 8, 8, c);
    TFT_FillRect(x + 4, y + 4, 4, 4, UI_CARD);        /* 中心挖空 */
    TFT_FillRect(x + 3, y + 12, 2, 2, c);             /* 热浪 */
    TFT_FillRect(x + 6, y + 12, 2, 2, c);
    TFT_FillRect(x + 2, y + 14, 2, 2, c);
    TFT_FillRect(x + 7, y + 14, 2, 2, c);
}

/* 圆形时钟图标（形象：圆盘 + 指针，约 12x12） */
static void draw_icon_clock(uint16_t x, uint16_t y)
{
    TFT_FillRect(x + 3, y + 1, 6, 1, UI_CYAN);
    TFT_FillRect(x + 2, y + 2, 8, 1, UI_CYAN);
    TFT_FillRect(x + 1, y + 3, 10, 7, UI_CYAN);
    TFT_FillRect(x + 2, y + 10, 8, 1, UI_CYAN);
    TFT_FillRect(x + 3, y + 11, 6, 1, UI_CYAN);
    TFT_FillRect(x + 3, y + 4, 6, 5, UI_CARD);
    TFT_FillRect(x + 5, y + 3, 2, 5, UI_CYAN);
    TFT_FillRect(x + 5, y + 6, 4, 2, UI_CYAN);
    TFT_FillRect(x + 5, y + 5, 1, 1, UI_CYAN);
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

/* 横屏卡片背景（可指定坐标/尺寸），sgl 风格：深色卡片 + 顶边高亮 */
static void draw_card_bg_at(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t accent_color)
{
    TFT_FillRect(x, y, w, h, UI_CARD);
    TFT_FillRect(x, y, w, 2, accent_color);        /* 顶部色条 */
    TFT_FillRect(x, y + h - 2, w, 2, accent_color); /* 底部色条 */
}

void UI_ShowBootScreen(void)
{
    uint16_t x;
    TFT_FillScreen(UI_BG);

    /* "QiMingXing" size=3，深蓝色，居中（10 字符 × 18px = 180px） */
    x = (TFT_WIDTH - 10U * 18U) / 2U;
    TFT_DrawString(x, 22, "QiMingXing", TFT_COLOR(0x15, 0x65, 0xC0), UI_BG, 3);

    /* "LianYan & -e-" 四色分段显示，居中（各颜色不重复） */
    {
        uint16_t cx = (TFT_WIDTH - 156U) / 2U;
        TFT_DrawString(cx, 60, "LianYan", TFT_COLOR(0x9C, 0x27, 0xB0), UI_BG, 2);  cx += 7 * 12;
        TFT_DrawString(cx, 60, " ", UI_TEXT_DIM, UI_BG, 2);       cx += 12;
        TFT_DrawString(cx, 60, "&", TFT_COLOR(0xFF, 0xC1, 0x07), UI_BG, 2);       cx += 12;
        TFT_DrawString(cx, 60, " ", UI_TEXT_DIM, UI_BG, 2);       cx += 12;
        TFT_DrawString(cx, 60, "-e-", TFT_COLOR(0xFF, 0x52, 0x52), UI_BG, 2);     cx += 3 * 12;
    }

    /* 进度条：宽 200 高 14 */
    TFT_FillRect(20, 98, 200, 1, UI_CARD_EDGE);
    TFT_FillRect(20, 111, 200, 1, UI_CARD_EDGE);
    TFT_FillRect(20, 98, 1, 14, UI_CARD_EDGE);
    TFT_FillRect(219, 98, 1, 14, UI_CARD_EDGE);
    for (int i = 0; i < 198; i += 2) {
        TFT_FillRect(21 + i, 99, 2, 12, TFT_COLOR(0x15, 0x65, 0xC0));
        Delay_ms(8);
    }
}

/* 通用横屏标题栏：深色底 + 分隔线 + 居中标题 */
static void draw_page_title(const char *title, uint16_t accent)
{
    TFT_FillRect(0, 0, TFT_WIDTH, 24, UI_TITLE_BG);
    TFT_FillRect(0, 24, TFT_WIDTH, 1, UI_CARD_EDGE);
    TFT_DrawString((TFT_WIDTH - (uint16_t)(strlen(title) * 12U)) / 2U,
                   6, title, accent, UI_TITLE_BG, 2);
}

/* 主界面 4 张卡片的位置（横屏 240x135，居中 2x2 小卡 + 底部时间栏） */
static void main_card_rect(uint8_t item, uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h)
{
    static const uint16_t card_w = 108;
    static const uint16_t card_h = 38;
    static const uint16_t gap = 8;
    static const uint16_t left = 8;
    static const uint16_t top = 8;
    switch (item) {
    case 0: *x = left;            *y = top; break;              /* TEMP */
    case 1: *x = left + card_w + gap; *y = top; break;          /* 湿度 */
    case 2: *x = left;            *y = top + card_h + gap; break; /* WEIGHT */
    case 3: *x = left + card_w + gap; *y = top + card_h + gap; break; /* PTC */
    default:*x = left; *y = top + (card_h + gap) * 2; *w = 224; *h = 30; return; /* 时间栏 */
    }
    *w = card_w;
    *h = card_h;
}

/* 绘制数值+单位（size2 与数字同字号），紧跟数值后，供全屏与局部刷新共用 */
static void draw_value_unit(uint8_t item, uint16_t x, uint16_t y, uint16_t h,
                            uint16_t bg, const char *value, uint16_t val_color)
{
    uint16_t vw = (uint16_t)(strlen(value) * 12U);
    uint16_t vx = (uint16_t)(x + 34);
    uint16_t ux = (uint16_t)(vx + vw + 4);

    TFT_DrawString(vx, y + (h - 16U) / 2U, value, val_color, bg, 2);

    switch (item) {
    case 0:  /* TEMP: °C */
    case 3:  /* PTC: °C */
        TFT_DrawString(ux, y + (h - 16U) / 2U, "C", val_color, bg, 2);
        draw_degree((uint16_t)(ux - 4), y + (h - 16U) / 2U - 1, val_color, 1);
        break;
    case 2:  /* WEIGHT: g */
        TFT_DrawString(ux, y + (h - 16U) / 2U, "g", val_color, bg, 2);
        break;
    default:
        break;
    }
}

/* 绘制单张主界面卡片：大图标(左) + 数值+单位(右)，单位与数字同字号(size2) */
static void draw_main_card(uint8_t item, uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h, uint16_t bg,
                           const char *value,
                           uint16_t val_color, uint8_t selected)
{
    uint16_t fill = selected ? UI_CARD_HI : bg;
    uint8_t icon_x, icon_y;

    fill_round_rect(x, y, w, h, fill, 10);
    draw_frame_rounded(x, y, w, h, selected ? UI_ACCENT : UI_CARD_EDGE, 10);

    /* 大图标：卡片左区，垂直居中（温度计位图 24px，其余矢量 20px） */
    icon_x = (uint8_t)(x + 10);
    icon_y = (uint8_t)(y + (h - 20U) / 2U);
    switch (item) {
    case 0:  draw_icon_temp(icon_x, icon_y, fill); break;
    case 1:  draw_icon_humi(icon_x, icon_y, fill); break;
    case 2:  draw_icon_weight(icon_x, icon_y); break;
    case 3:  draw_icon_ptc(icon_x, icon_y); break;
    default: draw_icon_clock(icon_x, icon_y); break;
    }

    draw_value_unit(item, x, y, h, fill, value, val_color);
}

/* 局部刷新单张主界面卡片（不整屏重绘，SGL 脏矩形思路） */
void UI_RefreshCard(uint8_t item)
{
    char buf[32];
    uint16_t x, y, w, h;
    main_card_rect(item, &x, &y, &w, &h);

    if (item == 4) {
        uint32_t hh, mm, ss;
        uint32_t rem_h, rem_m, rem_s;
        uint16_t fill = (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME;
        const char *state_str[] = {"IDLE", "HEAT", "DRY", "COOL", "DONE"};
        uint16_t state_color[] = {UI_TEXT_DIM, UI_ACCENT2, UI_WARN, UI_ACCENT, UI_OK};
        fill_round_rect(x, y, w, h, fill, 10);
        draw_frame_rounded(x, y, w, h, (g_sys.selected_item == 4) ? UI_ACCENT : UI_CARD_EDGE, 10);
        draw_icon_clock(x + 10, y + 9);
        hh = g_sys.params.dry_time_sec / 3600;
        mm = (g_sys.params.dry_time_sec % 3600) / 60;
        ss = g_sys.params.dry_time_sec % 60;
        sprintf(buf, "%02lu:%02lu:%02lu", hh, mm, ss);
        TFT_DrawString(x + (w - 8U * 12U) / 2U, y + (h - 14U) / 2U, buf, UI_ACCENT, fill, 2);
        TFT_DrawString(175, y + (h - 8U) / 2U, state_str[g_sys.run_state],
                       state_color[g_sys.run_state], fill, 1);
        if (g_sys.run_state == STATE_DRYING) {
            rem_h = g_sys.remaining_sec / 3600;
            rem_m = (g_sys.remaining_sec % 3600) / 60;
            rem_s = g_sys.remaining_sec % 60;
            sprintf(buf, "REM %02lu:%02lu:%02lu", rem_h, rem_m, rem_s);
            TFT_DrawString(x + (224U - 16U * 6U) / 2U, y + 20, buf, UI_OK, fill, 1);
        }
        return;
    }

    switch (item) {
    case 0:
        sprintf(buf, "%.1f", g_sys.current_temp);
        draw_main_card(0, x, y, w, h, CARD_BG_TEMP, buf, UI_WARN,
                       g_sys.selected_item == 0);
        break;
    case 1:
        sprintf(buf, "%.1f%%", g_sys.current_humidity);
        draw_main_card(1, x, y, w, h, CARD_BG_HUMI, buf, UI_CYAN,
                       g_sys.selected_item == 1);
        break;
    case 2:
        sprintf(buf, "%.1f", g_sys.weight_g);
        draw_main_card(2, x, y, w, h, CARD_BG_WEIGHT, buf, UI_PURPLE,
                       g_sys.selected_item == 2);
        break;
    default:
        sprintf(buf, "%.1f", g_sys.ptc_temp);
        draw_main_card(3, x, y, w, h, CARD_BG_PTC, buf, UI_ACCENT2,
                       g_sys.selected_item == 3);
        break;
    }
}

void UI_DrawMainScreen(void)
{
    char buf[32];
    uint32_t h, m, s;
    uint16_t cy;
    const char *state_str[] = {"IDLE", "HEAT", "DRY", "COOL", "DONE"};
    uint16_t state_color[] = {UI_TEXT_DIM, UI_ACCENT2, UI_WARN, UI_ACCENT, UI_OK};

    const uint16_t card_w = 108;
    const uint16_t card_h = 38;
    const uint16_t gap = 8;
    const uint16_t left = 8;
    const uint16_t top = 8;
    TFT_FillScreen(UI_BG);

    /* ── 卡1 TEMP ── */
    sprintf(buf, "%.1f", g_sys.current_temp);
    draw_main_card(0, left, top, card_w, card_h, CARD_BG_TEMP, buf, UI_WARN,
                   g_sys.selected_item == 0);

    /* ── 卡2 湿度 ── */
    sprintf(buf, "%.1f%%", g_sys.current_humidity);
    draw_main_card(1, left + card_w + gap, top, card_w, card_h, CARD_BG_HUMI, buf, UI_CYAN,
                   g_sys.selected_item == 1);

    /* ── 卡3 WEIGHT ── */
    sprintf(buf, "%.1f", g_sys.weight_g);
    draw_main_card(2, left, top + card_h + gap, card_w, card_h, CARD_BG_WEIGHT, buf, UI_PURPLE,
                   g_sys.selected_item == 2);

    /* ── 卡4 PTC ── */
    sprintf(buf, "%.1f", g_sys.ptc_temp);
    draw_main_card(3, left + card_w + gap, top + card_h + gap, card_w, card_h, CARD_BG_PTC, buf,
                   UI_ACCENT2, g_sys.selected_item == 3);

    /* ── 底部：烘干时间独立一栏 ── */
    cy = top + (card_h + gap) * 2;
    fill_round_rect(left, cy, 224, 30,
                    (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME, 10);
    draw_frame_rounded(left, cy, 224, 30,
                       (g_sys.selected_item == 4) ? UI_ACCENT : UI_CARD_EDGE, 10);
    draw_icon_clock(left + 10, cy + 9);
    h = g_sys.params.dry_time_sec / 3600;
    m = (g_sys.params.dry_time_sec % 3600) / 60;
    s = g_sys.params.dry_time_sec % 60;
    sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
    TFT_DrawString(left + (224U - 8U * 12U) / 2U, cy + (30U - 14U) / 2U, buf, UI_ACCENT,
                   (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME, 2);
    TFT_DrawString(175, cy + (30U - 8U) / 2U, state_str[g_sys.run_state],
                   state_color[g_sys.run_state],
                   (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME, 1);
    if (g_sys.run_state == STATE_DRYING) {
        h = g_sys.remaining_sec / 3600;
        m = (g_sys.remaining_sec % 3600) / 60;
        s = g_sys.remaining_sec % 60;
        sprintf(buf, "REM %02lu:%02lu:%02lu", h, m, s);
        TFT_DrawString(left + (224U - 16U * 6U) / 2U, cy + 20, buf, UI_OK,
                       (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME, 1);
    }
}

void UI_DrawWeightScreen(void)
{
    char buf[32];
    TFT_FillScreen(UI_BG);
    draw_page_title("WEIGHT TOOL", UI_ACCENT2);
    sprintf(buf, "Current: %.1fg", g_sys.weight_g);
    TFT_DrawString(30, 50, buf, UI_ACCENT2, UI_BG, 2);
    fill_round_rect(10, 80, 220, 26, UI_CARD, 6);
    if (g_sys.selected_item == 0) {
        TFT_DrawString(20, 87, "> Tare", UI_WARN, UI_CARD, 1);
        TFT_DrawString(120, 87, "  Exit", UI_TEXT_DIM, UI_CARD, 1);
    } else {
        TFT_DrawString(20, 87, "  Tare", UI_TEXT_DIM, UI_CARD, 1);
        TFT_DrawString(120, 87, "> Exit", UI_WARN, UI_CARD, 1);
    }
    TFT_DrawString(60, 118, "Click: Select", UI_TEXT_DIM, UI_BG, 1);
}

void UI_DrawTempAdjust(void)
{
    char buf[16];
    TFT_FillScreen(UI_BG);
    draw_page_title("TEMP SETTINGS", UI_WARN);
    if (g_sys.selected_item == 0) {
        TFT_DrawString(10, 40, "> Set Temp", UI_TEXT, UI_BG, 1);
        TFT_DrawString(150, 40, "  PID Autotune", UI_TEXT_DIM, UI_BG, 1);
    } else if (g_sys.selected_item == 1) {
        TFT_DrawString(10, 40, "  Set Temp", UI_TEXT_DIM, UI_BG, 1);
        TFT_DrawString(150, 40, "> PID Autotune", UI_TEXT, UI_BG, 1);
    } else {
        TFT_DrawString(10, 40, "  Set Temp", UI_TEXT_DIM, UI_BG, 1);
        TFT_DrawString(150, 40, "  PID Autotune", UI_TEXT_DIM, UI_BG, 1);
    }
    sprintf(buf, "%d", g_sys.params.target_temp);
    TFT_DrawString(10, 70, buf, UI_WARN, UI_BG, 3);
    draw_degree(70, 72, UI_WARN, 2);
    TFT_DrawString(10, 105, "Rotate: Select  Click: Enter", UI_TEXT_DIM, UI_BG, 1);
    fill_round_rect(10, 118, 220, 1, UI_CARD_EDGE, 0);
}

void UI_DrawTempEdit(void)
{
    char buf[16];
    TFT_FillScreen(UI_BG);
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
    TFT_FillScreen(UI_BG);
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
    TFT_FillScreen(UI_BG);
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
    if (g_sys.time_cursor == TIME_FIELD_HOUR) draw_frame(10, 48, 42, 28, UI_TEXT_DIM);
    if (g_sys.time_cursor == TIME_FIELD_MIN) draw_frame(58, 48, 42, 28, UI_TEXT_DIM);
    if (g_sys.time_cursor == TIME_FIELD_SEC) draw_frame(106, 48, 20, 28, UI_TEXT_DIM);
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
    TFT_FillScreen(UI_BG);
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
    TFT_FillScreen(UI_BG);
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
    TFT_FillScreen(UI_BG);
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
    TFT_FillScreen(UI_BG);
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
    TFT_FillScreen(UI_BG);
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
    const char *items[] = {"Start/Stop", "WiFi", "Motor", "About", "Reset", "Exit"};
    uint8_t i;

    TFT_FillScreen(UI_BG);
    TFT_FillRect(0, 0, TFT_WIDTH, 24, UI_TITLE_BG);
    TFT_FillRect(0, 24, TFT_WIDTH, 1, UI_CARD_EDGE);
    TFT_DrawString((TFT_WIDTH - 5 * 12U) / 2U, 6, "MENU", UI_ACCENT, UI_TITLE_BG, 2);

    /* 菜单项：选中项蓝底白字 + 圆角外轮廓，未选中灰字 */
    for (i = 0; i < 6; i++) {
        uint16_t y = (uint16_t)(32 + i * 17);
        if (i == g_sys.selected_item) {
            fill_round_rect(8, y, 224, 14, UI_ACCENT, 7);
            draw_frame_rounded(8, y, 224, 14, TFT_COLOR(0x0D, 0x47, 0xA1), 7);
            TFT_DrawString(18, y + 3, items[i], UI_TEXT, UI_ACCENT, 1);
        } else {
            TFT_DrawString(18, y + 3, items[i], UI_TEXT_DIM, UI_BG, 1);
        }
    }
}

void UI_DrawMotorAdjust(void)
{
    char buf[32];
    uint8_t is_tmc = (g_sys.params.motor_driver == MOTOR_DRIVER_TMC2209);
    uint8_t count = is_tmc ? 9 : 6;

    TFT_FillScreen(UI_BG);
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

    TFT_FillScreen(UI_BG);
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
    TFT_FillScreen(UI_BG);
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
    TFT_FillScreen(UI_BG);
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

static uint8_t ota_screen_drawn = 0;

/* 重新进入 OTA 界面时调用，下次 UI_DrawOTAScreen 会整屏重画 */
void UI_ResetOTAScreen(void)
{
    ota_screen_drawn = 0;
}

void UI_DrawOTAScreen(void)
{
    if (!ota_screen_drawn) {
        ota_screen_drawn = 1;
        TFT_FillScreen(UI_BG);
        TFT_DrawString(25, 10, "FIRMWARE", COLOR_WHITE, COLOR_BLACK, 2);
        draw_btn(184, "  Exit", COLOR_GRAY, g_sys.selected_item == 0);
        if (g_sys.selected_item == 0) draw_frame(5, 184, 125, BTN_H, COLOR_WHITE);
    }
    if (g_sys.ota_downloading) {
        TFT_FillRect(10, 55, 120, 30, COLOR_BLACK);
        TFT_DrawString(10, 60, "Downloading...", COLOR_YELLOW, COLOR_BLACK, 1);
        TFT_FillRect(10, 90, 115, 1, COLOR_WHITE);
        TFT_FillRect(10, 99, 115, 1, COLOR_WHITE);
        TFT_FillRect(10, 90, 1, 10, COLOR_WHITE);
        TFT_FillRect(124, 90, 1, 10, COLOR_WHITE);
        TFT_FillRect(11, 91, (113 * g_sys.ota_progress) / 100, 8, COLOR_GREEN);
        char buf[32];
        sprintf(buf, "%d%% (%lu/%lu)", g_sys.ota_progress, g_sys.ota_received_size, g_sys.ota_total_size);
        TFT_FillRect(10, 110, 120, 12, COLOR_BLACK);
        TFT_DrawString(10, 115, buf, COLOR_GRAY, COLOR_BLACK, 1);
    } else if (g_sys.ota_download_done) {
        TFT_FillRect(10, 55, 120, 60, COLOR_BLACK);
        TFT_DrawString(10, 60, "Download OK!", COLOR_GREEN, COLOR_BLACK, 2);
        TFT_DrawString(10, 100, "Click to Update", COLOR_YELLOW, COLOR_BLACK, 1);
        TFT_DrawString(10, 120, "Device will restart", COLOR_GRAY, COLOR_BLACK, 1);
    } else {
        TFT_FillRect(10, 55, 120, 60, COLOR_BLACK);
        TFT_DrawString(10, 60, "Drag firmware", COLOR_GRAY, COLOR_BLACK, 1);
        TFT_DrawString(10, 80, "to web page", COLOR_GRAY, COLOR_BLACK, 1);
        TFT_DrawString(10, 110, "and click Upload", COLOR_GRAY, COLOR_BLACK, 1);
    }
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
            case SCREEN_OTA:
                UI_ResetOTAScreen();
                UI_DrawOTAScreen();
                break;
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
    const uint16_t card_w = 108;
    const uint16_t card_h = 38;
    const uint16_t gap = 8;
    const uint16_t left = 8;
    const uint16_t top = 8;
    const uint16_t c2x = left + card_w + gap;
    uint16_t cy;
    static char last_val[4][32];
    static uint8_t last_sel = 0xFF;

    /* TEMP 卡（行1 左）：值 + °C，选中卡用高亮底色 */
    sprintf(buf, "%.1f", g_sys.current_temp);
    if (strcmp(buf, last_val[0]) != 0) {
        draw_value_unit(0, left, top, card_h,
                        (g_sys.selected_item == 0) ? UI_CARD_HI : CARD_BG_TEMP,
                        buf, UI_WARN);
        strcpy(last_val[0], buf);
    }

    /* 湿度卡（行1 右） */
    sprintf(buf, "%.1f%%", g_sys.current_humidity);
    if (strcmp(buf, last_val[1]) != 0) {
        draw_value_unit(1, c2x, top, card_h,
                        (g_sys.selected_item == 1) ? UI_CARD_HI : CARD_BG_HUMI,
                        buf, UI_CYAN);
        strcpy(last_val[1], buf);
    }

    /* WEIGHT 卡（行2 左） */
    sprintf(buf, "%.1f", g_sys.weight_g);
    if (strcmp(buf, last_val[2]) != 0) {
        draw_value_unit(2, left, top + card_h + gap, card_h,
                        (g_sys.selected_item == 2) ? UI_CARD_HI : CARD_BG_WEIGHT,
                        buf, UI_PURPLE);
        strcpy(last_val[2], buf);
    }

    /* PTC 卡（行2 右）：值 + °C */
    sprintf(buf, "%.1f", g_sys.ptc_temp);
    if (strcmp(buf, last_val[3]) != 0) {
        draw_value_unit(3, c2x, top + card_h + gap, card_h,
                        (g_sys.selected_item == 3) ? UI_CARD_HI : CARD_BG_PTC,
                        buf, UI_ACCENT2);
        strcpy(last_val[3], buf);
    }

    /* 选中变化时：值文字底色随选中态切换（重绘全部值，消除旧底色残留） */
    if (g_sys.selected_item != last_sel) {
        last_sel = g_sys.selected_item;
        sprintf(buf, "%.1f", g_sys.current_temp);
        draw_value_unit(0, left, top, card_h,
                        (g_sys.selected_item == 0) ? UI_CARD_HI : CARD_BG_TEMP,
                        buf, UI_WARN);
        sprintf(buf, "%.1f%%", g_sys.current_humidity);
        draw_value_unit(1, c2x, top, card_h,
                        (g_sys.selected_item == 1) ? UI_CARD_HI : CARD_BG_HUMI,
                        buf, UI_CYAN);
        sprintf(buf, "%.1f", g_sys.weight_g);
        draw_value_unit(2, left, top + card_h + gap, card_h,
                        (g_sys.selected_item == 2) ? UI_CARD_HI : CARD_BG_WEIGHT,
                        buf, UI_PURPLE);
        sprintf(buf, "%.1f", g_sys.ptc_temp);
        draw_value_unit(3, c2x, top + card_h + gap, card_h,
                        (g_sys.selected_item == 3) ? UI_CARD_HI : CARD_BG_PTC,
                        buf, UI_ACCENT2);
    }

    /* 烘干时间栏（底部，时间数字垂直居中） */
    cy = top + (card_h + gap) * 2;
    h = g_sys.params.dry_time_sec / 3600;
    m = (g_sys.params.dry_time_sec % 3600) / 60;
    s = g_sys.params.dry_time_sec % 60;
    sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
    TFT_DrawString(left + (224U - 8U * 12U) / 2U, cy + (30U - 14U) / 2U,
                    buf, UI_ACCENT,
                    (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME, 2);
    if (g_sys.run_state == STATE_DRYING) {
        h = g_sys.remaining_sec / 3600;
        m = (g_sys.remaining_sec % 3600) / 60;
        s = g_sys.remaining_sec % 60;
        sprintf(buf, "REM %02lu:%02lu:%02lu", h, m, s);
        TFT_DrawString(left + (224U - 16U * 6U) / 2U, cy + 20, buf, UI_OK,
                       (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME, 1);
    }

    /* 选中卡呼吸动画（局部色条） */
    draw_card_pulse(left, top, card_w, g_sys.selected_item == 0);
    draw_card_pulse(c2x, top, card_w, g_sys.selected_item == 1);
    draw_card_pulse(left, top + card_h + gap, card_w, g_sys.selected_item == 2);
    draw_card_pulse(c2x, top + card_h + gap, card_w, g_sys.selected_item == 3);
    draw_card_pulse(left, cy, 224, g_sys.selected_item == 4);
}

static void Delay_ms(uint16_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 7200; i++);
}
#endif /* BOOTLOADER_BUILD */

