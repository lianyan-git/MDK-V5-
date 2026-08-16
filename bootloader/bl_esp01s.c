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
static void send_upload_page(int conn_id);
static void send_no_content(int conn_id);

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
static char http_line[1536];
static uint16_t http_li = 0;
static uint8_t mp_state = 0;        /* 0=跳 multipart 头, 1=写固件, 2=尾部完成 */
static uint8_t mp_hdr = 0;          /* 匹配 \r\n\r\n */
static char mp_boundary[80];
static uint8_t mp_boundary_len = 0;
static uint8_t mp_bd_match = 0;     /* 尾部 boundary 匹配进度 */
static uint32_t body_len = 0;       /* Content-Length */
static uint32_t body_recv = 0;      /* 已接收 body 字节数 */

/* 流式 Content-Length 检测状态 */
static uint8_t clm = 0;
static uint8_t cl_read = 0;
static uint32_t cl_val = 0;

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
    /* 1. 等待 ESP 就绪（AT OK）。不再 AT+RST，避免 ESP 重启导致 AP 广播延迟、
     *    电脑 WiFi 扫描错过而搜不到热点（手机扫描快所以能搜到）。 */
    (void)ESP_WaitReady();
    ESP_SendCmd("ATE0", "OK", 300);

    /* 2. 设置 AP 模式 + WiFi 名称（加密）+ 固定信道 1，确保广播稳定 */
    ESP_SendCmd("AT+CWMODE=2", "OK", 500);
    ESP_SendCmd("AT+CWSAP=\"QiMingXing\",\"12345678\",1,4", "OK", 800);

    /* 3. 打开 TCP 服务器 */
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
    clm = 0;
    cl_read = 0;
    cl_val = 0;
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
        /* 写固件数据；检测尾部 "\r\n--" + boundary。
         * 匹配期间字节暂存，确认是 boundary 则丢弃，否则写回固件。 */
        static uint8_t bd_buf[96];
        static uint8_t bd_buf_len = 0;
        uint8_t is_boundary = 0;

        if (mp_boundary_len > 0 && mp_bd_match > 0) {
            /* 已在匹配中 */
            if (bd_buf_len < sizeof(bd_buf)) bd_buf[bd_buf_len++] = (uint8_t)c;
            const char *bd = mp_boundary;
            uint8_t expect;
            if (mp_bd_match == 1) expect = '\r';
            else if (mp_bd_match == 2) expect = '\n';
            else if (mp_bd_match == 3) expect = '-';
            else if (mp_bd_match == 4) expect = '-';
            else expect = (uint8_t)bd[mp_bd_match - 4];
            if (c == (char)expect) {
                mp_bd_match++;
                if (mp_bd_match >= (uint8_t)(4 + mp_boundary_len)) {
                    is_boundary = 1;   /* 完整匹配到 boundary，丢弃缓存 */
                    mp_state = 2;
                    mp_bd_match = 0;
                    bd_buf_len = 0;
                }
            } else {
                /* 匹配失败：把缓存字节写回固件 */
                for (uint8_t k = 0; k < bd_buf_len; k++) {
                    if (fw_write_addr < FIRMWARE_TEMP_ADDR + PLATFORM_FIRMWARE_ERASE_SIZE) {
                        write_fw_byte(bd_buf[k]);
                        fw_received++;
                        fw_total = fw_received;
                    }
                }
                bd_buf_len = 0;
                /* 当前字节 c 需重新按普通逻辑处理 */
                mp_bd_match = 0;
            }
        } else if (c == '\r' && mp_boundary_len > 0) {
            /* 开始匹配尾部边界 */
            mp_bd_match = 1;
            if (bd_buf_len < sizeof(bd_buf)) bd_buf[bd_buf_len++] = (uint8_t)c;
        } else {
            if (fw_write_addr < FIRMWARE_TEMP_ADDR + PLATFORM_FIRMWARE_ERASE_SIZE) {
                write_fw_byte((uint8_t)c);
                fw_received++;
                fw_total = fw_received;
            }
        }
        (void)is_boundary;
    }

    /* body 收完（Content-Length 达到）即完成 */
    if (body_recv >= body_len) {
        finish_upload(0);
        return 1;
    }
    return 0;
}

/* 大小写不敏感的子串查找（HTTP 头名不区分大小写） */
static char *mystr_casestr(const char *h, const char *n)
{
    if (!*n) return (char *)h;
    for (; *h; h++) {
        const char *p = h, *q = n;
        while (*q && *p) {
            char a = *p, b = *q;
            if (a >= 'A' && a <= 'Z') a += 32;
            if (b >= 'A' && b <= 'Z') b += 32;
            if (a != b) break;
            p++; q++;
        }
        if (!*q) return (char *)h;
    }
    return 0;
}

/* 处理一个 HTTP 层字节：流式检测 content-length / GET，body 阶段转 feed_fw_byte */
static void feed_http_byte(char c)
{
    static const char clpat[] = "content-length:";

    if (body_len == 0) {
        /* 累积 http_line（用于 GET 检测 + boundary 提取） */
        if (http_li < sizeof(http_line) - 1) http_line[http_li++] = c;
        http_line[http_li] = '\0';

        /* GET：打开网页 / favicon */
        if (strstr(http_line, "GET /favicon")) {
            send_no_content(0);
            http_li = 0;
            http_line[0] = '\0';
            return;
        }
        if (strstr(http_line, "GET / ") || strstr(http_line, "GET /index") || strstr(http_line, "GET / HTTP")) {
            send_upload_page(0);
            http_li = 0;
            http_line[0] = '\0';
            return;
        }

        /* 流式匹配 "content-length:"（大小写不敏感），不依赖 http_line 完整 */
        char cc = c;
        if (cc >= 'A' && cc <= 'Z') cc += 32;
        if (!cl_read) {
            if (cc == clpat[clm]) {
                clm++;
                if (clpat[clm] == '\0') {
                    cl_read = 1;
                    cl_val = 0;
                }
            } else {
                clm = (cc == clpat[0]) ? 1 : 0;
            }
        } else {
            if (cc >= '0' && cc <= '9') {
                cl_val = cl_val * 10U + (uint32_t)(cc - '0');
            } else if (cc == ' ') {
                /* 冒号后的空格，忽略 */
            } else {
                /* 数字结束 */
                if (cl_val > 0) {
                    body_len = cl_val;
                    fw_total = 0;
                    fw_received = 0;
                    fw_write_addr = FIRMWARE_TEMP_ADDR;
                    fw_crc = 0xFFFFFFFFU;
                    fw_buf_idx = 0;
                    body_recv = 0;
                    mp_state = 0;
                    mp_hdr = 0;
                    mp_bd_match = 0;
                    char *bd = mystr_casestr(http_line, "boundary=");
                    if (bd) {
                        bd += 9;
                        mp_boundary_len = 0;
                        while (*bd && *bd != '\r' && *bd != '\n' && mp_boundary_len < sizeof(mp_boundary)-1) {
                            mp_boundary[mp_boundary_len++] = *bd++;
                        }
                        mp_boundary[mp_boundary_len] = '\0';
                    }
                    transfer_state = 1;
                    http_li = 0;
                    http_line[0] = '\0';
                }
                cl_read = 0;
                clm = 0;
            }
        }

        if (http_li >= sizeof(http_line) - 1) {
            /* header 缓冲满：仅用于 GET/boundary 检测，满了循环覆盖 */
            http_li = 0;
            http_line[0] = '\0';
        }
    } else if (body_recv < body_len) {
        /* ── body 阶段：转 multipart 剥离 + 固件写入 ── */
        (void)feed_fw_byte(c);
    }
}

/* 返回 204 No Content（favicon 等请求，让浏览器立即完成） */
static void send_no_content(int conn_id)
{
    const char *resp = "HTTP/1.1 204 No Content\r\nConnection: close\r\n\r\n";
    char cmd[64];
    sprintf(cmd, "AT+CIPSEND=%d,%d", conn_id, (int)strlen(resp));
    ESP_Send(cmd);
    ESP_WaitResponse(">", 500);
    ESP_SendRaw(resp, strlen(resp));
}

static void send_upload_page(int conn_id)
{
    static const char *page =
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: text/html\r\n"
        "Connection: close\r\n\r\n"
        "<!DOCTYPE html><meta charset=UTF-8>"
        "<title>FW Update</title>"
        "<link rel=icon href=data:,>"
        "<style>body{font-family:Arial;margin:20px;background:#1a1a2e;color:#fff;text-align:center}"
        ".box{background:#16213e;border-radius:15px;padding:30px;max-width:400px;margin:auto}"
        "input{width:100%;padding:15px;margin:20px 0;background:#0f3460;border:2px dashed #e94560;color:#fff;box-sizing:border-box}"
        "button{background:#e94560;color:#fff;border:none;padding:15px 40px;border-radius:10px;font-size:18px;cursor:pointer}</style>"
        "<h2>Firmware Update</h2>"
        "<div class=box>"
        "<form method=POST action=/upload enctype=multipart/form-data>"
        "<input type=file name=firmware accept=.bin required>"
        "<button type=submit>UPLOAD & UPDATE</button>"
        "</form>"
        "<p>Uploading... screen shows progress, then device restarts</p>"
        "</div></html>";
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

    /* 屏幕状态由 bl_main 控制（DOWNLOAD OK / WRITE TO MCU），这里只更新进度 */
    BL_TFT_ShowProgressBar(100);
}

void BL_ESP01S_Process(void)
{
    static uint8_t in_ipd = 0;      /* 1=正在收 +IPD 分片数据 */
    static uint32_t ipd_remain = 0; /* 当前分片剩余数据字节 */
    static char pfx[20];
    static uint8_t pfx_len = 0;
    uint8_t byte;

    while (EspUart_ReadByte(&byte) != 0) {
        char c = (char)byte;

        /* ── 正在收 +IPD 分片数据：直接交给 HTTP 层 ── */
        if (in_ipd) {
            ipd_remain--;
            feed_http_byte(c);
            if (ipd_remain == 0) in_ipd = 0;
            continue;
        }

        /* ── 等待 +IPD,<id>,<len>: 前缀 ── */
        if (pfx_len == 0 && c != '+') continue;   /* 忽略非 + 开头 */

        if (pfx_len < 4) {
            /* 匹配 "+IPD" */
            static const char exp[] = "+IPD";
            if (c != exp[pfx_len]) { pfx_len = 0; continue; }
            pfx[pfx_len++] = c;
            continue;
        }
        if (pfx_len == 4) {
            if (c != ',') { pfx_len = 0; continue; }   /* 必须 ",",否则丢弃 */
            pfx[pfx_len++] = c;
            continue;
        }
        /* 收集 id,<len>，到 ':' 结束 */
        if (pfx_len < sizeof(pfx) - 1) pfx[pfx_len++] = c;
        pfx[pfx_len] = '\0';
        if (c == ':') {
            /* 解析 len：最后一个逗号后到冒号 */
            char *lc = pfx;
            char *tail = 0;
            while (*lc) { if (*lc == ',') tail = lc; lc++; }
            if (tail) {
                uint32_t len = 0;
                char *p = tail + 1;
                while (*p >= '0' && *p <= '9') { len = len * 10U + (uint32_t)(*p - '0'); p++; }
                ipd_remain = len;
                in_ipd = 1;
            }
            pfx_len = 0;
        } else if (pfx_len >= sizeof(pfx) - 1) {
            pfx_len = 0;   /* 前缀过长（非 +IPD），重置 */
        }
    }
}

static void Delay_ms(uint16_t ms)
{
    for (volatile uint32_t i = 0; i < ms * 7200; i++) {
        if ((i & 0x1FFFFU) == 0U) Watchdog_Kick();
    }
}