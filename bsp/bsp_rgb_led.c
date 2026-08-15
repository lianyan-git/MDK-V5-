#include "bsp_rgb_led.h"
#include "pin_config.h"
#include "stm32f10x.h"

static uint8_t strip2_buf[24];
static uint8_t strip3_buf[24];
static uint8_t rainbow_pos = 0;

/* 基于 SysTick 递减计数器(VAL)的周期延时。
 * SysTick 时钟 = HCLK(72MHz)，1 周期 ≈ 13.9ns。
 * 仅在 SysTick 已使能(CLKSOURCE=HCLK)时有效；bit 发送 <1ms，不会跨重载。 */
static void delay_cycles(uint32_t cycles)
{
    uint32_t start = SysTick->VAL;
    uint32_t elapsed;
    while (1) {
        elapsed = start - SysTick->VAL;      /* VAL 递减，差值为已过周期 */
        if (elapsed >= cycles) break;
    }
}

static void ws2812_send_byte(uint8_t byte, GPIO_TypeDef *port, uint16_t pin)
{
    for (int i = 7; i >= 0; i--) {
        if (byte & (1 << i)) {
            /* 1 码：高 550-850ns，低 550-850ns */
            port->BSRR = pin;
            delay_cycles(50);            /* 高 ~695ns */
            port->BSRR = (uint32_t)pin << 16;
            delay_cycles(50);            /* 低 ~695ns */
        } else {
            /* 0 码：高 220-380ns，低 580-1000ns */
            port->BSRR = pin;
            delay_cycles(18);            /* 高 ~250ns */
            port->BSRR = (uint32_t)pin << 16;
            delay_cycles(70);            /* 低 ~973ns */
        }
    }
}

static void ws2812_send_pixels(uint8_t *data, uint16_t num, GPIO_TypeDef *port, uint16_t pin)
{
    __disable_irq();
    for (uint16_t i = 0; i < num * 3; i++) {
        ws2812_send_byte(data[i], port, pin);
    }
    __enable_irq();
    GPIO_ResetBits(port, pin);
    for (volatile int d = 0; d < 50; d++);
}

static void hsv_to_rgb(uint8_t hue, uint8_t *r, uint8_t *g, uint8_t *b)
{
    uint8_t region = hue / 43;
    uint8_t remainder = (hue - region * 43) * 6;
    uint8_t q = 0, t = 0;

    q = 255 - remainder;
    t = remainder;

    switch (region) {
        case 0: *r = 255; *g = t;   *b = 0;   break;
        case 1: *r = q;   *g = 255; *b = 0;   break;
        case 2: *r = 0;   *g = 255; *b = t;   break;
        case 3: *r = 0;   *g = q;   *b = 255; break;
        case 4: *r = t;   *g = 0;   *b = 255; break;
        default: *r = 255; *g = 0;   *b = q;  break;
    }
}

void RGB_Strip_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    g.GPIO_Pin = PIN_RGB2_PIN | PIN_RGB3_PIN;
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PIN_RGB2_PORT, &g);
    GPIO_ResetBits(PIN_RGB2_PORT, PIN_RGB2_PIN | PIN_RGB3_PIN);
}

void RGB_Strip2_SetPixels(uint8_t *data, uint16_t num)
{
    uint16_t total = (num > 8) ? 8 : num;
    for (uint16_t i = 0; i < total * 3; i++) strip2_buf[i] = data[i];
    ws2812_send_pixels(strip2_buf, total, PIN_RGB2_PORT, PIN_RGB2_PIN);
}

void RGB_Strip3_SetPixels(uint8_t *data, uint16_t num)
{
    uint16_t total = (num > 8) ? 8 : num;
    for (uint16_t i = 0; i < total * 3; i++) strip3_buf[i] = data[i];
    ws2812_send_pixels(strip3_buf, total, PIN_RGB3_PORT, PIN_RGB3_PIN);
}

void RGB_Status_Red(void)
{
    uint8_t data[3] = {20, 0, 0};
    RGB_Strip2_SetPixels(data, 1);
}

void RGB_Status_Green(void)
{
    uint8_t data[3] = {0, 20, 0};
    RGB_Strip2_SetPixels(data, 1);
}

void RGB_Status_Off(void)
{
    uint8_t data[3] = {0, 0, 0};
    RGB_Strip2_SetPixels(data, 1);
}

void RGB_Progress_Rainbow(void)
{
    uint8_t data[3];
    uint8_t r, g, b;
    hsv_to_rgb(rainbow_pos, &r, &g, &b);
    data[0] = r >> 3;
    data[1] = g >> 3;
    data[2] = b >> 3;
    RGB_Strip3_SetPixels(data, 1);
    rainbow_pos++;
}

void RGB_Progress_ColorWheel(uint8_t pos)
{
    uint8_t data[21];
    for (int i = 0; i < 7; i++) {
        uint8_t hue = (pos + i * 36) % 256;
        uint8_t r, g, b;
        hsv_to_rgb(hue, &r, &g, &b);
        data[i * 3] = r >> 3;
        data[i * 3 + 1] = g >> 3;
        data[i * 3 + 2] = b >> 3;
    }
    RGB_Strip3_SetPixels(data, 7);
}