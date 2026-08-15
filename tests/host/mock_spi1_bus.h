#ifndef MOCK_SPI1_BUS_H
#define MOCK_SPI1_BUS_H

#include <stdint.h>

void MockSpi1_Reset(void);
void MockSpi1_SetJedecId(uint32_t jedec_id);
void MockSpi1_SetAcquireBusy(int busy);
uint32_t MockSpi1_GetProgramCount(void);
uint32_t MockSpi1_GetProgramAddress(uint32_t index);
uint32_t MockSpi1_GetProgramLength(uint32_t index);
uint32_t MockSpi1_GetEraseCount(void);
uint32_t MockSpi1_GetTftSelectCount(void);
uint32_t MockSpi1_GetTftTransferBytes(void);
uint32_t MockSpi1_GetTftMaxTransfer(void);
void MockSpi1_SetProgramFailure(int fail);
void MockSpi1_CorruptByte(uint32_t address, uint8_t xor_mask);

#endif /* MOCK_SPI1_BUS_H */
