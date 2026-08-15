#ifndef BOOTLOADER_BUILD
/*
 * mod_wifi_config.c
 * WiFi config storage with wear leveling
 */
#include "mod_wifi_config.h"
#include "bsp_w25q128.h"
#include <string.h>

#define WIFI_SECTOR_COUNT   4
#define WIFI_SECTOR_SIZE    (4096)
#define WIFI_CONFIG_START   (0x00F00000)

static uint8_t g_current_page = 0;
static uint32_t g_write_version = 0;

static uint32_t calc_checksum(const WiFiConfig_t *cfg)
{
    uint32_t sum = cfg->magic + cfg->version + cfg->last_used_index;
    for (uint8_t i = 0; i < WIFI_MAX_SAVED; i++) {
        for (uint8_t j = 0; j < WIFI_SSID_LEN; j++) sum += cfg->entries[i].ssid[j];
        for (uint8_t j = 0; j < WIFI_PASS_LEN; j++) sum += cfg->entries[i].password[j];
        sum += cfg->entries[i].valid;
    }
    return ~sum;
}

void WiFiConfig_Init(void)
{
    WiFiConfig_t cfg;
    uint32_t best_version = 0;
    uint8_t best_page = 0;
    uint8_t found = 0;

    for (uint8_t i = 0; i < WIFI_SECTOR_COUNT; i++) {
        uint32_t addr = WIFI_CONFIG_START + i * WIFI_SECTOR_SIZE;
        W25Q128_Read(addr, (uint8_t*)&cfg, sizeof(WiFiConfig_t));
        if (cfg.magic == WIFI_MAGIC && calc_checksum(&cfg) == cfg.checksum) {
            if (cfg.version > best_version) {
                best_version = cfg.version;
                best_page = i;
                found = 1;
            }
        }
    }
    if (found) {
        g_current_page = best_page;
        g_write_version = best_version;
    } else {
        memset(&cfg, 0, sizeof(cfg));
        cfg.magic = WIFI_MAGIC;
        cfg.version = 1;
        cfg.checksum = calc_checksum(&cfg);
        W25Q128_EraseSector(WIFI_CONFIG_START);
        W25Q128_Write(WIFI_CONFIG_START, (uint8_t*)&cfg, sizeof(WiFiConfig_t));
        g_current_page = 0;
        g_write_version = 1;
    }
}

int WiFiConfig_Read(WiFiConfig_t *config)
{
    uint32_t addr = WIFI_CONFIG_START + g_current_page * WIFI_SECTOR_SIZE;
    W25Q128_Read(addr, (uint8_t*)config, sizeof(WiFiConfig_t));
    if (config->magic != WIFI_MAGIC || calc_checksum(config) != config->checksum) return -1;
    return 0;
}

int WiFiConfig_Write(WiFiConfig_t *config)
{
    config->version = ++g_write_version;
    config->checksum = calc_checksum(config);
    g_current_page = (g_current_page + 1) % WIFI_SECTOR_COUNT;
    uint32_t addr = WIFI_CONFIG_START + g_current_page * WIFI_SECTOR_SIZE;
    W25Q128_EraseSector(addr);
    W25Q128_Write(addr, (uint8_t*)config, sizeof(WiFiConfig_t));
    return 0;
}

int WiFiConfig_Add(const char *ssid, const char *password)
{
    WiFiConfig_t cfg;
    if (WiFiConfig_Read(&cfg) != 0) {
        memset(&cfg, 0, sizeof(cfg));
        cfg.magic = WIFI_MAGIC;
    }
    int replace_idx = -1;
    for (uint8_t i = 0; i < WIFI_MAX_SAVED; i++) {
        if (!cfg.entries[i].valid) { replace_idx = i; break; }
    }
    if (replace_idx < 0) {
        uint8_t oldest = 0;
        uint32_t oldest_ver = cfg.entries[0].valid;
        for (uint8_t i = 1; i < WIFI_MAX_SAVED; i++) {
            if (cfg.entries[i].valid < oldest_ver) { oldest_ver = cfg.entries[i].valid; oldest = i; }
        }
        replace_idx = oldest;
    }
    strncpy(cfg.entries[replace_idx].ssid, ssid, WIFI_SSID_LEN - 1);
    cfg.entries[replace_idx].ssid[WIFI_SSID_LEN - 1] = '\0';
    strncpy(cfg.entries[replace_idx].password, password, WIFI_PASS_LEN - 1);
    cfg.entries[replace_idx].password[WIFI_PASS_LEN - 1] = '\0';
    cfg.entries[replace_idx].valid = 1;
    cfg.last_used_index = replace_idx;
    return WiFiConfig_Write(&cfg);
}

uint8_t WiFiConfig_GetValidCount(void)
{
    WiFiConfig_t cfg;
    if (WiFiConfig_Read(&cfg) != 0) return 0;
    uint8_t count = 0;
    for (uint8_t i = 0; i < WIFI_MAX_SAVED; i++) {
        if (cfg.entries[i].valid) count++;
    }
    return count;
}

uint8_t WiFiConfig_GetNextSlot(void)
{
    return 0;
}
#endif /* BOOTLOADER_BUILD */