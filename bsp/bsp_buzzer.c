#ifndef BOOTLOADER_BUILD
#include "bsp_buzzer.h"
#include "pin_config.h"
#include "stm32f10x.h"

void Buzzer_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);

    g.GPIO_Pin = PIN_BUZZER_PIN;
    g.GPIO_Mode = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PIN_BUZZER_PORT, &g);

    TIM_TimeBaseInitTypeDef t;
    t.TIM_Prescaler = 71;
    t.TIM_Period = 999;
    t.TIM_ClockDivision = 0;
    t.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &t);

    TIM_OCInitTypeDef o;
    o.TIM_OCMode = TIM_OCMode_PWM1;
    o.TIM_OutputState = TIM_OutputState_Enable;
    o.TIM_Pulse = 0;
    o.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init(TIM3, &o);
    TIM_OC4PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_Cmd(TIM3, ENABLE);
}

void Buzzer_Beep(uint16_t ms)
{
    TIM_SetCompare4(TIM3, 500);
    volatile uint32_t delay = (uint32_t)ms * 7200;
    while (delay--) { __NOP(); }
    TIM_SetCompare4(TIM3, 0);
}

void Buzzer_SetFreq(uint16_t freq)
{
    (void)freq;
}
#endif /* BOOTLOADER_BUILD */