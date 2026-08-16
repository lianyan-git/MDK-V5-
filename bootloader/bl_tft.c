/*
 * bl_tft.c
 * Bootloader TFT驱动 - 精简实现
 * 代码量控制在2KB以内
 */

#include "bl_tft.h"
#include "pin_config.h"
#include "board.h"
#include "stm32f10x.h"
#include <string.h>
#include <stdio.h>

static void Delay_ms(uint16_t ms);

#define TFT_WIDTH   240
#define TFT_HEIGHT  135

#define COLOR_BLACK     0x0000
#define COLOR_WHITE     0xFFFF
#define COLOR_RED       0xF800
#define COLOR_GREEN     0x07E0
#define COLOR_BLUE      0x001F
#define COLOR_YELLOW    0xFFE0
#define COLOR_CYAN      0x07FF
#define COLOR_GRAY      0x8410
#define COLOR_DARK      0x4208
#define COLOR_ORANGE    0xFC00

/*═════════════════════════════════════════════════════════════════════════════
 *  SPI底层 (与W25Q128共享SPI1)
 *═════════════════════════════════════════════════════════════════════════════*/

static void SPI1_Send(uint8_t byte) {
    volatile uint32_t guard = 0;
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET) {
        if (++guard > 500000U) return;   /* 超时保护，避免看门狗复位 */
    }
    SPI_I2S_SendData(SPI1, byte);
    guard = 0;
    while (SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET) {
        if (++guard > 500000U) return;   /* 超时保护，避免看门狗复位 */
    }
    (void)SPI_I2S_ReceiveData(SPI1);
}

static void TFT_Select(void) {
    GPIO_SetBits(PIN_W25Q128_CS_PORT, PIN_W25Q128_CS_PIN);   // 关闭Flash
    GPIO_ResetBits(PIN_TFT_CS_PORT, PIN_TFT_CS_PIN);           // 选中TFT
}

static void TFT_Release(void) {
    GPIO_SetBits(PIN_TFT_CS_PORT, PIN_TFT_CS_PIN);
}

static void TFT_Cmd(uint8_t cmd) {
    TFT_Select();
    GPIO_ResetBits(PIN_TFT_DC_PORT, PIN_TFT_DC_PIN);  // DC=0
    SPI1_Send(cmd);
    TFT_Release();
}

static void TFT_Data8(uint8_t data) {
    TFT_Select();
    GPIO_SetBits(PIN_TFT_DC_PORT, PIN_TFT_DC_PIN);    // DC=1
    SPI1_Send(data);
    TFT_Release();
}

/*═════════════════════════════════════════════════════════════════════════════
 *  初始化
 *═════════════════════════════════════════════════════════════════════════════*/

void BL_TFT_Init(void) {
    // 使能时钟（注意：背光在 PB0，必须使能 GPIOB）
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1 | RCC_APB2Periph_GPIOA |
                             RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitTypeDef GPIO_InitStruct;

    // SPI1引脚 (PA5/6/7)
    GPIO_InitStruct.GPIO_Pin = PIN_SPI1_SCK_PIN | PIN_SPI1_MOSI_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PIN_SPI1_SCK_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.GPIO_Pin = PIN_SPI1_MISO_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(PIN_SPI1_MISO_PORT, &GPIO_InitStruct);

    // TFT控制引脚
    GPIO_InitStruct.GPIO_Pin = PIN_TFT_CS_PIN;
    GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
    GPIO_Init(PIN_TFT_CS_PORT, &GPIO_InitStruct);
    GPIO_SetBits(PIN_TFT_CS_PORT, PIN_TFT_CS_PIN);

    GPIO_InitStruct.GPIO_Pin = PIN_TFT_DC_PIN;
    GPIO_Init(PIN_TFT_DC_PORT, &GPIO_InitStruct);

    // PC13 RES (2MHz)
    GPIO_InitStruct.GPIO_Pin = PIN_TFT_RES_PIN;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(PIN_TFT_RES_PORT, &GPIO_InitStruct);

    // PB0 背光
    GPIO_InitStruct.GPIO_Pin = PIN_TFT_BL_PIN;
    GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PIN_TFT_BL_PORT, &GPIO_InitStruct);

    // 初始化SPI1
    SPI_InitTypeDef SPI_InitStruct;
    SPI_InitStruct.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
    SPI_InitStruct.SPI_Mode = SPI_Mode_Master;
    SPI_InitStruct.SPI_DataSize = SPI_DataSize_8b;
    SPI_InitStruct.SPI_CPOL = SPI_CPOL_Low;
    SPI_InitStruct.SPI_CPHA = SPI_CPHA_1Edge;
    SPI_InitStruct.SPI_NSS = SPI_NSS_Soft;
    SPI_InitStruct.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8;  /* 72M/8=9MHz，稳妥 */
    SPI_InitStruct.SPI_FirstBit = SPI_FirstBit_MSB;
    SPI_InitStruct.SPI_CRCPolynomial = 7;
    SPI_Init(SPI1, &SPI_InitStruct);
    SPI_Cmd(SPI1, ENABLE);

    // 硬件复位
    GPIO_ResetBits(PIN_TFT_RES_PORT, PIN_TFT_RES_PIN);
    Delay_ms(100);
    GPIO_SetBits(PIN_TFT_RES_PORT, PIN_TFT_RES_PIN);
    Delay_ms(100);

    // ST7789初始化序列 (精简)
    TFT_Cmd(0x11); Delay_ms(120);
    TFT_Cmd(0x36); TFT_Data8(0xA0);   /* 旋转 90°+180°（MX|MV） */
    TFT_Cmd(0x3A); TFT_Data8(0x05);
    TFT_Cmd(0xB2); TFT_Data8(0x0C); TFT_Data8(0x0C); TFT_Data8(0x00); TFT_Data8(0x33); TFT_Data8(0x33);
    TFT_Cmd(0xB7); TFT_Data8(0x35);
    TFT_Cmd(0xBB); TFT_Data8(0x19);
    TFT_Cmd(0xC0); TFT_Data8(0x2C);
    TFT_Cmd(0xC2); TFT_Data8(0x01);
    TFT_Cmd(0xC3); TFT_Data8(0x12);
    TFT_Cmd(0xC4); TFT_Data8(0x20);
    TFT_Cmd(0xC6); TFT_Data8(0x0F);
    TFT_Cmd(0xD0); TFT_Data8(0xA4); TFT_Data8(0xA1);
    TFT_Cmd(0xE0); 
    uint8_t g1[] = {0xD0,0x04,0x0D,0x11,0x13,0x2B,0x3F,0x54,0x4C,0x18,0x0D,0x0B,0x1F,0x23};
    for (int i=0; i<14; i++) TFT_Data8(g1[i]);
    TFT_Cmd(0xE1);
    uint8_t g2[] = {0xD0,0x04,0x0C,0x11,0x13,0x2C,0x3F,0x44,0x51,0x2F,0x1F,0x1F,0x20,0x23};
    for (int i=0; i<14; i++) TFT_Data8(g2[i]);
    TFT_Cmd(0x21);
    TFT_Cmd(0x29);

    // 打开背光 (高电平点亮)
    GPIO_SetBits(PIN_TFT_BL_PORT, PIN_TFT_BL_PIN);

    // 清屏
    BL_TFT_Clear(COLOR_BLACK);
}

/*═════════════════════════════════════════════════════════════════════════════
 *  基本绘图
 *═════════════════════════════════════════════════════════════════════════════*/

/* ST7789 1.14寸 135x240（GRAM 240x320）旋转 90°：X=40 / Y=52 */
#define TFT_X_OFFSET  40
#define TFT_Y_OFFSET  52

static void TFT_SetWindow(uint16_t x, uint16_t y, uint16_t x1, uint16_t y1) {
    TFT_Cmd(0x2A);
    TFT_Data8(((x + TFT_X_OFFSET) >> 8) & 0xFF);
    TFT_Data8((x + TFT_X_OFFSET) & 0xFF);
    TFT_Data8(((x1 + TFT_X_OFFSET) >> 8) & 0xFF);
    TFT_Data8((x1 + TFT_X_OFFSET) & 0xFF);
    TFT_Cmd(0x2B);
    TFT_Data8(((y + TFT_Y_OFFSET) >> 8) & 0xFF);
    TFT_Data8((y + TFT_Y_OFFSET) & 0xFF);
    TFT_Data8(((y1 + TFT_Y_OFFSET) >> 8) & 0xFF);
    TFT_Data8((y1 + TFT_Y_OFFSET) & 0xFF);
    TFT_Cmd(0x2C);
}

void BL_TFT_Clear(uint16_t color) {
    BL_TFT_FillRect(0, 0, TFT_WIDTH, TFT_HEIGHT, color);
}

void BL_TFT_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= TFT_WIDTH || y >= TFT_HEIGHT) return;
    if (x+w > TFT_WIDTH) w = TFT_WIDTH - x;
    if (y+h > TFT_HEIGHT) h = TFT_HEIGHT - y;

    TFT_SetWindow(x, y, x+w-1, y+h-1);
    TFT_Select();
    GPIO_SetBits(PIN_TFT_DC_PORT, PIN_TFT_DC_PIN);

    uint32_t pixels = w * h;
    for (uint32_t i = 0; i < pixels; i++) {
        SPI1_Send(color >> 8);
        SPI1_Send(color & 0xFF);
    }
    TFT_Release();
}

void BL_TFT_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint8_t size) {
    static const uint8_t font5x7[][7] = {
        /* ' ' */
        {0x00,0x00,0x00,0x00,0x00,0x00,0x00},
        /* '!' */
        {0x04,0x04,0x04,0x04,0x00,0x00,0x04},
        /* '"' */
        {0x0A,0x0A,0x0A,0x00,0x00,0x00,0x00},
        /* '#' */
        {0x0A,0x0A,0x1F,0x0A,0x1F,0x0A,0x0A},
        /* '$' */
        {0x04,0x0F,0x14,0x0E,0x05,0x1E,0x04},
        /* '%' */
        {0x18,0x19,0x02,0x04,0x08,0x13,0x03},
        /* '&' */
        {0x0C,0x12,0x14,0x08,0x15,0x12,0x0D},
        /* '\'' */
        {0x0C,0x04,0x08,0x00,0x00,0x00,0x00},
        /* '(' */
        {0x02,0x04,0x08,0x08,0x08,0x04,0x02},
        /* ')' */
        {0x08,0x04,0x02,0x02,0x02,0x04,0x08},
        /* '*' */
        {0x00,0x04,0x15,0x0E,0x15,0x04,0x00},
        /* '+' */
        {0x00,0x04,0x04,0x1F,0x04,0x04,0x00},
        /* ',' */
        {0x00,0x00,0x00,0x00,0x0C,0x04,0x08},
        /* '-' */
        {0x00,0x00,0x00,0x1F,0x00,0x00,0x00},
        /* '.' */
        {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C},
        /* '/' */
        {0x01,0x02,0x04,0x04,0x08,0x10,0x00},
    };
    static const uint8_t font_digits[][7] = {
        /* '0'-'9' */
        {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},
        {0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},
        {0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
        {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},
        {0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},
        {0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},
        {0x1F,0x01,0x02,0x04,0x08,0x08,0x08},
        {0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
        {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C},
    };
    static const uint8_t font_upper[][7] = {
        /* 'A'-'Z' */
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},
        {0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
        {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},
        {0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},
        {0x11,0x11,0x11,0x1F,0x11,0x11,0x11},
        {0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
        {0x01,0x01,0x01,0x01,0x11,0x11,0x0E},
        {0x11,0x12,0x14,0x18,0x14,0x12,0x11},
        {0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},
        {0x11,0x19,0x15,0x13,0x11,0x11,0x11},
        {0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},
        {0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
        {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},
        {0x1F,0x04,0x04,0x04,0x04,0x04,0x04},
        {0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},
        {0x11,0x11,0x11,0x15,0x15,0x1B,0x11},
        {0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
        {0x11,0x11,0x0A,0x04,0x04,0x04,0x04},
        {0x1F,0x01,0x02,0x04,0x08,0x10,0x1F},
    };
    const uint8_t *glyph;
    uint8_t row, col;

    if (c >= 'a' && c <= 'z') c = (char)(c - ('a' - 'A'));
    if (c >= '0' && c <= '9') glyph = font_digits[c - '0'];
    else if (c >= 'A' && c <= 'Z') glyph = font_upper[c - 'A'];
    else if (c >= ' ' && c <= '/') glyph = font5x7[c - ' '];
    else return;

    for (row = 0; row < 7; row++) {
        uint8_t line = glyph[row];
        for (col = 0; col < 5; col++) {
            if (line & (0x10 >> col)) {
                BL_TFT_FillRect(x + col * size, y + row * size, size, size, color);
            }
        }
    }
}

void BL_TFT_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint8_t size) {
    while (*str) {
        BL_TFT_DrawChar(x, y, *str, color, size);
        x += 6 * size + 1;
        str++;
    }
}

/*═════════════════════════════════════════════════════════════════════════════
 *  界面显示
 *═════════════════════════════════════════════════════════════════════════════*/

void BL_TFT_ShowBootLogo(void) {
    BL_TFT_Clear(COLOR_BLACK);
    BL_TFT_FillRect(30, 80, 75, 20, COLOR_GREEN);
    BL_TFT_FillRect(30, 110, 75, 20, COLOR_BLUE);
    BL_TFT_FillRect(30, 140, 75, 20, COLOR_RED);
    Delay_ms(500);
}

void BL_TFT_ShowUpgradeScreen(void) {
    BL_TFT_Clear(COLOR_BLACK);

    /* 横屏 240x135 布局 */
    // 标题栏
    BL_TFT_FillRect(0, 0, TFT_WIDTH, 30, COLOR_DARK);
    BL_TFT_DrawString(8, 8, "FIRMWARE UPDATE", COLOR_WHITE, 1);

    // 状态区
    BL_TFT_FillRect(5, 40, TFT_WIDTH-10, 20, COLOR_GRAY);

    // 进度条背景
    BL_TFT_FillRect(10, 75, TFT_WIDTH-20, 20, COLOR_GRAY);

    // 底部信息区
    BL_TFT_FillRect(5, 110, TFT_WIDTH-10, 20, COLOR_DARK);
}

void BL_TFT_ShowStatus(const char *text) {
    BL_TFT_FillRect(5, 40, TFT_WIDTH-10, 20, COLOR_BLACK);
    BL_TFT_DrawString(8, 42, text, COLOR_WHITE, 1);
}

void BL_TFT_ShowAPInfo(const char *ssid, const char *pass, const char *ip) {
    BL_TFT_FillRect(5, 110, TFT_WIDTH-10, 22, COLOR_BLACK);
    /* 第一行：SSID 在最左，密码在最右 */
    BL_TFT_DrawString(8, 112, ssid, COLOR_CYAN, 1);
    {
        uint16_t pass_len = 0;
        const char *p = pass;
        while (*p) { pass_len += 7; p++; }
        BL_TFT_DrawString(TFT_WIDTH - 8 - pass_len, 112, pass, COLOR_YELLOW, 1);
    }
    /* 第二行：IP 居中 */
    {
        char buf[32];
        buf[0] = '\0';
        /* "IP:xxx" 长度（字符宽 7px） */
        uint16_t ip_len = 3;
        const char *p = ip;
        while (*p) { ip_len += 7; p++; }
        int x = (TFT_WIDTH - ip_len) / 2;
        if (x < 0) x = 0;
        sprintf(buf, "IP:%s", ip);
        BL_TFT_DrawString((uint16_t)x, 124, buf, COLOR_GREEN, 1);
    }
}

void BL_TFT_ShowProgressBar(uint8_t percent) {
    if (percent > 100) percent = 100;

    uint16_t bar_width = ((TFT_WIDTH - 22) * percent) / 100;

    // 清除旧进度
    BL_TFT_FillRect(11, 76, TFT_WIDTH-22, 18, COLOR_GRAY);
    // 画新进度
    BL_TFT_FillRect(11, 76, bar_width, 18, COLOR_GREEN);
}

void BL_TFT_ShowProgressText(const char *text) {
    BL_TFT_FillRect(5, 98, TFT_WIDTH-10, 12, COLOR_BLACK);
    BL_TFT_DrawString(10, 98, text, COLOR_WHITE, 1);
}

void BL_TFT_ShowError(const char *text) {
    BL_TFT_Clear(COLOR_BLACK);
    BL_TFT_FillRect(0, 100, TFT_WIDTH, 40, COLOR_RED);
    BL_TFT_DrawString(6, 112, text, COLOR_WHITE, 1);
}

static void Delay_ms(uint16_t ms) {
    for (volatile uint32_t i = 0; i < ms * 7200; i++) {
        if ((i & 0x1FFFFU) == 0U) Watchdog_Kick();   /* 喂狗，避免看门狗复位 */
    }
}
