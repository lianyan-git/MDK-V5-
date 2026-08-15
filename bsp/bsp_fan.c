#ifndef BOOTLOADER_BUILD
#include "bsp_fan.h"
#include "pin_config.h"
#include "stm32f10x.h"

static uint8_t fan_current = 0;

void Fan_Init(void)
{
    GPIO_InitTypeDef g;
    g.GPIO_Pin = PIN_FAN_PWM_PIN;
    g.GPIO_Mode = GPIO_Mode_AF_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PIN_FAN_PWM_PORT, &g);

    TIM_OCInitTypeDef o;
    o.TIM_OCMode = TIM_OCMode_PWM1;
    o.TIM_OutputState = TIM_OutputState_Enable;
    o.TIM_Pulse = 0;
    o.TIM_OCPolarity = TIM_OCPolarity_High;
    TIM_OC4Init(FAN_PWM_TIM, &o);
    TIM_OC4PreloadConfig(FAN_PWM_TIM, TIM_OCPreload_Enable);

    fan_current = 0;
}

void Fan_SetSpeed(uint8_t percent)
{
    if (percent > 100) percent = 100;
    fan_current = percent;
    TIM_SetCompare4(FAN_PWM_TIM, (percent * 1000) / 100);
}

uint8_t Fan_GetSpeed(void)
{
    return fan_current;
}

void Fan_AdjustSpeed(int8_t delta)
{
    int16_t new_speed = (int16_t)fan_current + delta;
    if (new_speed < 0) new_speed = 0;
    if (new_speed > 100) new_speed = 100;
    Fan_SetSpeed((uint8_t)new_speed);
}

void Fan_Off(void)
{
    Fan_SetSpeed(0);
}
#endif /* BOOTLOADER_BUILD */

