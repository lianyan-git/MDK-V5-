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

/* ── sgl 风格主题色（深色蓝黑） ── */
#define UI_BG           TFT_COLOR(0x0E, 0x12, 0x1A)   /* 深蓝黑背景 */
#define UI_CARD         TFT_COLOR(0x1A, 0x22, 0x2E)   /* 卡片面 */
#define UI_CARD_HI      TFT_COLOR(0x26, 0x30, 0x40)   /* 卡片高亮/悬停 */
#define UI_CARD_EDGE    TFT_COLOR(0x33, 0x40, 0x52)   /* 卡片描边 */
#define UI_TEXT         TFT_COLOR(0xEC, 0xEF, 0xF3)   /* 主文字 */
#define UI_TEXT_DIM     TFT_COLOR(0x9A, 0xA5, 0xB5)   /* 次要文字 */
#define UI_TITLE_BG     TFT_COLOR(0x0A, 0x0E, 0x14)   /* 标题栏底 */
#define UI_ACCENT       TFT_COLOR(0x2E, 0xA6, 0xFF)   /* 亮蓝 */
#define UI_ACCENT2      TFT_COLOR(0xFF, 0x5C, 0x6E)   /* 珊瑚红 */
#define UI_OK           TFT_COLOR(0x3E, 0xD8, 0x6E)   /* 亮绿 */
#define UI_WARN         TFT_COLOR(0xFF, 0xC1, 0x2E)   /* 琥珀黄 */
#define UI_CYAN         TFT_COLOR(0x29, 0xE0, 0xD8)   /* 青 */
#define UI_PURPLE       TFT_COLOR(0xB0, 0x7B, 0xF0)   /* 紫 */

#define TFT_COLOR(r,g,b)  ((uint16_t)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|((b)>>3)))

static void draw_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
static void Delay_ms(uint16_t ms);

static void draw_frame(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color)
{
    TFT_FillRect(x, y, w, SEL_FRAME_W, color);
    TFT_FillRect(x, y + h - SEL_FRAME_W, w, SEL_FRAME_W, color);
    TFT_FillRect(x, y, SEL_FRAME_W, h, color);
    TFT_FillRect(x + w - SEL_FRAME_W, y, SEL_FRAME_W, h, color);
}

/* 真圆角矩形：四角用圆弧像素，中间用 FillRect 高效填充 */
static void fill_round_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                            uint16_t color, uint8_t radius)
{
    int16_t dx, dy;
    if (radius == 0U) { TFT_FillRect(x, y, w, h, color); return; }
    /* 中间主体 */
    TFT_FillRect(x + radius, y, w - 2U * radius, h, color);
    TFT_FillRect(x, y + radius, radius, h - 2U * radius, color);
    TFT_FillRect(x + w - radius, y + radius, radius, h - 2U * radius, color);
    /* 四角圆弧：逐行画小矩形近似圆 */
    for (dy = 0; dy < radius; dy++) {
        uint8_t r2 = (uint8_t)(radius - 1);
        int16_t d = (int16_t)(r2 - dy);
        uint16_t seg = 0;
        for (dx = 0; dx < radius; dx++) {
            int16_t ddx = (int16_t)(r2 - dx);
            if (ddx * ddx + d * d <= (int16_t)(r2 * r2)) {
                seg = (uint16_t)(dx + 1);
            }
        }
        if (seg > 0) {
            TFT_FillRect(x, y + dy, seg, 1, color);
            TFT_FillRect(x + w - seg, y + dy, seg, 1, color);
            TFT_FillRect(x, y + h - 1 - dy, seg, 1, color);
            TFT_FillRect(x + w - seg, y + h - 1 - dy, seg, 1, color);
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

/* 各卡片主题色（更饱和，背景区分明显） */
#define CARD_BG_TEMP    TFT_COLOR(0x2E, 0x20, 0x0E)   /* 暖橙底 */
#define CARD_BG_HUMI    TFT_COLOR(0x0E, 0x2A, 0x2E)   /* 冷青底 */
#define CARD_BG_WEIGHT  TFT_COLOR(0x24, 0x16, 0x36)   /* 紫底 */
#define CARD_BG_PTC     TFT_COLOR(0x32, 0x16, 0x18)   /* 红底 */
#define CARD_BG_TIME    TFT_COLOR(0x10, 0x1E, 0x34)   /* 蓝底 */

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

/* 温度计图标（放大版，约 12x22） */
static void draw_icon_temp(uint16_t x, uint16_t y)
{
    uint16_t c = UI_WARN;
    TFT_FillRect(x + 4, y + 13, 5, 8, c);
    TFT_FillRect(x + 3, y + 15, 7, 5, c);
    TFT_FillRect(x + 4, y + 16, 5, 3, c);
    TFT_FillRect(x + 6, y + 3, 1, 10, c);
    TFT_FillRect(x + 7, y, 1, 14, c);
    TFT_FillRect(x + 6, y, 2, 2, c);
}

/* 重量哑铃图标（放大版，约 14x14） */
static void draw_icon_weight(uint16_t x, uint16_t y)
{
    uint16_t c = UI_PURPLE;
    TFT_FillRect(x, y + 4, 4, 9, c);
    TFT_FillRect(x + 10, y + 4, 4, 9, c);
    TFT_FillRect(x + 1, y + 3, 2, 11, c);
    TFT_FillRect(x + 11, y + 3, 2, 11, c);
    TFT_FillRect(x + 4, y + 6, 6, 4, c);
}

/* 湿度水滴图标（放大版，约 12x16） */
static void draw_icon_humi(uint16_t x, uint16_t y)
{
    uint16_t c = UI_CYAN;
    TFT_FillRect(x + 4, y, 4, 4, c);
    TFT_FillRect(x + 2, y + 4, 8, 4, c);
    TFT_FillRect(x + 1, y + 8, 10, 4, c);
    TFT_FillRect(x + 2, y + 12, 8, 3, c);
    TFT_FillRect(x + 4, y + 15, 4, 1, c);
}

/* 圆形时钟图标（饱满，约 12x12） */
static void draw_icon_clock(uint16_t x, uint16_t y)
{
    /* 圆盘外圈 */
    TFT_FillRect(x + 3, y + 1, 6, 1, UI_CYAN);
    TFT_FillRect(x + 2, y + 2, 8, 1, UI_CYAN);
    TFT_FillRect(x + 1, y + 3, 10, 7, UI_CYAN);
    TFT_FillRect(x + 2, y + 10, 8, 1, UI_CYAN);
    TFT_FillRect(x + 3, y + 11, 6, 1, UI_CYAN);
    /* 表盘内芯（挖空） */
    TFT_FillRect(x + 3, y + 4, 6, 5, UI_CARD);
    /* 指针 */
    TFT_FillRect(x + 5, y + 3, 2, 5, UI_CYAN);   /* 时针（向上） */
    TFT_FillRect(x + 5, y + 6, 4, 2, UI_CYAN);   /* 分针（向右） */
    TFT_FillRect(x + 5, y + 5, 1, 1, UI_CYAN);   /* 轴心 */
}

/* PTC 加热图标（放大版，约 12x16） */
static void draw_icon_ptc(uint16_t x, uint16_t y)
{
    uint16_t c = UI_ACCENT2;
    TFT_FillRect(x + 1, y + 2, 10, 10, c);
    TFT_FillRect(x + 3, y + 4, 6, 6, c);
    TFT_FillRect(x + 5, y + 12, 1, 3, c);
    TFT_FillRect(x + 8, y + 12, 1, 3, c);
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

    /* "QiMingXing" size=3 → 每字符 18px，11字符=198px，水平居中，上移 */
    x = (TFT_WIDTH - 198U) / 2U;
    TFT_DrawString(x, 22, "QiMingXing", UI_ACCENT, UI_BG, 3);

    /* "LianYan & -e-" 三色分段显示（与 bootloader 一致），水平居中 */
    {
        uint16_t cx = (TFT_WIDTH - 168U) / 2U;   /* 总宽：7+1+1+3 字符 ×12px = 144px */
        TFT_DrawString(cx, 60, "LianYan", UI_ACCENT, UI_BG, 2);  cx += 7 * 12;
        TFT_DrawString(cx, 60, " ", UI_TEXT_DIM, UI_BG, 2);       cx += 12;
        TFT_DrawString(cx, 60, "&", UI_WARN, UI_BG, 2);           cx += 12;
        TFT_DrawString(cx, 60, " ", UI_TEXT_DIM, UI_BG, 2);       cx += 12;
        TFT_DrawString(cx, 60, "-e-", UI_ACCENT2, UI_BG, 2);      cx += 3 * 12;
    }

    /* 进度条：更大（宽 200 高 14） */
    TFT_FillRect(20, 98, 200, 1, UI_CARD_EDGE);
    TFT_FillRect(20, 111, 200, 1, UI_CARD_EDGE);
    TFT_FillRect(20, 98, 1, 14, UI_CARD_EDGE);
    TFT_FillRect(219, 98, 1, 14, UI_CARD_EDGE);
    for (int i = 0; i < 198; i += 2) {
        TFT_FillRect(21 + i, 99, 2, 12, UI_ACCENT);
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

/* 主界面 4 张卡片的位置（横屏 240x135，无标题栏，2x2 小卡 + 底部时间栏） */
static void main_card_rect(uint8_t item, uint16_t *x, uint16_t *y, uint16_t *w, uint16_t *h)
{
    static const uint16_t card_w = 105;
    static const uint16_t card_h = 38;
    static const uint16_t gap = 6;
    static const uint16_t top = 4;
    switch (item) {
    case 0: *x = 6;            *y = top; break;              /* TEMP */
    case 1: *x = 6 + card_w + gap; *y = top; break;          /* 湿度 */
    case 2: *x = 6;            *y = top + card_h + gap; break; /* WEIGHT */
    case 3: *x = 6 + card_w + gap; *y = top + card_h + gap; break; /* PTC */
    default:*x = 6; *y = top + (card_h + gap) * 2 + 2; *w = 228; *h = 32; return; /* 时间栏 */
    }
    *w = card_w;
    *h = card_h;
}

/* 绘制单张主界面卡片：大图标(左) + 数值+单位(右)，全部垂直居中，无文字标签 */
static void draw_main_card(uint8_t item, uint16_t x, uint16_t y,
                           uint16_t w, uint16_t h, uint16_t bg,
                           const char *value,
                           uint16_t val_color, uint8_t selected)
{
    uint16_t fill = selected ? UI_CARD_HI : bg;
    uint8_t vw = (uint8_t)(strlen(value) * 12U);
    uint8_t icon_x, icon_y;
    uint16_t vx;

    fill_round_rect(x, y, w, h, fill, 10);
    draw_frame(x, y, w, h, selected ? UI_ACCENT : UI_CARD_EDGE);

    /* 大图标：卡片左区，垂直居中（图标高约 20px） */
    icon_x = (uint8_t)(x + 10);
    icon_y = (uint8_t)(y + (h - 20U) / 2U);
    switch (item) {
    case 0:  draw_icon_temp(icon_x, icon_y); break;
    case 1:  draw_icon_humi(icon_x, icon_y); break;
    case 2:  draw_icon_weight(icon_x, icon_y); break;
    case 3:  draw_icon_ptc(icon_x, icon_y); break;
    default: draw_icon_clock(icon_x, icon_y); break;
    }

    /* 数值+单位：图标右侧，垂直居中 */
    vx = (uint16_t)(x + 34 + (w - 34U - vw) / 2U);
    TFT_DrawString(vx, y + (h - 16U) / 2U, value, val_color, fill, 2);
}

/* 局部刷新单张主界面卡片（不整屏重绘，SGL 脏矩形思路） */
void UI_RefreshCard(uint8_t item)
{
    char buf[32];
    uint16_t x, y, w, h;
    main_card_rect(item, &x, &y, &w, &h);

    if (item == 4) {
        uint32_t hh, mm, ss;
        uint16_t fill = (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME;
        fill_round_rect(x, y, w, h, fill, 10);
        draw_frame(x, y, w, h, (g_sys.selected_item == 4) ? UI_ACCENT : UI_CARD_EDGE);
        draw_icon_clock(x + 10, y + 9);
        hh = g_sys.params.dry_time_sec / 3600;
        mm = (g_sys.params.dry_time_sec % 3600) / 60;
        ss = g_sys.params.dry_time_sec % 60;
        sprintf(buf, "%02lu:%02lu:%02lu", hh, mm, ss);
        TFT_DrawString(x + (w - 8U * 12U) / 2U, y + 3, buf, UI_ACCENT, fill, 2);
        return;
    }

    switch (item) {
    case 0:
        sprintf(buf, "%.1f", g_sys.current_temp);
        draw_main_card(0, x, y, w, h, CARD_BG_TEMP, buf, UI_WARN,
                       g_sys.selected_item == 0);
        draw_degree(x + w - 15, y + (h - 10U) / 2U, UI_WARN, 1);
        TFT_DrawString(x + w - 11, y + (h - 8U) / 2U, "C", UI_TEXT_DIM,
                       (g_sys.selected_item == 0) ? UI_CARD_HI : CARD_BG_TEMP, 1);
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
        TFT_DrawString(x + w - 13, y + (h - 8U) / 2U, "g", UI_TEXT_DIM,
                       (g_sys.selected_item == 2) ? UI_CARD_HI : CARD_BG_WEIGHT, 1);
        break;
    default:
        sprintf(buf, "%.1f", g_sys.ptc_temp);
        draw_main_card(3, x, y, w, h, CARD_BG_PTC, buf, UI_ACCENT2,
                       g_sys.selected_item == 3);
        draw_degree(x + w - 15, y + (h - 10U) / 2U, UI_ACCENT2, 1);
        TFT_DrawString(x + w - 11, y + (h - 8U) / 2U, "C", UI_TEXT_DIM,
                       (g_sys.selected_item == 3) ? UI_CARD_HI : CARD_BG_PTC, 1);
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

    const uint16_t card_w = 105;
    const uint16_t card_h = 38;
    const uint16_t gap = 6;
    const uint16_t top = 4;

    TFT_FillScreen(UI_BG);

    /* ── 卡1 TEMP（值 + °C 单位） ── */
    sprintf(buf, "%.1f", g_sys.current_temp);
    draw_main_card(0, 6, top, card_w, card_h, CARD_BG_TEMP, buf, UI_WARN,
                   g_sys.selected_item == 0);
    draw_degree(6 + card_w - 15, top + (card_h - 10U) / 2U, UI_WARN, 1);
    TFT_DrawString(6 + card_w - 11, top + (card_h - 8U) / 2U, "C", UI_TEXT_DIM,
                   (g_sys.selected_item == 0) ? UI_CARD_HI : CARD_BG_TEMP, 1);

    /* ── 卡2 湿度 ── */
    sprintf(buf, "%.1f%%", g_sys.current_humidity);
    draw_main_card(1, 6 + card_w + gap, top, card_w, card_h, CARD_BG_HUMI, buf, UI_CYAN,
                   g_sys.selected_item == 1);

    /* ── 卡3 WEIGHT（值 + g 单位） ── */
    sprintf(buf, "%.1f", g_sys.weight_g);
    draw_main_card(2, 6, top + card_h + gap, card_w, card_h, CARD_BG_WEIGHT, buf, UI_PURPLE,
                   g_sys.selected_item == 2);
    TFT_DrawString(6 + card_w - 13, top + card_h + gap + (card_h - 8U) / 2U, "g", UI_TEXT_DIM,
                   (g_sys.selected_item == 2) ? UI_CARD_HI : CARD_BG_WEIGHT, 1);

    /* ── 卡4 PTC（值 + °C 单位） ── */
    sprintf(buf, "%.1f", g_sys.ptc_temp);
    draw_main_card(3, 6 + card_w + gap, top + card_h + gap, card_w, card_h, CARD_BG_PTC, buf,
                   UI_ACCENT2, g_sys.selected_item == 3);
    draw_degree(6 + card_w + gap + card_w - 15, top + card_h + gap + (card_h - 10U) / 2U,
                UI_ACCENT2, 1);
    TFT_DrawString(6 + card_w + gap + card_w - 11, top + card_h + gap + (card_h - 8U) / 2U,
                   "C", UI_TEXT_DIM,
                   (g_sys.selected_item == 3) ? UI_CARD_HI : CARD_BG_PTC, 1);

    /* ── 底部：烘干时间独立一栏 ── */
    cy = top + (card_h + gap) * 2 + 2;
    fill_round_rect(6, cy, 228, 32,
                    (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME, 10);
    draw_frame(6, cy, 228, 32,
               (g_sys.selected_item == 4) ? UI_ACCENT : UI_CARD_EDGE);
    draw_icon_clock(16, cy + 9);
    h = g_sys.params.dry_time_sec / 3600;
    m = (g_sys.params.dry_time_sec % 3600) / 60;
    s = g_sys.params.dry_time_sec % 60;
    sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
    /* 时间数字：时间栏内水平居中 */
    TFT_DrawString(6 + (228U - 8U * 12U) / 2U, cy + 3, buf, UI_ACCENT,
                   (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME, 2);
    TFT_DrawString(175, cy + 5, state_str[g_sys.run_state],
                   state_color[g_sys.run_state],
                   (g_sys.selected_item == 4) ? UI_CARD_HI : CARD_BG_TIME, 1);
    if (g_sys.run_state == STATE_DRYING) {
        h = g_sys.remaining_sec / 3600;
        m = (g_sys.remaining_sec % 3600) / 60;
        s = g_sys.remaining_sec % 60;
        sprintf(buf, "REM %02lu:%02lu:%02lu", h, m, s);
        TFT_DrawString(6 + (228U - 16U * 6U) / 2U, cy + 19, buf, UI_OK,
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

    /* 菜单项：选中项蓝底白字，未选中透明灰字 */
    for (i = 0; i < 6; i++) {
        uint16_t y = (uint16_t)(32 + i * 17);
        if (i == g_sys.selected_item) {
            fill_round_rect(8, y, 224, 14, UI_ACCENT, 7);
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
    const uint16_t card_w = 105;
    const uint16_t gap = 6;
    const uint16_t c2x = 6 + card_w + gap;   /* 第二列 x */

    /* TEMP 卡（值居中 + 单位°C） */
    sprintf(buf, "%.1f", g_sys.current_temp);
    TFT_DrawString(6 + 26U + (card_w - 26U - 4U * 12U) / 2U, 18, buf, UI_WARN, CARD_BG_TEMP, 2);

    /* 湿度卡 */
    sprintf(buf, "%.1f%%", g_sys.current_humidity);
    TFT_DrawString(c2x + 26U + (card_w - 26U - 5U * 12U) / 2U, 18, buf, UI_CYAN, CARD_BG_HUMI, 2);

    /* WEIGHT 卡（值居中 + 单位 g） */
    sprintf(buf, "%.1f", g_sys.weight_g);
    TFT_DrawString(6 + 26U + (card_w - 26U - 4U * 12U) / 2U, 62, buf, UI_PURPLE, CARD_BG_WEIGHT, 2);

    /* PTC 卡（值居中 + 单位°C） */
    sprintf(buf, "%.1f", g_sys.ptc_temp);
    TFT_DrawString(c2x + 26U + (card_w - 26U - 4U * 12U) / 2U, 62, buf, UI_ACCENT2, CARD_BG_PTC, 2);

    /* 烘干时间栏（底部，时间数字水平居中） */
    h = g_sys.params.dry_time_sec / 3600;
    m = (g_sys.params.dry_time_sec % 3600) / 60;
    s = g_sys.params.dry_time_sec % 60;
    sprintf(buf, "%02lu:%02lu:%02lu", h, m, s);
    TFT_DrawString(6 + (228U - 8U * 12U) / 2U, 91, buf, UI_ACCENT, CARD_BG_TIME, 2);
    if (g_sys.run_state == STATE_DRYING) {
        h = g_sys.remaining_sec / 3600;
        m = (g_sys.remaining_sec % 3600) / 60;
        s = g_sys.remaining_sec % 60;
        sprintf(buf, "REM %02lu:%02lu:%02lu", h, m, s);
        TFT_DrawString(6 + (228U - 16U * 6U) / 2U, 107, buf, UI_OK, CARD_BG_TIME, 1);
    }

    /* 选中卡呼吸动画（局部色条） */
    draw_card_pulse(6, 4, card_w, g_sys.selected_item == 0);
    draw_card_pulse(c2x, 4, card_w, g_sys.selected_item == 1);
    draw_card_pulse(6, 48, card_w, g_sys.selected_item == 2);
    draw_card_pulse(c2x, 48, card_w, g_sys.selected_item == 3);
    draw_card_pulse(6, 88, 228, g_sys.selected_item == 4);
}

static void Delay_ms(uint16_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 7200; i++);
}
#endif /* BOOTLOADER_BUILD */

