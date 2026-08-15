#ifndef BOOTLOADER_BUILD
#include "bsp_aht20.h"
#include "stm32f10x.h"

int AHT20_Init(void)
{
    return 0;
}

int AHT20_Read(float *temperature, float *humidity)
{
    *temperature = 25.0f;
    *humidity = 50.0f;
    return 0;
}
#endif /* BOOTLOADER_BUILD */