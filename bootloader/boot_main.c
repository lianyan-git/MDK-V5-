#include "stm32f10x.h"

#include "board.h"
#include "boot_jump.h"
#include "boot_recovery.h"
#include "boot_updater.h"
#include "bsp_rgb_led.h"
#include "bsp_w25q128.h"
#include "ota_display.h"
#include "platform_contract.h"
#include "system_time.h"

void boot_main_entry(void);

#ifdef BOOTLOADER_BUILD
int main(void)
{
    boot_main_entry();
    return 0;
}
#endif

static void set_rgb_by_progress(uint8_t percent)
{
    static uint8_t rgb_data[3];

    if (percent < 30) {
        rgb_data[0] = 0; rgb_data[1] = 0; rgb_data[2] = 20;
    } else if (percent < 60) {
        rgb_data[0] = 20; rgb_data[1] = 10; rgb_data[2] = 0;
    } else if (percent < 100) {
        rgb_data[0] = 20; rgb_data[1] = 20; rgb_data[2] = 0;
    } else {
        rgb_data[0] = 0; rgb_data[1] = 20; rgb_data[2] = 0;
    }
    RGB_Strip2_SetPixels(rgb_data, 3);
}

static void show_progress(void *context, uint8_t percent)
{
    (void)context;
    (void)OtaDisplay_ShowStatus("INSTALLING");
    (void)OtaDisplay_ShowProgress(percent);
    set_rgb_by_progress(percent);
}

void boot_main_entry(void)
{
    BootUpdateStatus_t update_status;
    BootRecoveryAction_t action;

    SCB->VTOR = PLATFORM_BOOT_ADDR;
    Board_EarlyInit();
    SystemTime_Init();
    Watchdog_Init();
    Board_ForceHeaterOff();

    RGB_Strip_Init();
    set_rgb_by_progress(0);

    W25Q128_SetServiceCallback(Watchdog_Kick);
    (void)W25Q128_Init();
    (void)OtaDisplay_Init();
    (void)OtaDisplay_ShowStatus("CHECK UPDATE");
    update_status = BootUpdater_Run(show_progress, 0);
    action = BootRecovery_Decide(update_status, BootJump_IsApplicationValid());
    (void)OtaDisplay_ShowStatus(BootRecovery_StatusText(update_status));

    if (update_status == BOOT_UPDATE_UPDATED) {
        set_rgb_by_progress(100);
    } else {
        set_rgb_by_progress(0);
    }

    if (action == BOOT_RECOVERY_JUMP_APP) {
        Board_ForceHeaterOff();
        (void)BootJump_ToApplication();
    }
    if (action == BOOT_RECOVERY_NO_VALID_APP) {
        (void)OtaDisplay_ShowError("NO VALID APP");
    } else {
        (void)OtaDisplay_ShowError("POWER RETRY");
    }
    for (;;) {
        Board_ForceHeaterOff();
        Watchdog_Kick();
    }
}
