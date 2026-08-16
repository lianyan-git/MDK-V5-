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
static void finish_upload(int conn_id);

#define FW_BUF_SIZE         256
#define AT_TIMEOUT_MS       800
#define AT_RETRY_MAX        1
#define AT_READY_MAX        6        /* 等待 ESP 就绪的最大尝试次数 */
#define AT_READY_POLL_MS    300      /* 每次尝试前的间隔 */

static uint8_t fw_buffer[FW_BUF_SIZE];
static uint16_t fw_buf_idx = 0;

static uint32_t fw_received = 0;
static uint32_t fw_total = 0;
static uint32_t fw_write_addr = FIRMWARE_TEMP_ADDR;
static uint32_t fw_crc = 0xFFFFFFFFU;

static char ap_ip[16] = "192.168.4.1";

static int transfer_state = 0;
static int transfer_error = 0;

/* ── HTTP/multipart 上传解析状态 ── */
static char http_line[512];
static uint16_t http_li = 0;
static uint8_t mp_state = 0;        /* 0=跳 multipart 头, 1=写固件, 2=尾部完成 */
static uint8_t mp_hdr = 0;          /* 匹配 \r\n\r\n */
static char mp_boundary[80];
static uint8_t mp_boundary_len = 0;
static uint8_t mp_bd_match = 0;     /* 尾部 boundary 匹配进度 */
static uint32_t body_len = 0;       /* Content-Length */
static uint32_t body_recv = 0;      /* 已接收 body 字节数 */
static uint8_t ipd_probe[16];       /* 探测 +IPD,id,len: 前缀 */
static uint8_t ipd_probe_len = 0;

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
    body_len = 0;
    body_recv = 0;
    mp_state = 0;
    mp_hdr = 0;
    mp_boundary_len = 0;
    mp_bd_match = 0;
    ipd_probe_len = 0;
    http_li = 0;
    http_line[0] = '\0';
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

/* 处理一个固件 body 字节（含 multipart 剥离）。
 * 返回 1 表示上传完成。 */
static int feed_fw_byte(char c)
{
    body_recv++;

    if (mp_state == 0) {
        /* 跳过 multipart 头：--boundary\r\n ... \r\n\r\n */
        if (c == '\r' && mp_hdr == 0) mp_hdr = 1;
        else if (c == '\n' && mp_hdr == 1) mp_hdr = 2;
        else if (c == '\r' && mp_hdr == 2) mp_hdr = 3;
        else if (c == '\n' && mp_hdr == 3) { mp_hdr = 0; mp_state = 1; }
        else mp_hdr = 0;
    } else if (mp_state == 1) {
        /* 写固件数据；检测尾部 "\r\n--" + boundary */
        if (mp_boundary_len > 0) {
            const char *bd = mp_boundary;
            if (mp_bd_match == 0 && c == '\r') mp_bd_match = 1;
            else if (mp_bd_match == 1 && c == '\n') mp_bd_match = 2;
            else if (mp_bd_match == 2 && c == '-') mp_bd_match = 3;
            else if (mp_bd_match == 3 && c == '-') mp_bd_match = 4;
            else if (mp_bd_match >= 4 && mp_bd_match < (uint8_t)(4 + mp_boundary_len)) {
                if (c == bd[mp_bd_match - 4]) mp_bd_match++;
                else mp_bd_match = 0;
            } else {
                mp_bd_match = 0;
            }
            if (mp_bd_match >= (uint8_t)(4 + mp_boundary_len)) {
                mp_state = 2;   /* 已到尾部 boundary，停止写固件 */
                mp_bd_match = 0;
            }
        }
        if (mp_state == 1) {
            if (fw_write_addr < FIRMWARE_TEMP_ADDR + PLATFORM_FIRMWARE_ERASE_SIZE) {
                write_fw_byte((uint8_t)c);
                fw_received++;
                fw_total = fw_received;
            }
        }
    }

    /* body 收完（Content-Length 达到）即完成 */
    if (body_recv >= body_len) {
        finish_upload(0);
        return 1;
    }
    return 0;
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

    /* multipart body 含 boundary 头尾；真实固件大小 = 已写入的固件字节数 */
    fw_total = fw_received;
    transfer_state = 2;

    const char *resp = "HTTP/1.1 200 OK\r\n\r\nUpdate OK! Restarting...";
    char cmd[64];
    sprintf(cmd, "AT+CIPSEND=%d,%d", conn_id, (int)strlen(resp));
    ESP_Send(cmd);
    ESP_WaitResponse(">", 1000);
    ESP_SendRaw(resp, strlen(resp));

    BL_TFT_ShowProgressBar(100);
    BL_TFT_ShowProgressText("100% - Complete!");
    BL_TFT_ShowStatus("Restarting...");
}

void BL_ESP01S_Process(void)
{
    uint8_t byte;

    while (EspUart_ReadByte(&byte) != 0) {
        char c = (char)byte;

        /* ════ 固件上传数据模式（POST body 接收中） ════ */
        if (body_len > 0 && body_recv < body_len) {
            /* 跳过 ESP 分片前缀 "+IPD,<id>,<len>:"（可能出现在分片开头） */
            if (ipd_probe_len < sizeof(ipd_probe)) {
                ipd_probe[ipd_probe_len++] = (uint8_t)c;
                if (ipd_probe_len > 4) {
                    static const char prefix[] = "+IPD,";
                    if (memcmp(ipd_probe, prefix, 4) == 0) {
                        if (c == ':') { ipd_probe_len = 0; }   /* 前缀结束 */
                        continue;
                    }
                }
                if (ipd_probe_len >= 4 && memcmp(ipd_probe, "+IPD", 4) != 0) {
                    /* 前 4 字节不是 +IPD：回放为真实数据 */
                    for (uint8_t k = 0; k < ipd_probe_len; k++) {
                        if (feed_fw_byte((char)ipd_probe[k])) break;
                    }
                    ipd_probe_len = 0;
                }
                continue;
            }
            ipd_probe_len = 0;
            if (feed_fw_byte(c)) { /* 完成 */ }
            continue;
        }

        /* 上传完成后的残余字节：丢弃（等新的请求行） */
        if (body_len > 0 && body_recv >= body_len) {
            continue;
        }

        /* ════ HTTP header 模式（解析请求行） ════ */
        if (http_li < sizeof(http_line) - 1) http_line[http_li++] = c;
        http_line[http_li] = '\0';

        if (strstr(http_line, "GET / ") || strstr(http_line, "GET /index") || strstr(http_line, "GET / HTTP")) {
            send_upload_page(0);
            http_li = 0;
            http_line[0] = '\0';
            continue;
        }
        if (strstr(http_line, "POST /upload")) {
            char *cl = strstr(http_line, "Content-Length: ");
            if (cl) {
                sscanf(cl, "Content-Length: %lu", &body_len);
                fw_total = 0;
                fw_received = 0;
                fw_write_addr = FIRMWARE_TEMP_ADDR;
                fw_crc = 0xFFFFFFFFU;
                fw_buf_idx = 0;
                body_recv = 0;
                mp_state = 0;
                mp_hdr = 0;
                mp_bd_match = 0;
                ipd_probe_len = 0;
                char *bd = strstr(http_line, "boundary=");
                if (bd) {
                    bd += 9;
                    mp_boundary_len = 0;
                    while (*bd && *bd != '\r' && *bd != '\n' && mp_boundary_len < sizeof(mp_boundary)-1) {
                        mp_boundary[mp_boundary_len++] = *bd++;
                    }
                    mp_boundary[mp_boundary_len] = '\0';
                }
                /* 仅在 Content-Length 解析成功后才进入数据接收 */
                transfer_state = 1;
            }
            http_li = 0;
            http_line[0] = '\0';
            continue;
        }

        /* 其它内容（+IPD、CLOSED、SEND OK 等）直接忽略，等待请求行 */
        continue;
    }
}

static void Delay_ms(uint16_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 7200; i++) {
        if ((i & 0x1FFFFU) == 0U) Watchdog_Kick();
    }
}