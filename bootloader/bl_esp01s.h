#ifndef __BL_ESP01S_H
#define __BL_ESP01S_H

#include <stdint.h>

void BL_ESP01S_Init(void);
void BL_ESP01S_StartAP(void);
const char* BL_ESP01S_GetIP(void);

void BL_ESP01S_Process(void);
void BL_ESP01S_ResetTransfer(void);
void BL_ESP01S_FinishTransfer(void);
void BL_ESP01S_AbortTransfer(void);

int BL_ESP01S_GetTransferState(void);
uint32_t BL_ESP01S_GetReceivedSize(void);
uint32_t BL_ESP01S_GetTotalSize(void);
uint32_t BL_ESP01S_GetBodyLen(void);
uint32_t BL_ESP01S_GetTotalFirmwareSize(void);
uint32_t BL_ESP01S_GetFirmwareCrc32(void);

#endif