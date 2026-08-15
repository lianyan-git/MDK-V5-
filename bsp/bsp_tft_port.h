#ifndef BSP_TFT_PORT_H
#define BSP_TFT_PORT_H

#include <stdint.h>

void TftPort_Init(void);
void TftPort_SetDataMode(int data_mode);
void TftPort_SetReset(int high);
void TftPort_SetBacklight(int enabled);
void TftPort_DelayMs(uint32_t delay_ms);

#endif /* BSP_TFT_PORT_H */
