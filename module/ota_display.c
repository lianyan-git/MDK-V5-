#include "ota_display.h"

#include <stddef.h>

static TftStatus_t draw_label(uint16_t y, const char *label, const char *value,
                              uint16_t value_color)
{
    TftStatus_t result;

    result = TFT_DrawString(5U, y, label, TFT_COLOR_GRAY,
                            TFT_COLOR_BLACK, 1U);
    if (result != TFT_OK) return result;
    return TFT_DrawString(47U, y, value, value_color,
                          TFT_COLOR_BLACK, 1U);
}

TftStatus_t OtaDisplay_Init(void)
{
    TftStatus_t result = TFT_Init();
    if (result != TFT_OK) return result;
    result = TFT_DrawString(17U, 12U, "DRYER OTA", TFT_COLOR_CYAN,
                            TFT_COLOR_BLACK, 2U);
    if (result != TFT_OK) return result;
    return TFT_DrawString(23U, 34U, "WEB UPDATE", TFT_COLOR_WHITE,
                          TFT_COLOR_BLACK, 1U);
}

TftStatus_t OtaDisplay_ShowNetwork(const char *current_version,
                                   const char *target_version,
                                   const char *ssid,
                                   const char *ip_address)
{
    TftStatus_t result;

    if ((current_version == NULL) || (target_version == NULL) ||
        (ssid == NULL) || (ip_address == NULL)) return TFT_ERROR_ARGUMENT;
    result = TFT_FillRect(0U, 55U, TFT_WIDTH, 95U, TFT_COLOR_BLACK);
    if (result != TFT_OK) return result;
    result = draw_label(58U, "NOW:", current_version, TFT_COLOR_WHITE);
    if (result != TFT_OK) return result;
    result = draw_label(76U, "NEW:", target_version, TFT_COLOR_YELLOW);
    if (result != TFT_OK) return result;
    result = draw_label(104U, "AP:", ssid, TFT_COLOR_CYAN);
    if (result != TFT_OK) return result;
    return draw_label(122U, "IP:", ip_address, TFT_COLOR_GREEN);
}

TftStatus_t OtaDisplay_ShowStatus(const char *status)
{
    TftStatus_t result;

    if (status == NULL) return TFT_ERROR_ARGUMENT;
    result = TFT_FillRect(0U, 154U, TFT_WIDTH, 22U, TFT_COLOR_DARKGRAY);
    if (result != TFT_OK) return result;
    return TFT_DrawString(5U, 161U, status, TFT_COLOR_WHITE,
                          TFT_COLOR_DARKGRAY, 1U);
}

TftStatus_t OtaDisplay_ShowProgress(uint8_t percent)
{
    char text[5];
    uint8_t index = 0U;
    TftStatus_t result;

    if (percent > 100U) percent = 100U;
    result = TFT_DrawProgress(8U, 190U, 119U, 18U, percent,
                              TFT_COLOR_GREEN, TFT_COLOR_DARKGRAY);
    if (result != TFT_OK) return result;
    if (percent >= 100U) text[index++] = '1';
    if (percent >= 10U) text[index++] = (char)('0' + ((percent / 10U) % 10U));
    text[index++] = (char)('0' + (percent % 10U));
    text[index++] = '%';
    text[index] = '\0';
    return TFT_DrawString(52U, 215U, text, TFT_COLOR_WHITE,
                          TFT_COLOR_BLACK, 1U);
}

TftStatus_t OtaDisplay_ShowError(const char *error)
{
    TftStatus_t result;

    if (error == NULL) return TFT_ERROR_ARGUMENT;
    result = TFT_FillRect(0U, 154U, TFT_WIDTH, 86U, TFT_COLOR_RED);
    if (result != TFT_OK) return result;
    result = TFT_DrawString(5U, 162U, "ERROR", TFT_COLOR_WHITE,
                            TFT_COLOR_RED, 2U);
    if (result != TFT_OK) return result;
    return TFT_DrawString(5U, 190U, error, TFT_COLOR_WHITE,
                          TFT_COLOR_RED, 1U);
}
