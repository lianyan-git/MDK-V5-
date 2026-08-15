#ifndef BOARD_H
#define BOARD_H

void Board_EarlyInit(void);
void Board_Init(void);
void Board_ForceHeaterOff(void);
void Board_NTC_Init(void);

void Watchdog_Init(void);
void Watchdog_Kick(void);

#endif /* BOARD_H */
