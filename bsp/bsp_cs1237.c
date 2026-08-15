#ifndef BOOTLOADER_BUILD
#include "bsp_cs1237.h"
#include "pin_config.h"
#include "stm32f10x.h"

void CS1237_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    g.GPIO_Pin = PIN_CS1237_CLK_PIN | PIN_CS1237_DATA_PIN;
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PIN_CS1237_CLK_PORT, &g);
    GPIO_ResetBits(PIN_CS1237_CLK_PORT, PIN_CS1237_CLK_PIN);
}

float CS1237_ReadWeight(void)
{
    return 0.0f;
}

void CS1237_Tare(void)
{
}
#endif /* BOOTLOADER_BUILD */