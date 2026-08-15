#include "bl_esp01s.h"
#include "bl_tft.h"
#include "bsp_esp_uart.h"
#include "bsp_w25q128.h"
#include "board.h"
#include "shared_defs.h"
#include "pin_config.h"
#include "system_time.h"
#include "stm32f10x.h"
#include <string.h>
#include <stdio.h>

static void Delay_ms(uint16_t ms);

#define FW_BUF_SIZE         256
#define AT_TIMEOUT_MS       800
#define AT_RETRY_MAX        1
#define AT_READY_MAX        6        /* 等待 ESP 就绪的最大尝试次数 */
#define AT_READY_POLL_MS    300      /* 每次尝试前的间隔 */

static uint8_t fw_buffer[FW_BUF_SIZE];
static uint8_t fw_buf_idx = 0;

static uint32_t fw_received = 0;
static uint32_t fw_total = 0;
static uint32_t fw_write_addr = FIRMWARE_TEMP_ADDR;
static uint32_t fw_crc = 0xFFFFFFFFU;

static char ap_ip[16] = "192.168.4.1";

static int transfer_state = 0;
static int transfer_error = 0;

static void ESP_Send(const char *s)
{
    EspUart_Write((const uint8_t*)s, strlen(s), 1000);
    uint8_t crlf[] = {'\r', '\n'};
    EspUart_Write(crlf, 2, 1000);
}

static void ESP_SendRaw(const char *s, uint16_t len)
{
    EspUart_Write((const uint8_t*)s, len, 1000);
}

static void ESP_FlushRX(void)
{
    EspUart_ClearRx();
}

static int ESP_WaitResponse(const char *expected, uint32_t timeout_ms)
{
    char buf[80];
    uint16_t bi = 0;
    uint32_t start = SystemTime_Millis();
    uint8_t byte;

    while ((int32_t)(SystemTime_Millis() - start) < (int32_t)timeout_ms) {
        if (EspUart_ReadByte(&byte) != 0) {
            if (bi < sizeof(buf) - 1) buf[bi++] = (char)byte;
            if (bi >= sizeof(buf)) { bi = 0; }
            buf[bi] = '\0';
            if (strstr(buf, expected)) return 0;
            if (strstr(buf, "ERROR")) return -1;
        }
        Watchdog_Kick();
    }
    return -1;
}

static int ESP_SendCmd(const char *cmd, const char *expected, uint32_t timeout_ms)
{
    int retry;
    for (retry = 0; retry < AT_RETRY_MAX; retry++) {
        ESP_FlushRX();
        ESP_Send(cmd);
        if (ESP_WaitResponse(expected, timeout_ms) == 0) return 0;
    }
    return -1;
}

/* 等待 ESP 就绪：反复发 AT 直到收到 OK。不中途断电，避免多余重启。 */
static int ESP_WaitReady(void)
{
    int attempts;
    for (attempts = 0; attempts < AT_READY_MAX; attempts++) {
        Delay_ms(AT_READY_POLL_MS);
        ESP_FlushRX();
        if (ESP_SendCmd("AT", "OK", AT_TIMEOUT_MS) == 0) {
            return 0;
        }
    }
    return -1;
}

void BL_ESP01S_Init(void)
{
    /* 与 app 固件完全相同的 USART1 初始化：
     * 由 StdPeriph 库根据实际时钟自动计算波特率，不硬编码 BRR */
    EspUart_Init();
}

void BL_ESP01S_StartAP(void)
{
    /* 1. 等待 ESP 就绪（AT OK） */
    (void)ESP_WaitReady();

    /* 2. 重置 ESP（AT+RST，ESP 会重启） */
    ESP_SendCmd("AT+RST", "OK", 1000);
    Delay_ms(500);

    /* 3. 重置后重新等待 AT OK */
    (void)ESP_WaitReady();
    ESP_SendCmd("ATE0", "OK", 300);

    /* 4. 设置 WiFi 名称（加密）+ 等 OK */
    ESP_SendCmd("AT+CWMODE=2", "OK", 500);
    ESP_SendCmd("AT+CWSAP=\"QiMingXing\",\"12345678\",1,4", "OK", 800);

    /* 5. 打开 AP 模式（TCP 服务器） */
    ESP_SendCmd("AT+CIPMUX=1", "OK", 500);
    ESP_SendCmd("AT+CIPSERVER=1,80", "OK", 800);

    ESP_FlushRX();
    ESP_Send("AT+CIFSR");
    ESP_WaitResponse("+CIFSR", 800);
}

const char* BL_ESP01S_GetIP(void)
{
    return ap_ip;
}

void BL_ESP01S_ResetTransfer(void)
{
    fw_received = 0;
    fw_total = 0;
    fw_write_addr = FIRMWARE_TEMP_ADDR;
    fw_crc = 0xFFFFFFFFU;
    fw_buf_idx = 0;
    transfer_state = 0;
    transfer_error = 0;
}

int BL_ESP01S_GetTransferState(void)
{
    if (transfer_error) return -1;
    if (transfer_state == 1) return 1;
    if (transfer_state == 2) return 2;
    return 0;
}

uint32_t BL_ESP01S_GetReceivedSize(void) { return fw_received; }
uint32_t BL_ESP01S_GetTotalSize(void) { return fw_total; }
uint32_t BL_ESP01S_GetFirmwareCrc32(void) { return ~fw_crc; }

static void flush_fw_buffer(void)
{
    if (fw_buf_idx > 0) {
        if (W25Q128_Write(fw_write_addr, fw_buffer, fw_buf_idx) != W25Q128_OK) {
            transfer_error = 1;
            return;
        }
        for (uint32_t i = 0; i < fw_buf_idx; i++) {
            fw_crc ^= fw_buffer[i];
            for (uint8_t j = 0; j < 8; j++) fw_crc = (fw_crc >> 1) ^ (0xEDB88320 & -(fw_crc & 1));
        }
        fw_write_addr += fw_buf_idx;
        fw_buf_idx = 0;
    }
}

static void write_fw_byte(uint8_t byte)
{
    fw_buffer[fw_buf_idx++] = byte;
    if (fw_buf_idx >= FW_BUF_SIZE) {
        flush_fw_buffer();
    }
}

static void send_upload_page(int conn_id)
{
    static const char *page =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<!DOCTYPE html>"
        "<html><head><meta charset=\"UTF-8\">"
        "<title>Dryer Firmware Update</title>"
        "<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
        "<style>"
        "body{font-family:Arial;margin:0;padding:20px;background:#1a1a2e;color:#fff;text-align:center;}"
        ".box{background:#16213e;border-radius:15px;padding:30px;margin:20px auto;max-width:400px;}"
        "h1{color:#e94560;font-size:24px;}"
        "input[type=file]{width:100%;padding:15px;margin:20px 0;background:#0f3460;border:2px dashed #e94560;border-radius:10px;color:#fff;box-sizing:border-box;}"
        "button{background:#e94560;color:#fff;border:none;padding:15px 40px;border-radius:10px;font-size:18px;cursor:pointer;}"
        "button:hover{background:#c13651;}"
        ".info{color:#aaa;margin-top:20px;font-size:14px;}"
        "</style></head>"
        "<body>"
        "<h1>Firmware Update</h1>"
        "<div class=\"box\">"
        "<p>Select firmware file (.bin)</p>"
        "<form method=\"POST\" action=\"/upload\" enctype=\"multipart/form-data\">"
        "<input type=\"file\" name=\"firmware\" accept=\".bin\" required>"
        "<button type=\"submit\">UPLOAD & UPDATE</button>"
        "</form>"
        "<div class=\"info\">Device will restart automatically after update</div>"
        "</div></body></html>";
    char cmd[64];
    sprintf(cmd, "AT+CIPSEND=%d,%d", conn_id, (int)strlen(page));
    ESP_Send(cmd);
    /* 等待 ESP 返回 ">" 提示符后再发送数据 */
    ESP_WaitResponse(">", 500);
    ESP_SendRaw(page, strlen(page));
}

static void finish_upload(int conn_id)
{
    (void)conn_id;
    flush_fw_buffer();

    const char *resp = "HTTP/1.1 200 OK\r\n\r\nUpdate OK! Restarting...";
    char cmd[64];
    sprintf(cmd, "AT+CIPSEND=%d,%d", conn_id, (int)strlen(resp));
    ESP_Send(cmd);
    ESP_WaitResponse(">", 1000);
    ESP_SendRaw(resp, strlen(resp));

    BL_TFT_ShowProgressBar(100);
    BL_TFT_ShowProgressText("100% - Complete!");
    BL_TFT_ShowStatus("Restarting...");

    transfer_state = 2;
}

void BL_ESP01S_Process(void)
{
    static char line[512];
    static uint16_t li = 0;
    static uint8_t in_data = 0;
    static uint32_t data_len = 0;
    static uint32_t data_received = 0;
    static int conn_id = 0;
    static int pending_ipd = 0;
    uint8_t byte;

    while (EspUart_ReadByte(&byte) != 0) {
        char c = (char)byte;

        if (!in_data) {
            /* 解析 AT 主动上报：+IPD,<id>,<len>:<data> 以及透传前的其他提示 */
            if (pending_ipd) {
                /* 已经过了 ":", 后续字节是 HTTP 数据，拼接进 line */
                if (li < (int)sizeof(line) - 1) line[li++] = c;
                line[li] = '\0';

                if (strstr(line, "GET / ") || strstr(line, "GET /index") || strstr(line, "GET / HTTP")) {
                    send_upload_page(conn_id);
                    li = 0;
                    pending_ipd = 0;
                    continue;
                }
                if (strstr(line, "POST /upload")) {
                    char *cl = strstr(line, "Content-Length: ");
                    if (cl) {
                        sscanf(cl, "Content-Length: %lu", &fw_total);
                        data_len = fw_total;
                        data_received = 0;
                        fw_received = 0;
                        fw_write_addr = FIRMWARE_TEMP_ADDR;
                        fw_crc = 0xFFFFFFFFU;
                        fw_buf_idx = 0;
                        transfer_state = 1;
                    }
                    /* 进入数据接收模式：之后所有字节都是固件内容 */
                    in_data = 1;
                    li = 0;
                    pending_ipd = 0;
                    continue;
                }
                continue;
            }

            if (c == '+') {
                /* 可能是 +IPD 开头，也可能 +CIFSR 等。缓存，等 ':'
                 * 出现时确认是 +IPD 数据包 */
                if (li < (int)sizeof(line) - 1) line[li++] = c;
                line[li] = '\0';
                if (strstr(line, "+IPD")) {
                    pending_ipd = 1;
                    li = 0;
                }
                continue;
            }

            if (c == ':') {
                /* +IPD,<id>,<len>: 前缀结束，提取 conn_id（第二个逗号前） */
                char *p1 = strstr(line, "+IPD,");
                if (p1) {
                    int id = 0;
                    p1 += 5;
                    while (*p1 >= '0' && *p1 <= '9') { id = id * 10 + (*p1 - '0'); p1++; }
                    conn_id = id;
                    pending_ipd = 1;
                    li = 0;
                }
                continue;
            }

            if (c >= '0' && c <= '9' || c == ',' || c == 'I' || c == 'P' ||
                c == 'C' || c == 'S' || c == 'E' || c == 'D') {
                /* +IPD 前缀的字符，收集到 line 里等待 ': ' 判定 */
                if (li < (int)sizeof(line) - 1) line[li++] = c;
                line[li] = '\0';
                continue;
            }
        } else {
            if (data_received < data_len && fw_write_addr < FIRMWARE_TEMP_ADDR + PLATFORM_FIRMWARE_ERASE_SIZE) {
                write_fw_byte((uint8_t)c);
                data_received++;
                fw_received = data_received;
            }

            if (data_received >= data_len) {
                finish_upload(conn_id);
                in_data = 0;
                li = 0;
                pending_ipd = 0;
            }
        }
    }
}

static void Delay_ms(uint16_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 7200; i++) {
        if ((i & 0x1FFFFU) == 0U) Watchdog_Kick();
    }
}