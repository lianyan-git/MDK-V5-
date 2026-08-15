#include "bsp_tft_port.h"

#include "pin_config.h"

void TftPort_Init(void)
{
    GPIO_InitTypeDef gpio;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA |
                           RCC_APB2Periph_GPIOB |
                           RCC_APB2Periph_GPIOC, ENABLE);
    gpio.GPIO_Mode = GPIO_Mode_Out_PP;
    gpio.GPIO_Speed = GPIO_Speed_2MHz;

    gpio.GPIO_Pin = PIN_TFT_DC_PIN;
    GPIO_Init(PIN_TFT_DC_PORT, &gpio);
    gpio.GPIO_Pin = PIN_TFT_RES_PIN;
    GPIO_Init(PIN_TFT_RES_PORT, &gpio);
    gpio.GPIO_Pin = PIN_TFT_BL_PIN;
    GPIO_Init(PIN_TFT_BL_PORT, &gpio);

    GPIO_ResetBits(PIN_TFT_DC_PORT, PIN_TFT_DC_PIN);
    GPIO_SetBits(PIN_TFT_RES_PORT, PIN_TFT_RES_PIN);
    GPIO_ResetBits(PIN_TFT_BL_PORT, PIN_TFT_BL_PIN);
}

void TftPort_SetDataMode(int data_mode)
{
    if (data_mode) GPIO_SetBits(PIN_TFT_DC_PORT, PIN_TFT_DC_PIN);
    else GPIO_ResetBits(PIN_TFT_DC_PORT, PIN_TFT_DC_PIN);
}

void TftPort_SetReset(int high)
{
    if (high) GPIO_SetBits(PIN_TFT_RES_PORT, PIN_TFT_RES_PIN);
    else GPIO_ResetBits(PIN_TFT_RES_PORT, PIN_TFT_RES_PIN);
}

void TftPort_SetBacklight(int enabled)
{
    if (enabled) GPIO_SetBits(PIN_TFT_BL_PORT, PIN_TFT_BL_PIN);
    else GPIO_ResetBits(PIN_TFT_BL_PORT, PIN_TFT_BL_PIN);
}

void TftPort_DelayMs(uint32_t delay_ms)
{
    volatile uint32_t count;
    uint32_t loops_per_ms = SystemCoreClock / 8000U;

    while (delay_ms-- != 0U) {
        for (count = 0U; count < loops_per_ms; ++count) {
            __NOP();
        }
    }
}
