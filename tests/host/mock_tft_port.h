#ifndef MOCK_TFT_PORT_H
#define MOCK_TFT_PORT_H

#include <stdint.h>

void MockTftPort_Reset(void);
int MockTftPort_GetBacklight(void);
int MockTftPort_GetReset(void);
uint32_t MockTftPort_GetDelayMs(void);

#endif /* MOCK_TFT_PORT_H */
