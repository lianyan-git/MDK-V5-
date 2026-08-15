#ifndef SYSTEM_TIME_H
#define SYSTEM_TIME_H

#include <stdint.h>

#define SYSTEM_TIME_TICK_HZ 1000U

void SystemTime_Init(void);
uint32_t SystemTime_Millis(void);
void SystemTime_TickISR(void);

#endif /* SYSTEM_TIME_H */
