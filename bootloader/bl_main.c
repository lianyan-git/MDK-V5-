#include "bl_main.h"
#include "shared_defs.h"
#include "flash_ops.h"
#include "upgrade_flag.h"
#include "bl_tft.h"
#include "bl_esp01s.h"
#include "bsp_w25q128.h"
#include "bsp_esp_uart.h"
#include "board.h"
#include "pin_config.h"
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

static uint32_t fw_total = 0;
static uint32_t fw_crc = 0;
static uint32_t fw_start = 0;   /* 固件在外部 Flash 中的偏移（搜索向量表定位） */

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

static int verify_staged_image(void)
{
    uint8_t buf[FLASH_CHUNK];
    uint32_t offset = 0U;
    int found = 0;

    /* 在外部 Flash 中搜索固件向量表（SP=0x2000xxxx + PC=0x0800xxxx）定位固件开头。
     * 用户要求不校验大小/CRC，只要引导到指定地址。 */
    fw_start = 0;
    while (offset + 8 <= fw_total) {
        uint32_t read_len = offset + FLASH_CHUNK <= fw_total ? FLASH_CHUNK : (fw_total - offset);
        if (read_len > sizeof(buf)) read_len = sizeof(buf);
        Watchdog_Kick();
        if (W25Q128_Read(FIRMWARE_TEMP_ADDR + offset, buf, read_len) != W25Q128_OK) return -1;

        for (uint32_t i = 0; i + 8 <= read_len; i++) {
            uint32_t sp = (uint32_t)buf[i] | ((uint32_t)buf[i+1] << 8) | ((uint32_t)buf[i+2] << 16) | ((uint32_t)buf[i+3] << 24);
            uint32_t pc = (uint32_t)buf[i+4] | ((uint32_t)buf[i+5] << 8) | ((uint32_t)buf[i+6] << 16) | ((uint32_t)buf[i+7] << 24);
            if ((sp & 0x2FFE0000) == 0x20000000 && (pc & 0xFF000000) == 0x08000000) {
                fw_start = offset + i;
                found = 1;
                break;
            }
        }
        if (found) break;
        offset += read_len;
    }
    if (!found) return -1;   /* 没找到向量表：固件开头丢失 */

    /* 固件大小 = 固件开头到数据末尾（含尾部元数据，不影响引导）。
     * 限制不超过 APP 分区大小。 */
    fw_total = fw_total - fw_start;
    if (fw_total > APP_SIZE) fw_total = APP_SIZE;
    return 0;
}

static int flash_staged_to_app(void)
{
    uint8_t buf[FLASH_CHUNK];
    uint32_t offset = 0U;
    uint8_t last_pct = 0;

    /* 固件大小不超过 APP 分区（尾部可能含 multipart 元数据，截断到 APP_SIZE） */
    if (fw_total > APP_SIZE) fw_total = APP_SIZE;

    if (erase_app_area_with_progress() != 0) return -1;
    while (offset < fw_total) {
        uint32_t length = fw_total - offset;
        if (length > sizeof(buf)) length = sizeof(buf);
        Watchdog_Kick();
        if (W25Q128_Read(FIRMWARE_TEMP_ADDR + fw_start + offset, buf, length) != W25Q128_OK) return -1;
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

    /* 强制进入升级模式：App 内上电长按编码器已写 FORCE_BOOT 标志，
     * 复位后直接开 AP 收固件（覆盖所有其它判定）。 */
    if (have_flag && flag.status == UPGRADE_STATUS_FORCE_BOOT) {
        UpgradeFlag_Clear();
        BootloaderV2_EnterUpgradeMode();
    }

    /* 阶段2：检测到 DOWNLOADED → 拷贝模式（刷写 APP，显示进度），不碰 ESP。
     * 下载阶段完成时已写 flag（含 firmware_size/crc），重启后直接刷写。 */
    if (have_flag && flag.status == UPGRADE_STATUS_DOWNLOADED) {
        BootloaderV2_EnterCopyMode(&flag);
    }

    /* 阶段1：无有效 App（首次刷写/测试）→ 下载模式（开 AP 收固件） */
    int app_valid = (BootloaderV2_VerifyApp() == 0);
    if (!app_valid) {
        BootloaderV2_EnterUpgradeMode();
        /* 下载模式只在刷写成功并复位后才返回（NVIC_SystemReset），
         * 或校验失败重试 3 次后死循环——正常不会走到这里。 */
        app_valid = (BootloaderV2_VerifyApp() == 0);
    }

    if (app_valid) {
        BootloaderV2_JumpToApp();
    }
    BootloaderV2_ShowError();
    for (;;) {
        Watchdog_Kick();
    }
}

void BootloaderV2_EnterCopyMode(UpgradeFlag_t *flag)
{
    /* 拷贝模式不需要 ESP 联网：先初始化 SPI1（W25Q128_Init 会调用 Spi1Bus_Init），
     * 否则 BL_TFT_Init / verify_staged_image 的 SPI 操作都不工作 → 黑屏。 */
    W25Q128_SetServiceCallback(Watchdog_Kick);
    W25Q128_Init();

    /* 关闭 ESP01S 电源（P-MOS 高=关断）。注意：必须先初始化 ESP_EN 引脚为输出，
     * 否则 GPIO_SetBits 无效；EspUart_Init 会配置该引脚及 USART1。 */
    EspUart_Init();

    /* PA9（UART TX）改推挽输出低电平，主动拉低 ESP RX 引脚，
     * 克服 ESP 内部上拉电阻（40-50kΩ 到 VCC）导致的回灌供电。
     * 仅输入浮空不够，ESP 内部上拉会通过 RX 引脚向 VCC 灌电。 */
    {
        GPIO_InitTypeDef gpio;
        gpio.GPIO_Pin = PIN_ESP_TX_PIN;
        gpio.GPIO_Mode = GPIO_Mode_Out_PP;
        gpio.GPIO_Speed = GPIO_Speed_2MHz;
        GPIO_Init(PIN_ESP_TX_PORT, &gpio);
        GPIO_ResetBits(PIN_ESP_TX_PORT, PIN_ESP_TX_PIN);
    }

    EspUart_SetEnabled(0);

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
        fw_total = 0;
        fw_crc = 0;

        if (flash_ok && (stage_erase() != 0)) {
            flash_ok = 0;
        }

        BL_ESP01S_ResetTransfer();
        BL_TFT_ShowStatus("WAIT");

        uint32_t last_watchdog = SystemTime_Millis();
        uint32_t last_progress_tick = 0;
        uint32_t last_rx_time = 0;
        uint32_t last_recv = 0;
        uint8_t last_progress_step = 0;
        int transfer_done = 0;

        /* 上传循环：显示接收进度条（每 10% 刷新一次） */
        while (!transfer_done) {
            uint32_t now = SystemTime_Millis();
            if ((int32_t)(now - last_watchdog) >= 100) {
                Watchdog_Kick();
                last_watchdog = now;
            }

            BL_ESP01S_Process();

            /* 跟踪最后收到数据的时间（用于超时完成） */
            uint32_t recv_now = BL_ESP01S_GetReceivedSize();
            if (recv_now != last_recv) {
                last_recv = recv_now;
                last_rx_time = now;
            }

            int esp_state = BL_ESP01S_GetTransferState();
            if (esp_state == 1) {
                fw_total = BL_ESP01S_GetTotalSize();
                uint32_t recv = BL_ESP01S_GetReceivedSize();
                uint32_t total = BL_ESP01S_GetTotalFirmwareSize();   /* X-Total-Size 总字节数 */
                if (total > 0) {
                    uint8_t progress = (uint8_t)(recv * 100U / total);
                    uint8_t step = (uint8_t)(progress / 10);
                    if (step != last_progress_step && (int32_t)(now - last_progress_tick) > 500) {
                        last_progress_step = step;
                        last_progress_tick = now;
                        BL_TFT_ShowProgressBar(progress);
                        char buf[24];
                        sprintf(buf, "DL %d%%", progress);
                        BL_TFT_ShowStatus(buf);
                    }
                }
                /* 超时无新数据：如果数据已收全，直接完成（不需等浏览器 /done）；
                 * 否则中止重试。 */
                if (last_recv > 0 && (int32_t)(now - last_rx_time) > 10000) {
                    uint32_t exp = BL_ESP01S_GetTotalFirmwareSize();
                    if (exp > 0 && last_recv >= exp) {
                        BL_ESP01S_FinishTransfer();
                        transfer_done = 1;
                        break;
                    }
                    BL_ESP01S_AbortTransfer();
                    transfer_done = 1;
                    break;
                }
            } else if (esp_state == 2) {
                transfer_done = 1;
            } else if (esp_state < 0) {
                BL_TFT_ShowError("Upload Error!");
                Delay_ms(2000);
                break;
            }
        }

        if (!transfer_done) continue;
        if (BL_ESP01S_GetTransferState() < 0) {
            /* 超时/异常中止：残缺固件不得进入引导，直接重试 */
            BL_TFT_ShowError("Upload Error!");
            Delay_ms(2000);
            continue;
        }

        /* 上传完成：从 bl_esp01s 重新读取真实固件大小（multipart 剥离后的大小） */
        fw_total = BL_ESP01S_GetTotalSize();
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
            uint32_t clen = BL_ESP01S_GetTotalFirmwareSize();
            uint32_t got  = BL_ESP01S_GetReceivedSize();
            if (W25Q128_Read(FIRMWARE_TEMP_ADDR, v0, 8U) == W25Q128_OK) {
                sp = (uint32_t)v0[0] | ((uint32_t)v0[1] << 8) | ((uint32_t)v0[2] << 16) | ((uint32_t)v0[3] << 24);
                sprintf(buf, "CL%lu GOT%lu SP%08X", clen, got, sp);
            } else {
                sprintf(buf, "CL%lu GOT%lu RDERR", clen, got);
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
    /* 用户要求不校验大小/CRC，只验证向量表有效即可引导 */
    uint32_t sp = *(__IO uint32_t*)APP_ADDR;
    if ((sp & 0x2FFE0000) != 0x20000000) return -1;

    uint32_t pc = *(__IO uint32_t*)(APP_ADDR + 4);
    if ((pc & 0xFF000000) != 0x08000000) return -1;

    return 0;
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
    /* 跳转前关闭 ESP01S 电源。先拉低 PA9（UART TX）→ 主动拉低 ESP RX，
     * 克服 ESP 内部上拉电阻（40-50kΩ 到 VCC）导致的回灌供电。
     * 仅输入浮空不够——ESP 内部上拉会通过 RX 引脚向 VCC 灌电。 */
    {
        GPIO_InitTypeDef gpio;
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

        /* PA9 推挽输出低电平，克服 ESP RX 内部上拉 */
        gpio.GPIO_Pin = PIN_ESP_TX_PIN;
        gpio.GPIO_Mode = GPIO_Mode_Out_PP;
        gpio.GPIO_Speed = GPIO_Speed_2MHz;
        GPIO_Init(PIN_ESP_TX_PORT, &gpio);
        GPIO_ResetBits(PIN_ESP_TX_PORT, PIN_ESP_TX_PIN);

        /* ESP_EN 置高，关断 P-MOS */
        gpio.GPIO_Pin = PIN_ESP_EN_PIN;
        gpio.GPIO_Mode = GPIO_Mode_Out_PP;
        gpio.GPIO_Speed = GPIO_Speed_2MHz;
        GPIO_Init(PIN_ESP_EN_PORT, &gpio);
        GPIO_SetBits(PIN_ESP_EN_PORT, PIN_ESP_EN_PIN);
    }

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
    /* 每毫秒约 SystemCoreClock/10000 次循环（每条循环约 10 周期） */
    uint32_t per_ms = (SystemCoreClock + 9999U) / 10000U;
    for (volatile uint32_t i = 0; i < (uint32_t)ms * per_ms; i++) {
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