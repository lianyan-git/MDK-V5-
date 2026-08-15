#ifndef BOOTLOADER_BUILD
#include "bsp_ntc.h"
#include "pin_config.h"
#include "stm32f10x.h"

// NTC: 100kΩ, B=3950, 10kΩ 上拉�?3.3V
// ADC_mV = 3300 * Rntc / (Rntc + 10000)
// 温度范围: -20°C ~ 120°C, 步进 5°C
static const int16_t ntc_lut[] = {
    3270, 3252, 3232, 3210, 3190,
    3168, 3142, 3112, 3078, 3000,
    2932, 2860, 2780, 2692, 2596,
    2494, 2386, 2274, 2160, 2044,
    1928, 1814, 1702, 1594, 1490,
    1392, 1298, 1210, 1130
};

void NTC_Init(void)
{
    ADC_InitTypeDef adc;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_ADC1 | RCC_APB2Periph_GPIOA, ENABLE);
    RCC_ADCCLKConfig(RCC_PCLK2_Div6);

    GPIO_InitTypeDef gpio;
    gpio.GPIO_Pin = PIN_NTC_PIN;
    gpio.GPIO_Mode = GPIO_Mode_AIN;
    GPIO_Init(PIN_NTC_PORT, &gpio);

    ADC_DeInit(ADC1);
    ADC_StructInit(&adc);
    adc.ADC_Mode = ADC_Mode_Independent;
    adc.ADC_ScanConvMode = DISABLE;
    adc.ADC_ContinuousConvMode = DISABLE;
    adc.ADC_ExternalTrigConv = ADC_ExternalTrigConv_None;
    adc.ADC_DataAlign = ADC_DataAlign_Right;
    adc.ADC_NbrOfChannel = 1;
    ADC_Init(ADC1, &adc);

    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_55Cycles5);

    ADC_Cmd(ADC1, ENABLE);
    ADC_ResetCalibration(ADC1);
    while (ADC_GetResetCalibrationStatus(ADC1));
    ADC_StartCalibration(ADC1);
    while (ADC_GetCalibrationStatus(ADC1));
}

uint16_t NTC_ReadADC(void)
{
    ADC_RegularChannelConfig(ADC1, ADC_Channel_2, 1, ADC_SampleTime_55Cycles5);
    ADC_SoftwareStartConvCmd(ADC1, ENABLE);
    while (!ADC_GetFlagStatus(ADC1, ADC_FLAG_EOC));
    return ADC_GetConversionValue(ADC1);
}

int16_t NTC_GetTemperature(void)
{
    uint16_t adc_val = NTC_ReadADC();
    uint16_t adc_mv = (uint32_t)adc_val * 3300 / 4096;
    uint16_t index;

    if (adc_mv >= ntc_lut[0]) return -200;
    if (adc_mv <= ntc_lut[28]) return 1200;

    for (index = 0; index < sizeof(ntc_lut)/sizeof(ntc_lut[0]) - 1; index++) {
        if (adc_mv >= ntc_lut[index + 1] && adc_mv <= ntc_lut[index]) {
            int16_t temp = (int16_t)(index * 5) - 200;
            uint16_t range = ntc_lut[index] - ntc_lut[index + 1];
            if (range > 0) {
                uint16_t frac = (adc_mv - ntc_lut[index + 1]) * 50 / range;
                temp += (int16_t)frac;
            }
            return temp;
        }
    }
    return 1200;
}

uint8_t NTC_IsOverTemp(void)
{
    int16_t temp = NTC_GetTemperature();
    return (temp >= (int16_t)(NTC_OVERTEMP_THRESHOLD * 10)) ? 1 : 0;
}
#endif /* BOOTLOADER_BUILD */

