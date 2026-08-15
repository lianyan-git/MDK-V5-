/*
 * bl_tft.h
 * Bootloader TFT驱动 - 精简版
 * 只支持基本显示功能，代码量极小
 */

#ifndef __BL_TFT_H
#define __BL_TFT_H

#include <stdint.h>

// 初始化
void BL_TFT_Init(void);

// 显示启动Logo
void BL_TFT_ShowBootLogo(void);

// 显示升级界面
void BL_TFT_ShowUpgradeScreen(void);

// 显示状态文本
void BL_TFT_ShowStatus(const char *text);

// 显示AP信息
void BL_TFT_ShowAPInfo(const char *ssid, const char *pass, const char *ip);

// 显示进度条
void BL_TFT_ShowProgressBar(uint8_t percent);

// 显示进度文本
void BL_TFT_ShowProgressText(const char *text);

// 显示错误
void BL_TFT_ShowError(const char *text);

// 清屏
void BL_TFT_Clear(uint16_t color);

// 画字符串
void BL_TFT_DrawString(uint16_t x, uint16_t y, const char *str, uint16_t color, uint8_t size);

// 画单个字符
void BL_TFT_DrawChar(uint16_t x, uint16_t y, char c, uint16_t color, uint8_t size);

// 填充矩形
void BL_TFT_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

#endif /* __BL_TFT_H */
