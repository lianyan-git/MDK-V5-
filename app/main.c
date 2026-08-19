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
#include "pin_config.h"
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
    uint32_t ui_tick = 0;
    uint32_t now;

    /* ── 先点亮背光(PB0 推挽高)：已验证有效，防止任何后续卡死时屏幕全黑 ── */
    {
        GPIO_InitTypeDef g;
        RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);
        g.GPIO_Pin = PIN_TFT_BL_PIN;
        g.GPIO_Mode = GPIO_Mode_Out_PP;
        g.GPIO_Speed = GPIO_Speed_50MHz;
        GPIO_Init(PIN_TFT_BL_PORT, &g);
        GPIO_SetBits(PIN_TFT_BL_PORT, PIN_TFT_BL_PIN);
    }

    /* g_sys 初始化（UI 需要） */
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

    /* 不用 Board_Init（含 NTC_Init 的 ADC 校准 while，未接传感器可能卡死）。
     * 只做安全引脚初始化（加热器关断 + SWJ 重映射）。 */
    Board_EarlyInit();
    Watchdog_Init();   /* App 自启看门狗，不依赖 Bootloader */

    /* 关键：从 RCC 读回实际时钟，纠正 SystemCoreClock（与 bootloader 一致）。
     * 否则 SysTick 周期错，SystemTime_Millis 不走。 */
    SystemCoreClockUpdate();
    SystemTime_Init(); /* 启动 SysTick，供 SystemTime_Millis/Encoder 计时 */

    /* Bootloader 在 BootloaderV2_JumpToApp() 跳转前调用了 __disable_irq()，
     * 而 App 的 SystemInit/main 从不重新开中断 → PRIMASK 保持 1 → SysTick
     * 永不触发 → SystemTime_Millis 冻结 → 编码器单击/长按计时全部失效
     * （旋转仍可用，因其只轮询 GPIO 不依赖中断）。此处必须重新开中断。 */
    __enable_irq();

    /* 上电长按编码器(约1s) → 强制进入 Bootloader 下载模式。
     * 直接轮询按钮引脚，不依赖 Encoder_Process()（它内部会消费事件，
     * 导致这里再调 Encoder_GetEvent() 永远拿不到 LONG_PRESS）。 */
    {
        Encoder_Init();
        int force_boot = 0;
        uint32_t t0 = SystemTime_Millis();
        uint32_t btn_t0 = 0;
        while ((int32_t)(SystemTime_Millis() - t0) < 3000) {
            Watchdog_Kick();
            if (GPIO_ReadInputDataBit(PIN_ENC_BTN_PORT, PIN_ENC_BTN_PIN) == 0) {
                if (btn_t0 == 0) btn_t0 = SystemTime_Millis();
                else if ((int32_t)(SystemTime_Millis() - btn_t0) >= 1000) {
                    force_boot = 1;
                    break;
                }
            } else {
                btn_t0 = 0;
            }
        }
        if (force_boot) {
            OTA_EnterBootloader();   /* 写 FORCE_BOOT 标志并复位，不会返回 */
        }
    }

    /* 屏幕初始化 + 主界面 */
    TFT_Init();
    UI_ShowBootScreen();
    UI_DrawMainScreen();

    /* 编码器：Encoder_Process 内部处理旋转/单击，长按进菜单由下方独立检测。
     * 旋转后整屏重绘主界面（无白框残影，选中框随卡片高亮）。 */
    {
        uint32_t btn_press_ms = 0;
        uint8_t  btn_was_down = 0;
        uint8_t  btn_long_done = 0;
        uint8_t  last_sel = 0xFF;
        for (;;) {
            now = SystemTime_Millis();
            Watchdog_Kick();

            {
                uint8_t old_sel = last_sel;
                Encoder_Process();   /* 旋转/单击由内部状态机处理 */
                if (g_sys.current_screen == SCREEN_MAIN && last_sel != 0xFF
                    && g_sys.selected_item != last_sel) {
                    /* SGL 脏矩形局部刷新：只重绘旧/新两张卡片，不整屏重绘 */
                    UI_RefreshCard(old_sel);
                    UI_RefreshCard(g_sys.selected_item);
                }
                last_sel = g_sys.selected_item;
            }

            /* 独立长按检测：按钮按下持续 1s → 进菜单（与 bootloader 长按区分：这里不写标志） */
            if (GPIO_ReadInputDataBit(PIN_ENC_BTN_PORT, PIN_ENC_BTN_PIN) == 0) {
                if (!btn_was_down) { btn_press_ms = now; btn_was_down = 1; btn_long_done = 0; }
                else if (!btn_long_done && (int32_t)(now - btn_press_ms) >= 1000) {
                    btn_long_done = 1;
                    if (g_sys.current_screen == SCREEN_MAIN) g_sys.current_screen = SCREEN_MENU;
                    else g_sys.current_screen = SCREEN_MAIN;
                }
            } else {
                btn_was_down = 0;
            }

            if ((int32_t)(now - ui_tick) >= (int32_t)50) {
                ui_tick = now;
                UI_Update();
            }
        }
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