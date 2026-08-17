#include "stm32f10x.h"

#include "board.h"
#include "bsp_w25q128.h"
#include "ota_display.h"
#include "platform_contract.h"
#include "system_time.h"
#include "system_config.h"
#include "bsp_tft_st7789.h"

#ifndef BOOTLOADER_BUILD
#include "bsp_aht20.h"
#include "bsp_buzzer.h"
#include "bsp_cs1237.h"
#include "bsp_encoder.h"
#include "bsp_fan.h"
#include "bsp_ntc.h"
#include "bsp_ptc.h"
#include "bsp_rgb_led.h"
#include "bsp_stepper.h"
#include "esp_at.h"
#include "esp_http_bridge.h"
#include "http_server.h"
#include "ota_http.h"
#include "ota_metadata_store.h"
#include "ota_update_controller.h"
#include "ota_upload.h"
#include "mod_ota.h"
#include "ui_manager.h"
#include <string.h>
#include <stdio.h>
#endif

SystemState_t g_sys;

#ifndef BOOTLOADER_BUILD
static void refresh_api_data(void);
static void read_sensors(void);
static void safety_check(void);
static void control_update(void);
static void update_rgb(void);
static void trigger_safety(SafetyState_t state);
#endif

#ifndef BOOTLOADER_BUILD
int main(void)
{
    /* ── 最小屏幕验证版：仅初始化时钟 + 屏幕，显示主界面。
     * 其余功能（传感器/WiFi/长按进bootloader等）全部注释，先确认屏幕能亮。 ── */

    g_sys.params.target_temp = TEMP_DEFAULT;
    g_sys.params.dry_time_sec = TIME_DEFAULT_SEC;
    g_sys.params.ptc_max_temp = PTC_TEMP_DEFAULT;
    g_sys.params.ptc_cooling_temp = PTC_COOLING_TEMP_DEFAULT;
    g_sys.params.pid_kp = 10.0f;
    g_sys.params.pid_ki = 0.5f;
    g_sys.params.pid_kd = 2.0f;
    g_sys.params.motor_enabled = 1;
    g_sys.params.motor_direction = 0;
    g_sys.params.motor_speed = 5;
    g_sys.params.motor_oscillate = 0;
    g_sys.params.motor_oscillate_angle = 30;
    g_sys.params.motor_driver = MOTOR_DRIVER_A4988;
    g_sys.params.motor_current = 2;
    g_sys.params.motor_stealthchop = 0;

    g_sys.current_temp = 25.0f;
    g_sys.current_humidity = 50.0f;
    g_sys.weight_g = 0.0f;
    g_sys.ptc_temp = 25.0f;
    g_sys.run_state = STATE_IDLE;
    g_sys.current_screen = SCREEN_MAIN;
    g_sys.selected_item = 0;
    g_sys.submenu_active = 0;
    g_sys.time_cursor = TIME_FIELD_HOUR;
    g_sys.temp_edit_active = 0;
    g_sys.ptc_edit_active = 0;
    g_sys.pid_autotune_running = 0;
    g_sys.pid_autotune_progress = 0;
    g_sys.temp_pid_running = 0;
    g_sys.temp_pid_progress = 0;
    g_sys.complete_timer = 0;
    g_sys.device_id = 0;
    g_sys.safety_state = SAFETY_NONE;
    g_sys.fan_speed = 0;
    g_sys.drying_active = 0;
    g_sys.chamber_temp_last = 25.0f;

    Board_Init();
    Watchdog_Init();   /* App 自启看门狗，不依赖 Bootloader */
    SystemTime_Init(); /* 启动 SysTick，供 SystemTime_Millis 使用 */

    /* 初始化屏幕并显示主界面 */
    TFT_Init();
    UI_ShowBootScreen();
    UI_DrawMainScreen();

    for (;;) {
        Watchdog_Kick();
        /* 只喂狗，其余全部注释 */
    }
}
#endif /* BOOTLOADER_BUILD */

#ifndef BOOTLOADER_BUILD
#define APP_VERSION_TEXT        "0.1.0"
#define BOOTLOADER_VERSION_TEXT "0.1.0"
#define SAFETY_DROP_THRESHOLD   2.0f
#define SAFETY_STUCK_MINUTES    2
#define FAN_MIN_SPEED           10

static void refresh_api_data(void)
{
    HttpApiData_t data;
    OtaMetadata_t metadata;
    memset(&data, 0, sizeof(data));
    strcpy(data.app_version, APP_VERSION_TEXT);
    strcpy(data.bootloader_version, BOOTLOADER_VERSION_TEXT);
    data.ota_state = OTA_STATE_IDLE;
    if (OtaMetadataStore_Load(&metadata, 0) == OTA_METADATA_STORE_OK) {
        data.ota_state = (OtaState_t)metadata.state;
        if ((metadata.state == (uint32_t)OTA_STATE_RECEIVING) && (OtaUpload_GetState() == OTA_UPLOAD_STATE_IDLE) && OtaUpload_IsStoragePrepared()) data.ota_state = OTA_STATE_IDLE;
        data.staged_size = metadata.image_size;
        data.staged_crc32 = metadata.image_crc32;
        data.staged_crc_valid = (metadata.state == (uint32_t)OTA_STATE_READY) || (metadata.state == (uint32_t)OTA_STATE_APPLYING) || (metadata.state == (uint32_t)OTA_STATE_APPLIED);
    }
    HttpServer_SetApiData(&data);
}

static void read_sensors(void)
{
    float temp, hum, weight;
    int16_t ptc_raw;

    /* AHT20 读取失败：保持上次温度，但若正在烘干则触发安全保护，
     * 避免"传感器假 25°C → 永远认为没到温 → 持续加热"的致命场景 */
    if (AHT20_Read(&temp, &hum) != 0) {
        if (g_sys.drying_active && g_sys.safety_state == SAFETY_NONE) {
            trigger_safety(SAFETY_BOX_BROKEN);
        }
    } else {
        g_sys.current_temp = temp;
        g_sys.current_humidity = hum;
    }

    weight = CS1237_ReadWeight();
    if (weight >= 0.0f) g_sys.weight_g = weight;

    ptc_raw = NTC_GetTemperature();
    g_sys.ptc_temp = (float)ptc_raw / 10.0f;

    /* NTC 开路/短路异常（映射到极端值）也触发安全保护 */
    if (ptc_raw <= -100 || ptc_raw >= 2000) {
        if (g_sys.drying_active && g_sys.safety_state == SAFETY_NONE) {
            trigger_safety(SAFETY_BOX_BROKEN);
        }
    }
}

static void update_rgb(void)
{
    static uint8_t rgb_tick = 0;
    rgb_tick++;
    if (g_sys.safety_state != SAFETY_NONE) { RGB_Status_Red(); RGB_Progress_ColorWheel(rgb_tick); return; }
    if (g_sys.run_state == STATE_HEATING || g_sys.run_state == STATE_DRYING) RGB_Status_Red();
    else if (g_sys.run_state == STATE_COMPLETE) RGB_Status_Green();
    else RGB_Status_Off();
    if (g_sys.run_state == STATE_IDLE || g_sys.run_state == STATE_COMPLETE) RGB_Progress_Rainbow();
    else RGB_Progress_ColorWheel(rgb_tick);
}

static void trigger_safety(SafetyState_t state)
{
    if (g_sys.safety_state != SAFETY_NONE) return;
    g_sys.safety_state = state;
    g_sys.run_state = STATE_IDLE;
    g_sys.drying_active = 0;
    PTC_SetPower(0);
    Fan_SetSpeed(100);
    Stepper_Enable(0);
    Buzzer_Beep(200);
    Buzzer_Beep(300);
    Buzzer_Beep(200);
    g_sys.prev_screen = g_sys.current_screen;
    g_sys.current_screen = SCREEN_SAFETY_ALERT;
}

void StartDrying(void)
{
    g_sys.drying_active = 1;
    g_sys.run_state = STATE_HEATING;
    g_sys.remaining_sec = g_sys.params.dry_time_sec;
    g_sys.temp_stuck_start = SystemTime_Millis();
    g_sys.safety_state = SAFETY_NONE;
    g_sys.chamber_temp_last = g_sys.current_temp;
    PTC_SetPower(100);
    Fan_SetSpeed(30);
    if (g_sys.params.motor_enabled) {
        Stepper_Enable(1);
        Stepper_SetSpeed(g_sys.params.motor_speed * 200);
        if (g_sys.params.motor_oscillate) Stepper_SetOscillate(g_sys.params.motor_oscillate_angle * 10);
        else Stepper_Move(g_sys.params.motor_direction ? -100000 : 100000);
    }
}

void StopDrying(void)
{
    g_sys.drying_active = 0;
    g_sys.run_state = STATE_COOLING;
    PTC_SetPower(0);
    Fan_SetSpeed(100);
    Stepper_Enable(0);
}

static void safety_check(void)
{
    uint32_t now = SystemTime_Millis();
    if (g_sys.safety_state != SAFETY_NONE || !g_sys.drying_active) return;
    if (g_sys.run_state != STATE_HEATING && g_sys.run_state != STATE_DRYING) return;
    if (g_sys.chamber_temp_last - g_sys.current_temp > SAFETY_DROP_THRESHOLD) { trigger_safety(SAFETY_BOX_BROKEN); return; }
    if (g_sys.temp_stuck_start == 0) { g_sys.temp_stuck_start = now; }
    if ((int32_t)(now - g_sys.temp_stuck_start) >= (int32_t)(SAFETY_STUCK_MINUTES * 60000)) {
        if (g_sys.current_temp - g_sys.chamber_temp_last < 1.0f) { trigger_safety(SAFETY_LID_OPEN); return; }
        g_sys.temp_stuck_start = now;
    }
    g_sys.chamber_temp_last = g_sys.current_temp;
}

static void update_fan_for_ptc(void)
{
    static float last_ptc = 0.0f;
    if (g_sys.ptc_temp <= 0.0f) { last_ptc = g_sys.ptc_temp; return; }  /* 传感器异常不调风 */
    float delta = last_ptc - g_sys.ptc_temp;
    if (delta > 1.0f) { if (Fan_GetSpeed() > FAN_MIN_SPEED) Fan_AdjustSpeed(-5); }
    else if (g_sys.ptc_temp > (float)(g_sys.params.ptc_max_temp + 2)) Fan_AdjustSpeed(5);
    last_ptc = g_sys.ptc_temp;
}

static void control_update(void)
{
    float target = (float)g_sys.params.target_temp;
    static uint8_t heater_on = 0;
    static uint32_t last_tick = 0;
    uint32_t now = SystemTime_Millis();

    /* 独立硬过温保护：PTC 温度超限立即切断加热，不依赖控制状态 */
    if (NTC_IsOverTemp()) {
        PTC_SetPower(0);
        trigger_safety(SAFETY_BOX_BROKEN);
        return;
    }

    if (g_sys.safety_state != SAFETY_NONE) { PTC_SetPower(0); return; }

    switch (g_sys.run_state) {
    case STATE_HEATING:
        if (g_sys.current_temp < target - 1.0f) PTC_SetPower(100);
        else if (g_sys.current_temp >= target) { PTC_SetPower(0); g_sys.run_state = STATE_DRYING; last_tick = now; }
        break;
    case STATE_DRYING:
        if (g_sys.current_temp < target - 1.0f) { if (!heater_on) { PTC_SetPower(100); heater_on = 1; } }
        else if (g_sys.current_temp >= target + 0.5f) { if (heater_on) { PTC_SetPower(0); heater_on = 0; } }
        update_fan_for_ptc();
        /* 每 200ms 调用一次，累计满 1 秒才减 1 秒（修复 5 倍速倒计时） */
        if (last_tick == 0) last_tick = now;
        if ((int32_t)(now - last_tick) >= (int32_t)1000) {
            last_tick = now;
            if (g_sys.remaining_sec > 0) {
                g_sys.remaining_sec--;
            } else {
                StopDrying();
                heater_on = 0;
            }
        }
        break;
    case STATE_COOLING:
        Fan_SetSpeed(100);
        if (g_sys.ptc_temp <= (float)g_sys.params.ptc_cooling_temp) { g_sys.run_state = STATE_COMPLETE; Fan_Off(); }
        break;
    default: break;
    }
}
#endif /* BOOTLOADER_BUILD */