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

/* 上电兜底：长按编码器按键（PB5，按下为低电平）直接进入下载模式，
 * 无论 App 是否有效都生效，避免 OTA 写入坏固件后“变砖”无法再进升级界面。 */
static int EncoderButton_HeldAtBoot(void)
{
    GPIO_InitTypeDef gpio;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
    gpio.GPIO_Pin  = PIN_ENC_BTN_PIN;
    gpio.GPIO_Mode = GPIO_Mode_IPU;   /* 内部上拉，按下为低电平 */
    gpio.GPIO_Speed = GPIO_Speed_2MHz;
    GPIO_Init(PIN_ENC_BTN_PORT, &gpio);
    uint32_t low = 0U;
    for (volatile uint32_t i = 0; i < 4000U; i++) {
        if (GPIO_ReadInputDataBit(PIN_ENC_BTN_PORT, PIN_ENC_BTN_PIN) == 0) low++;
    }
    return (low > 2000U) ? 1 : 0;
}

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

/* 校验失败诊断：记录计算 CRC / 期望 CRC / 覆盖长度，便于定位写坏还是读坏 */
static uint32_t g_dbg_crc_calc = 0;
static uint32_t g_dbg_crc_exp  = 0;
static uint32_t g_dbg_crc_len  = 0;

/* 重算暂存镜像 [0, len) 的 CRC32（与 verify 同算法），用于二次读取比对读稳定性 */
static uint32_t staged_crc_recompute(uint32_t len)
{
    uint8_t buf[FLASH_CHUNK];
    uint32_t crc = 0xFFFFFFFFU;
    uint32_t rd = 0U;
    uint32_t rem = len;
    while (rem > 0U) {
        uint32_t rl = (rem > sizeof(buf)) ? sizeof(buf) : rem;
        Watchdog_Kick();
        if (W25Q128_Read(FIRMWARE_TEMP_ADDR + rd, buf, rl) != W25Q128_OK) return 0U;
        for (uint32_t i = 0; i < rl; i++) {
            crc ^= buf[i];
            for (uint8_t j = 0; j < 8; j++)
                crc = (crc >> 1) ^ (0xEDB88320U & -(crc & 1));
        }
        rd += rl; rem -= rl;
    }
    return ~crc;
}

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
    W25Q128_ClearProtection();   /* 清除状态寄存器 BP 写保护位，避免页编程被硬件忽略 */
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
    uint32_t orig_total = fw_total;   /* 握手/整包原始大小，CRC 覆盖 [0, orig_total) */

    /* 先在外部 Flash 中搜索固件向量表（SP=0x2000xxxx + PC=0x0800xxxx）定位固件开头。 */
    fw_start = 0;
    while (offset + 8 <= orig_total) {
        uint32_t read_len = offset + FLASH_CHUNK <= orig_total ? FLASH_CHUNK : (orig_total - offset);
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

    fw_total = orig_total - fw_start;
    if (fw_total > APP_SIZE) fw_total = APP_SIZE;

    /* 整包 CRC32 校验：重算 [0, orig_total) 的 CRC32，与 ESP 上报的 fw_crc 比对。
     * UART 段已有 CRC16+ACK、存储写出错会返回错误，二者都不会静默损坏；
     * 只有 WiFi 上传或存储落地环节可能污染体部而保留向量表，这里能拦住，
     * 避免“下载成功、校验通过”却被拷成坏固件导致跳转黑屏。 */
    {
        uint32_t crc = 0xFFFFFFFFU;
        uint32_t rd = 0U;
        uint32_t rem = orig_total;
        while (rem > 0U) {
            uint32_t rl = (rem > sizeof(buf)) ? sizeof(buf) : rem;
            Watchdog_Kick();
            if (W25Q128_Read(FIRMWARE_TEMP_ADDR + rd, buf, rl) != W25Q128_OK) return -1;
            for (uint32_t i = 0; i < rl; i++) {
                crc ^= buf[i];
                for (uint8_t j = 0; j < 8; j++)
                    crc = (crc >> 1) ^ (0xEDB88320U & -(crc & 1));
            }
            rd += rl;
            rem -= rl;
        }
        if ((~crc) != fw_crc) {
            g_dbg_crc_calc = ~crc;      /* 计算值 */
            g_dbg_crc_exp  = fw_crc;    /* ESP 上报期望值 */
            g_dbg_crc_len  = orig_total;
            return -1;   /* CRC 不匹配：固件在传输/存储中损坏，拒绝引导 */
        }
    }
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

    /* 上电初 HSE/PLL 可能未完全稳定，延时 200ms 让时钟稳定后再读 Flash */
    Delay_ms(200);

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

    /* 先验证 App 有效性（重试 5 次，避免上电时钟不稳误判），
     * 再检查编码器（避免编码器 GPIO 初始化影响 Flash 读取）。 */
    {
        int app_valid = 0;
        int retry;
        for (retry = 0; retry < 5; retry++) {
            if (BootloaderV2_VerifyApp() == 0) { app_valid = 1; break; }
            Delay_ms(10);
        }
        if (app_valid) {
            BootloaderV2_JumpToApp();
        }
    }

    /* 兜底：上电长按编码器（PB5）强制进入下载模式，坏固件也不会变砖。 */
    if (EncoderButton_HeldAtBoot()) {
        BootloaderV2_EnterUpgradeMode();
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

    /* ── 先初始化屏幕（SPI1 + TFT），再操作 ESP ─────────────
     * ESP_WaitReady 是阻塞握手，若 ESP 异常会卡住；屏幕必须在此之前就绪，
     * 否则永远黑屏、无法显示升级界面。 */
    W25Q128_SetServiceCallback(Watchdog_Kick);
    if (W25Q128_Init() == W25Q128_OK) {
        flash_ok = 1;
    }
    BL_TFT_Init();
    BL_TFT_ShowUpgradeScreen();

    /* 初始化 ESP 串口并上电（P-MOS: 低=开）。OTA 触发放在重试循环内，
     * 失败后重试，避免 ESP 启动慢导致一次就失败。 */
    BL_ESP01S_Init();
    EspUart_SetEnabled(1);   /* P-MOS: 低=开，确保 ESP01S 供电 */

    for (retry = 0; retry < MAX_RETRY; retry++) {
        fw_total = 0;
        fw_crc = 0;

        if (flash_ok && (stage_erase() != 0)) {
            flash_ok = 0;
        }

        BL_ESP01S_ResetTransfer();
        BL_TFT_ShowStatus("WAIT");

        /* 通知 ESP 进入 OTA 模式（自定义固件开 AP + 网页，并回 OK），
         * 之后 BL_ESP01S_Process 按二进制 OTA 协议接收固件：
         * 握手 -> 1KB/包(带 CRC16 包级 ACK) -> 总 CRC32 -> 完成。 */
        if (BL_ESP01S_StartOta() != 0) {
            BL_TFT_ShowError("ESP No AT");
            Delay_ms(2000);
            continue;
        }
        {
            const char *ip = BL_ESP01S_GetIP();
            BL_TFT_ShowAPInfo("QiMingXing", "12345678", ip);
        }
        BL_TFT_ShowStatus("DOWNLOAD TO QFLASH");

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

            /* 固件已收完：由单片机主动让 ESP 进入休眠（AT+OTACLOSE），
             * 关闭网页/AP 进入低功耗，而不是让 ESP 自己决定。失败也无害，
             * 拷贝阶段会硬断电 ESP。 */
            BL_ESP01S_CloseWeb();

            BL_TFT_ShowStatus("Restarting...");
            Delay_ms(2000);
            NVIC_SystemReset();
            return;
        } else {
            /* 校验失败诊断：重算 CRC 两次比对读稳定性；EXP=期望 GOT=计算值。
             * 若两次重算不同 → 读不稳定（SPI 读路径问题）；
             * 若相同但与 EXP 不同 → 确为写坏（或存储落地损坏）。 */
            uint32_t crcA = g_dbg_crc_calc;
            uint32_t crcB = staged_crc_recompute(g_dbg_crc_len);
            uint32_t clen = BL_ESP01S_GetTotalFirmwareSize();
            uint32_t got  = BL_ESP01S_GetReceivedSize();
            char buf[32];
            if (crcA != crcB) {
                sprintf(buf, "RD UNSTABLE");
                BL_TFT_ShowStatus(buf);
            } else {
                sprintf(buf, "EXP%08X", (unsigned int)g_dbg_crc_exp);
                BL_TFT_ShowStatus(buf);
            }
            Delay_ms(2500);
            if (crcA != crcB) {
                BL_TFT_ShowError("Verify Failed!");
            } else {
                sprintf(buf, "GOT%08X L%lu", (unsigned int)crcA, (unsigned long)clen);
                BL_TFT_ShowError(buf);
            }
            Delay_ms(3500);
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