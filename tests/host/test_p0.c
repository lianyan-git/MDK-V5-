#include <stdint.h>
#include <stdio.h>

#include "platform_contract.h"
#include "resource_config.h"
#include "system_time.h"

extern uint32_t mock_systick_reload;

static int failures = 0;

#define CHECK(condition)                                                        \
    do {                                                                        \
        if (!(condition)) {                                                     \
            fprintf(stderr, "FAIL: %s:%d: %s\n", __FILE__, __LINE__, #condition); \
            ++failures;                                                         \
        }                                                                       \
    } while (0)

static void test_internal_flash_contract(void)
{
    CHECK(PLATFORM_BOOT_ADDR == PLATFORM_FLASH_BASE);
    CHECK(PLATFORM_BOOT_END == PLATFORM_FLAG_ADDR);
    CHECK(PLATFORM_FLAG_END == PLATFORM_WIFI_CONFIG_ADDR);
    CHECK(PLATFORM_WIFI_CONFIG_END == PLATFORM_APP_ADDR);
    CHECK(PLATFORM_APP_END == PLATFORM_FLASH_END);
    CHECK(PLATFORM_APP_SIZE == 46U * 1024U);
}

static void test_external_flash_contract(void)
{
    CHECK(PLATFORM_DATA_ADDR == 0U);
    CHECK(PLATFORM_DATA_END == PLATFORM_FIRMWARE_ADDR);
    CHECK(PLATFORM_FIRMWARE_END == PLATFORM_METADATA_ADDR);
    CHECK(PLATFORM_METADATA_END == PLATFORM_EXT_FLASH_SIZE);
}

static void test_board_resource_contract(void)
{
    CHECK(RESOURCE_W25Q128_CS_PORT == RESOURCE_PORT_A);
    CHECK(RESOURCE_W25Q128_CS_PIN == 15U);
    CHECK(RESOURCE_TFT_RESET_PORT == RESOURCE_PORT_C);
    CHECK(RESOURCE_TFT_RESET_PIN == 13U);
    CHECK(RESOURCE_ENCODER_USES_GPIO_POLLING == 1U);
    CHECK(RESOURCE_PTC_TIMER == RESOURCE_TIMER_POWER_OUTPUT);
    CHECK(RESOURCE_FAN_TIMER == RESOURCE_TIMER_POWER_OUTPUT);
    CHECK(RESOURCE_TIMER_STEPPER_RESERVED == 2U);
}

static void test_system_time(void)
{
    SystemTime_Init();
    CHECK(mock_systick_reload == 72000U);
    CHECK(SystemTime_Millis() == 0U);
    SystemTime_TickISR();
    SystemTime_TickISR();
    SystemTime_TickISR();
    CHECK(SystemTime_Millis() == 3U);
}

int main(void)
{
    test_internal_flash_contract();
    test_external_flash_contract();
    test_board_resource_contract();
    test_system_time();

    if (failures != 0) {
        fprintf(stderr, "FAIL: %d P0 host assertion(s) failed\n", failures);
        return 1;
    }
    puts("PASS: P0 host contracts and system time");
    return 0;
}
