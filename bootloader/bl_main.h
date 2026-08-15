#ifndef __BOOTLOADER_V2_H
#define __BOOTLOADER_V2_H

#include <stdint.h>

void BootloaderV2_Run(void);
void BootloaderV2_EnterUpgradeMode(void);
int BootloaderV2_WriteFirmware(uint32_t offset, uint8_t *data, uint16_t len);
int BootloaderV2_VerifyFirmware(void);
int BootloaderV2_VerifyApp(void);
void BootloaderV2_ShowError(void);
void BootloaderV2_JumpToApp(void);

#endif
