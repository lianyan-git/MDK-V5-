#include "bsp_encoder.h"
#include "bsp_buzzer.h"
#include "bsp_cs1237.h"
#include "bsp_ptc.h"
#include "bsp_stepper.h"
#include "system_config.h"
#include "pin_config.h"
#include "system_time.h"
#include "stm32f10x.h"

#ifndef BOOTLOADER_BUILD

/* Weak stubs for bootloader build - overridden by strong definitions in APP build */
SystemState_t g_sys __attribute__((weak));
void Buzzer_Beep(uint16_t ms) __attribute__((weak));
void CS1237_Tare(void) __attribute__((weak));
uint32_t System_GetDeviceId(void) __attribute__((weak));
void System_SaveParams(void) __attribute__((weak));
void StartDrying(void) __attribute__((weak));
void StopDrying(void) __attribute__((weak));

void Buzzer_Beep(uint16_t ms) { (void)ms; }
void CS1237_Tare(void) { }
uint32_t System_GetDeviceId(void) { return 0; }
void System_SaveParams(void) { }
void StartDrying(void) { }
void StopDrying(void) { }

static uint8_t enc_last_ab = 0;
static int8_t enc_accum = 0;
static volatile uint32_t btn_down_time = 0;
static volatile uint8_t btn_down = 0;
static volatile uint8_t btn_long_flag = 0;

void Encoder_Init(void)
{
    GPIO_InitTypeDef g;
    g.GPIO_Pin = PIN_ENC_A_PIN | PIN_ENC_B_PIN;
    g.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(PIN_ENC_A_PORT, &g);
    g.GPIO_Pin = PIN_ENC_BTN_PIN;
    g.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(PIN_ENC_BTN_PORT, &g);
    enc_last_ab = (uint8_t)((GPIO_ReadInputDataBit(PIN_ENC_A_PORT, PIN_ENC_A_PIN) << 1)
                  | GPIO_ReadInputDataBit(PIN_ENC_B_PORT, PIN_ENC_B_PIN));
}

EncoderEvent_t Encoder_GetEvent(void)
{
    static const int8_t table[16] = {0,-1,1,0,1,0,0,-1,-1,0,0,1,0,1,-1,0};
    uint8_t ab = (uint8_t)((GPIO_ReadInputDataBit(PIN_ENC_A_PORT, PIN_ENC_A_PIN) << 1)
                 | GPIO_ReadInputDataBit(PIN_ENC_B_PORT, PIN_ENC_B_PIN));
    enc_accum += table[(enc_last_ab << 2) | ab];
    enc_last_ab = ab;
    if (enc_accum >= 4) { enc_accum = 0; return ENC_EVT_CW; }
    if (enc_accum <= -4) { enc_accum = 0; return ENC_EVT_CCW; }
    if (!btn_down && GPIO_ReadInputDataBit(PIN_ENC_BTN_PORT, PIN_ENC_BTN_PIN) == 0) {
        btn_down = 1; btn_down_time = SystemTime_Millis();
    }
    if (btn_long_flag) { btn_long_flag = 0; return ENC_EVT_LONG_PRESS; }
    if (btn_down && GPIO_ReadInputDataBit(PIN_ENC_BTN_PORT, PIN_ENC_BTN_PIN) == 1) {
        uint32_t dur = SystemTime_Millis() - btn_down_time;
        btn_down = 0;
        if (dur > 1000) return ENC_EVT_LONG_PRESS;
        if (dur > 50) return ENC_EVT_CLICK;
    }
    if (btn_down && (SystemTime_Millis() - btn_down_time > 1000) && !btn_long_flag) {
        btn_long_flag = 1;
    }
    return ENC_EVT_NONE;
}

static void motor_edit_set(int param_index, int up)
{
    Params_t *p = &g_sys.params;
    switch (param_index) {
    case 0: /* 烘干联动 */
        p->motor_enabled = up ? 1 : 0;
        break;
    case 1: /* 转动方向 */
        p->motor_direction = up ? 1 : 0;
        break;
    case 2: /* 转动速度 1-50 */
        if (up) { if (p->motor_speed < 50) p->motor_speed++; }
        else    { if (p->motor_speed > 1) p->motor_speed--; }
        break;
    case 3: /* 边烘边打 */
        p->motor_oscillate = up ? 1 : 0;
        break;
    case 4: /* 摆动角度 1-30 (封顶30) */
        if (up) { if (p->motor_oscillate_angle < 30) p->motor_oscillate_angle++; }
        else    { if (p->motor_oscillate_angle > 1) p->motor_oscillate_angle--; }
        break;
    case 5: /* 驱动选择 */
        p->motor_driver = up ? MOTOR_DRIVER_TMC2209 : MOTOR_DRIVER_A4988;
        break;
    case 6: /* 驱动电流 0.2-0.6 步进0.1 */
        if (up) { if (p->motor_current < 6) p->motor_current++; }
        else    { if (p->motor_current > 2) p->motor_current--; }
        break;
    case 7: /* 静音 */
        p->motor_stealthchop = up ? 1 : 0;
        break;
    default: break;
    }
}

void Encoder_Process(void)
{
    EncoderEvent_t evt = Encoder_GetEvent();
    if (evt == ENC_EVT_NONE) return;
    Buzzer_Beep(20);

    switch (g_sys.current_screen) {

    case SCREEN_MAIN:
        if (evt == ENC_EVT_CW) g_sys.selected_item = (g_sys.selected_item + 1) % 4;
        else if (evt == ENC_EVT_CCW) g_sys.selected_item = (g_sys.selected_item == 0) ? 3 : g_sys.selected_item - 1;
        else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) g_sys.current_screen = SCREEN_TEMP_ADJUST;
            else if (g_sys.selected_item == 1) { g_sys.selected_item = 0; g_sys.current_screen = SCREEN_WEIGHT; }
            else if (g_sys.selected_item == 2) g_sys.current_screen = SCREEN_TIME_ADJUST;
            else if (g_sys.selected_item == 3) g_sys.current_screen = SCREEN_PTC_ADJUST;
        }
        else if (evt == ENC_EVT_LONG_PRESS) g_sys.current_screen = SCREEN_MENU;
        break;

    case SCREEN_WEIGHT:
        if (evt == ENC_EVT_CW) g_sys.selected_item = (g_sys.selected_item + 1) % 2;
        else if (evt == ENC_EVT_CCW) g_sys.selected_item = (g_sys.selected_item == 0) ? 1 : 0;
        else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) { CS1237_Tare(); g_sys.weight_g = 0.0f; }
            else g_sys.current_screen = SCREEN_MAIN;
        }
        break;

    case SCREEN_TEMP_ADJUST:
        if (evt == ENC_EVT_CW) g_sys.selected_item = (g_sys.selected_item + 1) % 3;
        else if (evt == ENC_EVT_CCW) g_sys.selected_item = (g_sys.selected_item == 0) ? 2 : g_sys.selected_item - 1;
        else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) { g_sys.submenu_active = 0; g_sys.current_screen = SCREEN_TEMP_EDIT; }
            else if (g_sys.selected_item == 1) {
                g_sys.temp_pid_running = 1; g_sys.temp_pid_progress = 0;
                PTC_TempPID_AutotuneStart((float)g_sys.params.target_temp);
                g_sys.current_screen = SCREEN_TEMP_PID;
            }
            else g_sys.current_screen = SCREEN_MAIN;
        }
        break;

    case SCREEN_TEMP_EDIT:
        if (evt == ENC_EVT_CW) { if (g_sys.params.target_temp < TEMP_MAX) g_sys.params.target_temp++; }
        else if (evt == ENC_EVT_CCW) { if (g_sys.params.target_temp > TEMP_MIN) g_sys.params.target_temp--; }
        else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) { System_SaveParams(); g_sys.current_screen = SCREEN_TEMP_ADJUST; }
            else g_sys.current_screen = SCREEN_TEMP_ADJUST;
        }
        break;

    case SCREEN_TEMP_PID:
        if (evt == ENC_EVT_CLICK) g_sys.current_screen = SCREEN_TEMP_ADJUST;
        break;

    case SCREEN_TIME_ADJUST:
        if (evt == ENC_EVT_CW) {
            if (g_sys.time_cursor == TIME_FIELD_HOUR) g_sys.time_cursor = TIME_FIELD_MIN;
            else if (g_sys.time_cursor == TIME_FIELD_MIN) g_sys.time_cursor = TIME_FIELD_SEC;
            else g_sys.time_cursor = TIME_FIELD_HOUR;
            g_sys.selected_item = 0;
        } else if (evt == ENC_EVT_CCW) {
            if (g_sys.time_cursor == TIME_FIELD_HOUR) g_sys.time_cursor = TIME_FIELD_SEC;
            else if (g_sys.time_cursor == TIME_FIELD_MIN) g_sys.time_cursor = TIME_FIELD_HOUR;
            else g_sys.time_cursor = TIME_FIELD_MIN;
            g_sys.selected_item = 0;
        } else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) { g_sys.current_screen = SCREEN_TIME_EDIT; }
            else { System_SaveParams(); g_sys.current_screen = SCREEN_MAIN; }
        }
        break;

    case SCREEN_TIME_EDIT: {
        uint32_t h = g_sys.params.dry_time_sec / 3600;
        uint32_t m = (g_sys.params.dry_time_sec % 3600) / 60;
        uint32_t s = g_sys.params.dry_time_sec % 60;
        if (evt == ENC_EVT_CW) {
            if (g_sys.time_cursor == TIME_FIELD_HOUR && h < 47) h++;
            else if (g_sys.time_cursor == TIME_FIELD_MIN && m < 59) m++;
            else if (g_sys.time_cursor == TIME_FIELD_SEC && s < 59) s++;
        } else if (evt == ENC_EVT_CCW) {
            if (g_sys.time_cursor == TIME_FIELD_HOUR && h > 0) h--;
            else if (g_sys.time_cursor == TIME_FIELD_MIN && m > 0) m--;
            else if (g_sys.time_cursor == TIME_FIELD_SEC && s > 0) s--;
        } else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) { System_SaveParams(); g_sys.current_screen = SCREEN_TIME_ADJUST; }
            else g_sys.current_screen = SCREEN_TIME_ADJUST;
        }
        g_sys.params.dry_time_sec = h * 3600 + m * 60 + s;
        break;
    }

    case SCREEN_PTC_ADJUST:
        if (evt == ENC_EVT_CW) g_sys.selected_item = (g_sys.selected_item + 1) % 4;
        else if (evt == ENC_EVT_CCW) g_sys.selected_item = (g_sys.selected_item == 0) ? 3 : g_sys.selected_item - 1;
        else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) { g_sys.current_screen = SCREEN_PTC_EDIT; }
            else if (g_sys.selected_item == 1) { g_sys.current_screen = SCREEN_PTC_COOLING_EDIT; }
            else if (g_sys.selected_item == 2) {
                g_sys.pid_autotune_running = 1; g_sys.pid_autotune_progress = 0;
                PTC_PID_AutotuneStart();
                g_sys.current_screen = SCREEN_PID_AUTOTUNE;
            }
            else g_sys.current_screen = SCREEN_MAIN;
        }
        break;

    case SCREEN_PTC_EDIT:
        if (evt == ENC_EVT_CW) { if (g_sys.params.ptc_max_temp < PTC_TEMP_MAX) g_sys.params.ptc_max_temp++; }
        else if (evt == ENC_EVT_CCW) { if (g_sys.params.ptc_max_temp > PTC_TEMP_MIN) g_sys.params.ptc_max_temp--; }
        else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) { System_SaveParams(); g_sys.current_screen = SCREEN_PTC_ADJUST; }
            else g_sys.current_screen = SCREEN_PTC_ADJUST;
        }
        break;

    case SCREEN_PTC_COOLING_EDIT:
        if (evt == ENC_EVT_CW) { if (g_sys.params.ptc_cooling_temp < PTC_TEMP_MIN) g_sys.params.ptc_cooling_temp++; }
        else if (evt == ENC_EVT_CCW) { if (g_sys.params.ptc_cooling_temp > 25) g_sys.params.ptc_cooling_temp--; }
        else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) { System_SaveParams(); g_sys.current_screen = SCREEN_PTC_ADJUST; }
            else g_sys.current_screen = SCREEN_PTC_ADJUST;
        }
        break;

    case SCREEN_PID_AUTOTUNE:
        if (evt == ENC_EVT_CLICK) g_sys.current_screen = SCREEN_PTC_ADJUST;
        break;

    case SCREEN_MENU:
        if (evt == ENC_EVT_CW) g_sys.selected_item = (g_sys.selected_item + 1) % 6;
        else if (evt == ENC_EVT_CCW) g_sys.selected_item = (g_sys.selected_item == 0) ? 5 : g_sys.selected_item - 1;
        else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) {
                if (g_sys.drying_active) StopDrying();
                else StartDrying();
                g_sys.current_screen = SCREEN_MAIN;
            }
            else if (g_sys.selected_item == 1) g_sys.current_screen = SCREEN_WIFI;
            else if (g_sys.selected_item == 2) { g_sys.selected_item = 0; g_sys.current_screen = SCREEN_MOTOR_ADJUST; }
            else if (g_sys.selected_item == 3) { g_sys.device_id = System_GetDeviceId(); g_sys.current_screen = SCREEN_ABOUT; }
            else if (g_sys.selected_item == 4) NVIC_SystemReset();
            else g_sys.current_screen = SCREEN_MAIN;
        }
        break;

    case SCREEN_MOTOR_ADJUST:
    {
        uint8_t count = (g_sys.params.motor_driver == MOTOR_DRIVER_TMC2209) ? 9 : 6;
        if (evt == ENC_EVT_CW) g_sys.selected_item = (g_sys.selected_item + 1) % count;
        else if (evt == ENC_EVT_CCW) g_sys.selected_item = (g_sys.selected_item == 0) ? (count - 1) : g_sys.selected_item - 1;
        else if (evt == ENC_EVT_CLICK) {
            g_sys.submenu_active = g_sys.selected_item;
            g_sys.current_screen = SCREEN_MOTOR_EDIT;
        }
        break;
    }

    case SCREEN_MOTOR_EDIT:
        if (evt == ENC_EVT_CW) motor_edit_set(g_sys.submenu_active, 1);
        else if (evt == ENC_EVT_CCW) motor_edit_set(g_sys.submenu_active, 0);
        else if (evt == ENC_EVT_CLICK) {
            if (g_sys.selected_item == 0) { System_SaveParams(); g_sys.current_screen = SCREEN_MOTOR_ADJUST; }
            else g_sys.current_screen = SCREEN_MOTOR_ADJUST;
        }
        break;

    case SCREEN_ABOUT:
        if (evt == ENC_EVT_CLICK) g_sys.current_screen = SCREEN_MENU;
        break;

    case SCREEN_WIFI:
        if (evt == ENC_EVT_CLICK) { if (g_sys.selected_item == 0) g_sys.current_screen = SCREEN_MENU; else g_sys.wifi_enabled = !g_sys.wifi_enabled; }
        break;

    case SCREEN_OTA:
        if (evt == ENC_EVT_CLICK) g_sys.current_screen = SCREEN_MENU;
        break;

    case SCREEN_SAFETY_ALERT:
        if (evt == ENC_EVT_CLICK || evt == ENC_EVT_LONG_PRESS) {
            g_sys.safety_state = SAFETY_NONE;
            g_sys.run_state = STATE_IDLE;
            g_sys.current_screen = SCREEN_MAIN;
        }
        break;

default:
        break;
}
}
#endif /* BOOTLOADER_BUILD */

