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
static void send_upload_page(int conn_id);
static void send_no_content(int conn_id);
static void send_200(int conn_id);

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

/* ── HTTP 上传解析状态 ── */
static char http_line[1536];
static uint16_t http_li = 0;
static uint32_t body_len = 0;       /* Content-Length */
static uint32_t body_recv = 0;      /* 已接收 body 字节数 */

/* 流式 Content-Length / X-Total-Size 检测状态 */
static uint8_t clm = 0;
static uint8_t cl_read = 0;
static uint32_t cl_val = 0;
static uint8_t tsm = 0;              /* "x-total-size:" 匹配进度 */
static uint8_t ts_read = 0;
static uint32_t ts_val = 0;
static uint32_t total_expected = 0;  /* 浏览器上报的文件总字节数 */
static uint8_t hdr_crlf = 0;         /* header 结束符 \r\n\r\n 匹配进度 */

static int esp_conn_id = 0;   /* 当前 +IPD 连接的 id（分块上传响应用） */
static uint8_t ipd_skip_pending = 0;  /* GET 处理完后，跳过当前 +IPD 分片剩余字节 */

/* +IPD 分片解析状态（文件级：ResetTransfer 也要清理，防止跨传输残留吃掉下一请求） */
static uint8_t in_ipd = 0;      /* 1=正在收 +IPD 分片数据 */
static uint32_t ipd_remain = 0; /* 当前分片剩余数据字节 */
static char pfx[24];
static uint8_t pfx_len = 0;

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
    clm = 0;
    cl_read = 0;
    cl_val = 0;
    tsm = 0;
    ts_read = 0;
    ts_val = 0;
    total_expected = 0;
    hdr_crlf = 0;
    http_li = 0;
    http_line[0] = '\0';
    esp_conn_id = 0;
    ipd_skip_pending = 0;
    in_ipd = 0;
    ipd_remain = 0;
    pfx_len = 0;
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
uint32_t BL_ESP01S_GetBodyLen(void) { return body_len; }
uint32_t BL_ESP01S_GetTotalFirmwareSize(void) { return total_expected; }
uint32_t BL_ESP01S_GetFirmwareCrc32(void) { return ~fw_crc; }

static void flush_fw_buffer(void)
{
    if (fw_buf_idx > 0) {
        W25Q128_Status_t st = W25Q128_Write(fw_write_addr, fw_buffer, fw_buf_idx);
        if (st != W25Q128_OK) {
            transfer_error = 1;
            fw_buf_idx = 0;   /* 失败重置，避免越界写 fw_buffer */
            char dbg[32];
            sprintf(dbg, "W%d@%06lX", (int)st, (unsigned long)fw_write_addr);
            BL_TFT_ShowStatus(dbg);
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

/* 处理一个固件 body 字节（分块上传：块内容写入外部 Flash）。
 * 返回 1 表示当前块接收完成。 */
static int feed_fw_byte(char c)
{
    body_recv++;

    if (fw_write_addr < FIRMWARE_TEMP_ADDR + PLATFORM_FIRMWARE_ERASE_SIZE) {
        write_fw_byte((uint8_t)c);
        fw_received++;
        fw_total = fw_received;
    }

    /* 当前块收完（Content-Length 达到）：flush，等待下一块或 /done */
    if (body_recv >= body_len) {
        flush_fw_buffer();
        body_len = 0;
        body_recv = 0;
        return 1;
    }
    return 0;
}

/* 处理一个 HTTP 层字节：流式检测 content-length / x-total-size / GET，
 * 头结束符 \r\n\r\n 后才进入 body 阶段（分块上传：块内容写入外部 Flash）。 */
static void feed_http_byte(char c)
{
    static const char clpat[] = "content-length:";
    static const char tspat[] = "x-total-size:";

    if (body_len == 0) {
        /* ── header 阶段 ── */
        if (http_li < sizeof(http_line) - 1) http_line[http_li++] = c;
        http_line[http_li] = '\0';

        /* GET：打开网页 / favicon / 分块上传完成 */
        if (strstr(http_line, "GET /done")) {
            /* 分块上传全部完成 */
            flush_fw_buffer();
            if (total_expected > 0 && fw_received != total_expected) {
                transfer_error = 1;   /* 数据不完整：禁止进入引导 */
            } else {
                fw_total = fw_received;
                transfer_state = 2;
            }
            send_200(esp_conn_id);
            http_li = 0;
            http_line[0] = '\0';
            ipd_skip_pending = 1;
            return;
        }
        if (strstr(http_line, "GET /favicon")) {
            send_no_content(esp_conn_id);
            http_li = 0;
            http_line[0] = '\0';
            ipd_skip_pending = 1;
            return;
        }
        if (strstr(http_line, "GET / ") || strstr(http_line, "GET /index") || strstr(http_line, "GET / HTTP")) {
            send_upload_page(esp_conn_id);
            http_li = 0;
            http_line[0] = '\0';
            ipd_skip_pending = 1;
            return;
        }

        /* 流式匹配 "content-length:"（大小写不敏感） */
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
                cl_read = 0;
                clm = 0;
            }
        }

        /* 流式匹配 "x-total-size:"（上传总字节数，用于整体进度） */
        if (!ts_read) {
            if (cc == tspat[tsm]) {
                tsm++;
                if (tspat[tsm] == '\0') {
                    ts_read = 1;
                    ts_val = 0;
                }
            } else {
                tsm = (cc == tspat[0]) ? 1 : 0;
            }
        } else {
            if (cc >= '0' && cc <= '9') {
                ts_val = ts_val * 10U + (uint32_t)(cc - '0');
            } else if (cc == ' ') {
                /* 冒号后的空格，忽略 */
            } else {
                if (ts_val > 0) total_expected = ts_val;
                ts_read = 0;
                tsm = 0;
            }
        }

        /* 头结束符 \r\n\r\n：此后才进入 body 阶段（不再把空行字节当 body） */
        if (c == '\r' && hdr_crlf == 0) hdr_crlf = 1;
        else if (c == '\n' && hdr_crlf == 1) hdr_crlf = 2;
        else if (c == '\r' && hdr_crlf == 2) hdr_crlf = 3;
        else if (c == '\n' && hdr_crlf == 3) {
            hdr_crlf = 0;
            if (cl_val > 0) {
                body_len = cl_val;
                body_recv = 0;
                cl_val = 0;
                transfer_state = 1;
                http_li = 0;
                http_line[0] = '\0';
            }
        } else {
            hdr_crlf = 0;
        }

        if (http_li >= sizeof(http_line) - 1) {
            /* header 缓冲满：仅用于 GET 检测，满了循环覆盖 */
            http_li = 0;
            http_line[0] = '\0';
        }
    } else if (body_recv < body_len) {
        /* ── body 阶段：写固件块。块收完后发 200，等待下一块 ── */
        if (feed_fw_byte(c)) {
            send_200(esp_conn_id);
        }
    }
}

/* 返回 200 OK（分块上传每块完成后，让浏览器发下一块）。
 * 必须带 Content-Length，否则 HTTP/1.1 浏览器会等连接关闭才认为响应结束，
 * 导致 XHR 的 onload 不触发、下一块永远不发送。 */
static void send_200(int conn_id)
{
    const char *resp = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
    char cmd[64];
    sprintf(cmd, "AT+CIPSEND=%d,%d", conn_id, (int)strlen(resp));
    ESP_Send(cmd);
    ESP_WaitResponse(">", 500);
    ESP_SendRaw(resp, strlen(resp));
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
        "button{background:#e94560;color:#fff;border:none;padding:15px 40px;border-radius:10px;font-size:18px;cursor:pointer}"
        "</style>"
        "<h2>Firmware Update</h2>"
        "<div class=box>"
        "<input type=file id=i accept=.bin>"
        "<button onclick=u()>UPLOAD & UPDATE</button>"
        "<p id=m></p>"
        "</div><script>"
        "function u(){"
        "var i=document.getElementById('i');if(!i.files[0]){alert('select file');return;}"
        "var f=i.files[0],CH=1024,off=0;"
        "function next(){"
        "if(off>=f.size){var d=new XMLHttpRequest();d.open('GET','/done',true);d.send();return;}"
        "var b=f.slice(off,off+CH);"
        "var x=new XMLHttpRequest();x.open('POST','/upload',true);"
        "x.setRequestHeader('Content-Type','application/octet-stream');"
        "x.setRequestHeader('X-Total-Size',f.size);"
        "x.onload=function(){off+=b.size;next();};"
        "x.onerror=function(){document.getElementById('m').innerHTML='block error';};"
        "x.send(b);}"
        "next();}"
        "</script></html>";
    char cmd[64];
    sprintf(cmd, "AT+CIPSEND=%d,%d", conn_id, (int)strlen(page));
    ESP_Send(cmd);
    /* 等待 ESP 返回 ">" 提示符后再发送数据 */
    ESP_WaitResponse(">", 500);
    ESP_SendRaw(page, strlen(page));
}

/* 强制结束上传（超时兜底）：flush 并标记完成 */
void BL_ESP01S_FinishTransfer(void)
{
    flush_fw_buffer();
    fw_total = fw_received;
    transfer_state = 2;
    BL_TFT_ShowProgressBar(100);
}

/* 中止上传（超时无新数据等异常）：标记错误，不进入引导，
 * 避免把残缺固件刷进 APP 分区导致启动黑屏。 */
void BL_ESP01S_AbortTransfer(void)
{
    transfer_error = 1;
}

void BL_ESP01S_Process(void)
{
    uint8_t byte;

    /* 乐鑫官方 AT 固件：+IPD,<id>,<len>:<data> 标准格式。
     * 正确剥离前缀，只把数据交给 HTTP 层。 */
    while (EspUart_ReadByte(&byte) != 0) {
        char c = (char)byte;

        /* ── 正在收 +IPD 分片数据：直接交给 HTTP 层 ── */
        if (in_ipd) {
            /* GET 已处理完（页面/图标/完成）：本分片剩余字节全部丢弃，
             * 重新等待下一个 +IPD 前缀，避免把上一请求尾部误当下一请求数据。 */
            if (ipd_skip_pending) {
                ipd_skip_pending = 0;
                in_ipd = 0;
                ipd_remain = 0;
                pfx_len = 0;
                continue;
            }
            feed_http_byte(c);
            ipd_remain--;
            if (ipd_remain == 0) in_ipd = 0;
            continue;
        }

        /* GET 处理完但此刻不在分片内：清掉跳过标记，等下一个 +IPD */
        if (ipd_skip_pending) {
            ipd_skip_pending = 0;
            continue;
        }

        /* ── 等待 +IPD,<id>,<len>: 前缀 ── */
        if (pfx_len == 0 && c != '+') continue;   /* 忽略非 + 开头（含官方调试输出） */

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
            /* 解析 len（最后一个逗号后）和 conn_id（第一个逗号前） */
            char *lc = pfx;
            char *tail = 0;
            while (*lc) { if (*lc == ',') tail = lc; lc++; }
            if (tail) {
                uint32_t len = 0;
                char *p = tail + 1;
                while (*p >= '0' && *p <= '9') { len = len * 10U + (uint32_t)(*p - '0'); p++; }
                ipd_remain = len;
                in_ipd = 1;
                /* conn_id: "+IPD," 后第一个数字 */
                char *q = pfx + 5;
                int id = 0;
                while (*q >= '0' && *q <= '9') { id = id * 10 + (*q - '0'); q++; }
                esp_conn_id = id;
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