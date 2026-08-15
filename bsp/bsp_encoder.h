/*
 * encoder.h
 * EC11旋转编码器驱动
 */

#ifndef __ENCODER_H
#define __ENCODER_H

#include <stdint.h>

typedef enum {
    ENC_EVT_NONE,
    ENC_EVT_CW,         // 顺时针
    ENC_EVT_CCW,        // 逆时针
    ENC_EVT_CLICK,      // 单击
    ENC_EVT_LONG_PRESS, // 长按
} EncoderEvent_t;

void Encoder_Init(void);
EncoderEvent_t Encoder_GetEvent(void);
void Encoder_Process(void);

#endif /* __ENCODER_H */
