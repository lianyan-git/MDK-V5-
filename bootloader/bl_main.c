#include "bl_main.h"
#include "shared_defs.h"
#include "flash_ops.h"
#include "upgrade_flag.h"
#include "bl_tft.h"
#include "bl_esp01s.h"
#include "bsp_w25q128.h"
#include "bsp_esp_uart.h"
#include "board.h"
#include "system_time.h"
#include "stm32f10x.h"
#include <string.h>
#include <stdio.h>

static void Delay_ms(uint16_t ms);

#ifdef BOOTLOADER_BUILD
int main(void)
{
    BootloaderV2_Run();
    for (;;) { Watchdog_Kick(); }
}
#endif

#define MAX_RETRY                    3
#define ERASE_PROGRESS_PAGES        16
#define FLASH_CHUNK                 256U

static uint32_t fw_received = 0;
static uint32_t fw_total = 0;
static uint32_t fw_crc = 0;

static int erase_app_area_with_progress(void)
{
    uint32_t total_pages = APP_SIZE / FLASH_PAGE_SIZE;
    uint32_t done = 0;
    uint32_t addr;

    BL_TFT_ShowStatus("Erasing Flash...");
    for (addr = APP_ADDR; addr < APP_ADDR + APP_SIZE; addr += FLASH_PAGE_SIZE) {
        Watchdog_Kick();
        if (Flash_ErasePage(addr) != 0) return -1;
        done++;
        if ((done % ERASE_PROGRESS_PAGES) == 0) {
            uint8_t pct = (uint8_t)(done * 100U / total_pages);
            BL_TFT_ShowProgressBar(pct);
        }
    }
    BL_TFT_ShowProgressBar(100);
    return 0;
}

static int stage_erase(void)
{
    BL_TFT_ShowStatus("ERASE...");
    if (W25Q128_EraseRange(FIRMWARE_TEMP_ADDR, PLATFORM_FIRMWARE_ERASE_SIZE) != W25Q128_OK) {
        BL_TFT_ShowStatus("ERASE FAIL");
        return -1;
    }
    BL_TFT_ShowStatus("ERASE OK");
    return 0;
}

static uint32_t crc32_bytes(uint32_t crc, const uint8_t *data, uint32_t len)
{
    while (len-- != 0U) {
        crc ^= *data++;
        for (uint8_t j = 0; j < 8; j++) crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
    }
    return crc;
}

static int validate_firmware_vectors(uint32_t image_size)
{
    uint8_t v[8];
    uint32_t sp, pc;
    if (W25Q128_Read(FIRMWARE_TEMP_ADDR, v, 8U) != W25Q128_OK) return -1;
    sp = (uint32_t)v[0] | ((uint32_t)v[1] << 8) | ((uint32_t)v[2] << 16) | ((uint32_t)v[3] << 24);
    pc = (uint32_t)v[4] | ((uint32_t)v[5] << 8) | ((uint32_t)v[6] << 16) | ((uint32_t)v[7] << 24);
    (void)image_size;
    if ((sp & 0x2FFE0000) != 0x20000000) return -1;
    if ((pc & 0xFF000000) != 0x08000000) return -1;
    return 0;
}

static int verify_staged_image(void)
{
    uint8_t buf[FLASH_CHUNK];
    uint32_t offset = 0U;
    uint32_t crc = 0xFFFFFFFFU;

    if (fw_total > APP_SIZE) return -1;
    if (validate_firmware_vectors(fw_total) != 0) return -1;
    while (offset < fw_total) {
        uint32_t length = fw_total - offset;
        if (length > sizeof(buf)) length = sizeof(buf);
        Watchdog_Kick();
        if (W25Q128_Read(FIRMWARE_TEMP_ADDR + offset, buf, length) != W25Q128_OK) return -1;
        crc = crc32_bytes(crc, buf, length);
        offset += length;
    }
    crc = ~crc;
    return (crc == fw_crc) ? 0 : -1;
}

static int flash_staged_to_app(void)
{
    uint8_t buf[FLASH_CHUNK];
    uint32_t offset = 0U;
    uint8_t last_pct = 0;

    if (erase_app_area_with_progress() != 0) return -1;
    while (offset < fw_total) {
        uint32_t length = fw_total - offset;
        if (length > sizeof(buf)) length = sizeof(buf);
        Watchdog_Kick();
        if (W25Q128_Read(FIRMWARE_TEMP_ADDR + offset, buf, length) != W25Q128_OK) return -1;
        if (Flash_Write(APP_ADDR + offset, buf, length) != 0) return -1;
        offset += length;
        /* 拷贝进度：每 10% 刷新一次 */
        uint8_t pct = (uint8_t)(offset * 100U / fw_total);
        if (pct != last_pct && (pct % 10) == 0) {
            last_pct = pct;
            BL_TFT_ShowProgressBar(pct);
        }
    }
    BL_TFT_ShowProgressBar(100);
    return 0;
}

void BootloaderV2_Run(void)
{
    Board_EarlyInit();

    /* 关键：从 RCC 寄存器读回实际时钟源，纠正 SystemCoreClock。
     * 若 HSE/PLL 未锁住 72MHz（比如晶振路径有异常），系统实为 HSI 8MHz，
     * 但 SystemCoreClock 编译默认是 72MHz —— 不纠正的话 USART 波特率全错。
     * 必须在 SystemTime_Init / EspUart_Init 之前调用。 */
    SystemCoreClockUpdate();

    SystemTime_Init();
    Watchdog_Init();   /* 约 4 秒窗口 */

    UpgradeFlag_t flag;
    int have_flag = (UpgradeFlag_Read(&flag) == 0);

    /* 阶段2：检测到 DOWNLOADED → 拷贝模式（刷写 APP，显示进度），不碰 ESP。
     * 下载阶段完成时已写 flag（含 firmware_size/crc），重启后直接刷写。 */
    if (have_flag && flag.status == UPGRADE_STATUS_DOWNLOADED) {
        BootloaderV2_EnterCopyMode(&flag);
    }

    /* 阶段1：无有效 App（首次刷写/测试）→ 下载模式（开 AP 收固件） */
    if (BootloaderV2_VerifyApp() != 0) {
        BootloaderV2_EnterUpgradeMode();
    }

    if (BootloaderV2_VerifyApp() == 0) {
        BootloaderV2_JumpToApp();
    }

    BootloaderV2_ShowError();
    for (;;) {
        Watchdog_Kick();
    }
}

void BootloaderV2_EnterCopyMode(UpgradeFlag_t *flag)
{
    /* 显示完整界面 + 标题 */
    BL_TFT_Init();
    BL_TFT_ShowUpgradeScreen();
    {
        const char *ip = BL_ESP01S_GetIP();
        BL_TFT_ShowAPInfo("QiMingXing", "12345678", ip);
    }
    BL_TFT_ShowStatus("WRITE TO MCU");

    /* 从 flag 读取固件大小和 CRC */
    fw_total = flag->firmware_size;
    fw_crc = flag->firmware_crc32;

    /* 校验外部 Flash 固件 */
    if (verify_staged_image() != 0) {
        BL_TFT_ShowError("Verify Failed!");
        UpgradeFlag_Clear();
        for (;;) Watchdog_Kick();
    }

    /* 刷写 APP（内部实现更新进度条） */
    if (flash_staged_to_app() == 0 && BootloaderV2_VerifyFirmware() == 0) {
        BL_TFT_ShowStatus("UPDATE OK!");
        UpgradeFlag_Clear();
        Delay_ms(2000);
        NVIC_SystemReset();
    }

    BL_TFT_ShowError("Copy Failed!");
    UpgradeFlag_Clear();
    for (;;) Watchdog_Kick();
}

void BootloaderV2_EnterUpgradeMode(void)
{
    int retry;
    int flash_ok = 0;

    /* ── 先开 AP（最优先，不依赖屏幕/外部Flash）────────────────── */
    BL_ESP01S_Init();
    EspUart_SetEnabled(1);   /* P-MOS: 低=开，确保 ESP01S 供电 */
    BL_ESP01S_StartAP();     /* 等 AT OK → CWMODE/CWSAP/CIPSERVER */

    /* ── 之后才初始化外部Flash（用于上传暂存）。失败不再死等，仅记录。 */
    W25Q128_SetServiceCallback(Watchdog_Kick);
    if (W25Q128_Init() == W25Q128_OK) {
        flash_ok = 1;
    }

    /* ── 初始化屏幕：直接显示完整页面，状态文字表示当前阶段 ── */
    BL_TFT_Init();
    BL_TFT_ShowUpgradeScreen();
    {
        const char *ip = BL_ESP01S_GetIP();
        BL_TFT_ShowAPInfo("QiMingXing", "12345678", ip);
    }
    BL_TFT_ShowStatus("DOWNLOAD TO QFLASH");

    for (retry = 0; retry < MAX_RETRY; retry++) {
        fw_received = 0;
        fw_total = 0;
        fw_crc = 0;

        if (flash_ok && (stage_erase() != 0)) {
            flash_ok = 0;
        }

        BL_ESP01S_ResetTransfer();
        BL_TFT_ShowProgressBar(0);
        BL_TFT_ShowStatus("WAIT");

        uint32_t last_progress_tick = 0;
        uint32_t last_watchdog = SystemTime_Millis();
        uint8_t last_progress_step = 0;   /* 已显示的 10% 档位 */
        uint8_t shown_wait = 0;
        int transfer_done = 0;

        while (!transfer_done) {
            uint32_t now = SystemTime_Millis();
            if ((int32_t)(now - last_watchdog) >= 100) {
                Watchdog_Kick();
                last_watchdog = now;
            }

            BL_ESP01S_Process();

            int esp_state = BL_ESP01S_GetTransferState();
            if (esp_state == 1) {
                fw_received = BL_ESP01S_GetReceivedSize();
                fw_total = BL_ESP01S_GetTotalSize();
                if (fw_total > 0) {
                    uint8_t progress = (uint8_t)(fw_received * 100U / fw_total);
                    uint8_t step = (uint8_t)(progress / 10);
                    /* 每达成 10% 才刷新一次屏幕，减少与 Flash 写入的 SPI 干涉 */
                    if (step != last_progress_step && (int32_t)(now - last_progress_tick) > 500) {
                        last_progress_step = step;
                        last_progress_tick = now;
                        BL_TFT_ShowProgressBar(progress);
                        char buf[24];
                        sprintf(buf, "DL %d%%", progress);
                        BL_TFT_ShowStatus(buf);   /* 状态区大字显示进度 */
                    }
                    shown_wait = 1;
                }
            } else if (esp_state == 2) {
                transfer_done = 1;
            } else if (esp_state < 0) {
                BL_TFT_ShowError("Upload Error!");
                Delay_ms(2000);
                break;
            } else {
                /* 一直没进入接收：提示等待 */
                if (!shown_wait && (int32_t)(now - last_progress_tick) > 3000) {
                    last_progress_tick = now;
                    BL_TFT_ShowStatus("NO DATA...");
                }
            }
        }

        if (!transfer_done) continue;

        /* 上传完成：从 bl_esp01s 重新读取真实固件大小（multipart 剥离后的大小） */
        fw_total = BL_ESP01S_GetTotalSize();
        fw_received = BL_ESP01S_GetReceivedSize();
        fw_crc = BL_ESP01S_GetFirmwareCrc32();

        BL_TFT_ShowStatus("DOWNLOAD OK");
        Watchdog_Kick();

        if (verify_staged_image() == 0) {
            /* 下载完成：显示完整界面 + 进度 100%，写入升级标志后复位。
             * 重启后进入拷贝阶段（刷写 APP，显示拷贝进度）。 */
            BL_TFT_ShowUpgradeScreen();
            BL_TFT_ShowProgressBar(100);
            BL_TFT_ShowStatus("Download OK");
            Watchdog_Kick();

            UpgradeFlag_t uf;
            memset(&uf, 0, sizeof(uf));
            uf.magic = UPGRADE_MAGIC;
            uf.version = 0x00020000;
            uf.status = UPGRADE_STATUS_DOWNLOADED;
            uf.firmware_size = fw_total;
            uf.firmware_crc32 = fw_crc;
            uf.target_addr = APP_ADDR;
            uf.timestamp = SystemTime_Millis();
            UpgradeFlag_Write(&uf);
            UpgradeFlag_WriteExt(&uf);

            BL_TFT_ShowStatus("Restarting...");
            Delay_ms(2000);
            NVIC_SystemReset();
            return;
        } else {
            /* 校验失败：显示关键数值便于定位 */
            uint8_t v0[8];
            char buf[48];
            uint32_t sp;
            if (W25Q128_Read(FIRMWARE_TEMP_ADDR, v0, 8U) == W25Q128_OK) {
                sp = (uint32_t)v0[0] | ((uint32_t)v0[1] << 8) | ((uint32_t)v0[2] << 16) | ((uint32_t)v0[3] << 24);
                sprintf(buf, "VFAIL %lu SP%08X", fw_total, sp);
            } else {
                sprintf(buf, "VFAIL %lu RDERR", fw_total);
            }
            BL_TFT_ShowStatus(buf);
            Delay_ms(5000);
            BL_TFT_ShowError("Verify Failed!");
            if (retry < MAX_RETRY - 1) {
                BL_TFT_ShowStatus("Retrying...");
                Delay_ms(1500);
            }
        }
    }

    BL_TFT_ShowError("Update Failed!");
    for (;;) {
        Watchdog_Kick();
    }
}

int BootloaderV2_WriteFirmware(uint32_t offset, uint8_t *data, uint16_t len)
{
    if (offset + len > APP_SIZE) return -1;
    return Flash_Write(APP_ADDR + offset, data, len);
}

int BootloaderV2_VerifyFirmware(void)
{
    uint32_t sp = *(__IO uint32_t*)APP_ADDR;
    if ((sp & 0x2FFE0000) != 0x20000000) return -1;

    uint32_t pc = *(__IO uint32_t*)(APP_ADDR + 4);
    if ((pc & 0xFF000000) != 0x08000000) return -1;

    uint8_t buf[FLASH_CHUNK];
    uint32_t offset = 0U;
    uint32_t crc = 0xFFFFFFFFU;
    while (offset < fw_total) {
        uint32_t length = fw_total - offset;
        if (length > sizeof(buf)) length = sizeof(buf);
        Watchdog_Kick();
        crc = crc32_bytes(crc, (const uint8_t*)(APP_ADDR + offset), length);
        offset += length;
    }
    crc = ~crc;
    return (crc == fw_crc) ? 0 : -1;
}

int BootloaderV2_VerifyApp(void)
{
    uint32_t sp = *(__IO uint32_t*)APP_ADDR;
    if ((sp & 0x2FFE0000) != 0x20000000) return -1;

    uint32_t pc = *(__IO uint32_t*)(APP_ADDR + 4);
    if ((pc & 0xFF000000) != 0x08000000) return -1;

    return 0;
}

__asm void set_msp(uint32_t sp)
{
    MSR msp, r0
    BX lr
}

void BootloaderV2_JumpToApp(void)
{
    __disable_irq();
    RCC_DeInit();
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;
    SCB->VTOR = APP_ADDR;

    uint32_t sp = *(__IO uint32_t*)APP_ADDR;
    uint32_t pc = *(__IO uint32_t*)(APP_ADDR + 4);

    set_msp(sp);
    { void (*jump)(void) = (void (*)(void))pc; jump(); }
}

void BootloaderV2_ShowError(void)
{
    BL_TFT_Init();
    BL_TFT_ShowError("No Valid APP!");
}

static void Delay_ms(uint16_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 7200; i++) {
        if ((i & 0x1FFFFU) == 0U) Watchdog_Kick();
    }
}

uint32_t CRC32_Calculate(uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ (0xEDB88320 & -(crc & 1));
        }
    }
    return ~crc;
}