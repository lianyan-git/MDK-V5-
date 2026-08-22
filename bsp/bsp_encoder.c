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

extern void theme_apply(void);
extern void UI_DrawSettingsScreen(void);
extern void TFT_SetBrightness(uint8_t pct);

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

/* 旋转加速步进：综合"事件间隔(轮询快时按时间递增)"与"单次轮询相位累计量
 * (轮询被阻塞时编码器转过若干格→累计量大→大步进)"，上限10。与重绘/阻塞无关。 */
static uint8_t enc_accel_step = 1;
static uint32_t enc_last_rot_time = 0;

static void enc_update_accel(int8_t mag)
{
    uint32_t now = SystemTime_Millis();
    uint32_t dt = now - enc_last_rot_time;
    enc_last_rot_time = now;

    uint8_t step = enc_accel_step;
    if (dt < 100)        { if (step < 10) step++; }   /* 轮询快 + 快转：按时间递增 */
    else if (dt > 300)   { step = 1; }                 /* 停顿：复位 */
    /* 100..300ms：保持 */

    if (mag < 1) mag = 1;
    if (step < (uint8_t)mag) step = (uint8_t)mag;      /* 轮询被阻塞时按累计量取大 */
    if (step > 10) step = 10;
    enc_accel_step = step;
}

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
    /* EC11 涓€鏍?= 4 涓浉浣嶇姸鎬併€備负璁?杞竴鏍煎氨鍝嶅簲涓€娆?鏇寸伒鏁忥紝
     * 绱 卤2 鍗宠Е鍙戯紙绾﹀崐鏍硷級锛屽揩閫熸棆杞篃涓嶆紡銆?*/
    if (enc_accum >= 2) {
        int8_t mag = (int8_t)(enc_accum / 2);
        if (mag > 10) mag = 10;
        enc_accum = 0; enc_update_accel(mag); return ENC_EVT_CW;
    }
    if (enc_accum <= -2) {
        int8_t mag = (int8_t)((-enc_accum) / 2);
        if (mag > 10) mag = 10;
        enc_accum = 0; enc_update_accel(mag); return ENC_EVT_CCW;
    }
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
    case 0: /* 鐑樺共鑱斿姩 */
        p->motor_enabled = up ? 1 : 0;
        break;
    case 1: /* 杞姩鏂瑰悜 */
        p->motor_direction = up ? 1 : 0;
        break;
    case 2: /* 杞姩閫熷害 1-50 */
        if (up) { if (p->motor_speed < 50) p->motor_speed++; }
        else    { if (p->motor_speed > 1) p->motor_speed--; }
        break;
    case 3: /* 杈圭儤杈规墦 */
        p->motor_oscillate = up ? 1 : 0;
        break;
    case 4: /* 鎽嗗姩瑙掑害 1-30 (灏侀《30) */
        if (up) { if (p->motor_oscillate_angle < 30) p->motor_oscillate_angle++; }
        else    { if (p->motor_oscillate_angle > 1) p->motor_oscillate_angle--; }
        break;
    case 5: /* 椹卞姩閫夋嫨 */
        p->motor_driver = up ? MOTOR_DRIVER_TMC2209 : MOTOR_DRIVER_A4988;
        break;
    case 6: /* 椹卞姩鐢垫祦 0.2-0.6 姝ヨ繘0.1 */
        if (up) { if (p->motor_current < 6) p->motor_current++; }
        else    { if (p->motor_current > 2) p->motor_current--; }
        break;
    case 7: /* 闈欓煶 */
        p->motor_stealthchop = up ? 1 : 0;
        break;
    default: break;
    }
}

/* 从 dry_time_sec 初始化六位数字 */
static void time_init_digits(void)
{
    uint32_t h = g_sys.params.dry_time_sec / 3600;
    uint32_t m = (g_sys.params.dry_time_sec % 3600) / 60;
    uint32_t s = g_sys.params.dry_time_sec % 60;
    g_sys.time_digits[0] = (uint8_t)(h / 10);
    g_sys.time_digits[1] = (uint8_t)(h % 10);
    g_sys.time_digits[2] = (uint8_t)(m / 10);
    g_sys.time_digits[3] = (uint8_t)(m % 10);
    g_sys.time_digits[4] = (uint8_t)(s / 10);
    g_sys.time_digits[5] = (uint8_t)(s % 10);
}

/* 提交六位数字到 dry_time_sec 并保存 */
static void time_commit(void)
{
    uint32_t h = g_sys.time_digits[0] * 10 + g_sys.time_digits[1];
    uint32_t m = g_sys.time_digits[2] * 10 + g_sys.time_digits[3];
    uint32_t s = g_sys.time_digits[4] * 10 + g_sys.time_digits[5];
    g_sys.params.dry_time_sec = h * 3600 + m * 60 + s;
    System_SaveParams();
}

void Encoder_Process(void)
{
    static Screen_t last_proc_screen = (Screen_t)0xFF;
    EncoderEvent_t evt = Encoder_GetEvent();
    if (evt == ENC_EVT_NONE) return;
    /* 蜂鸣器联动：长按时只响一声（用 btn_down 边沿去重） */
    if (g_sys.buzzer_link) {
        static uint8_t last_beep_btn = 0;
        if (evt == ENC_EVT_LONG_PRESS) {
            if (btn_down != last_beep_btn) {
                last_beep_btn = btn_down;
                Buzzer_Beep(20);
            }
        } else {
            if (btn_down == 0) last_beep_btn = 0;
            Buzzer_Beep(20);
        }
    }

    if (g_sys.current_screen != last_proc_screen) {
        g_sys.prev_screen = last_proc_screen;
        last_proc_screen = g_sys.current_screen;
    }

    if (evt == ENC_EVT_LONG_PRESS) {
        if (g_sys.current_screen == SCREEN_TIME_ADJUST) {
            g_sys.selected_item = 0; g_sys.current_screen = SCREEN_MAIN;
        }
        return;
    }

    switch (g_sys.current_screen) {

    case SCREEN_TIME_ADJUST:
        if (evt == ENC_EVT_CW) { if (g_sys.time_cursor < 5) g_sys.time_cursor++; else g_sys.time_cursor = 0; }
        else if (evt == ENC_EVT_CCW) { if (g_sys.time_cursor > 0) g_sys.time_cursor--; else g_sys.time_cursor = 5; }
        else if (evt == ENC_EVT_CLICK) { g_sys.selected_item = 0; g_sys.current_screen = SCREEN_TIME_EDIT; }
        break;

    case SCREEN_TIME_EDIT: {
        static const uint8_t max_d[6] = {4, 7, 5, 9, 5, 9};
        if (evt == ENC_EVT_CW || evt == ENC_EVT_CCW) {
            int8_t delta = (evt == ENC_EVT_CW) ? 1 : -1;
            uint8_t cur = g_sys.time_cursor;
            int16_t new_val = (int16_t)g_sys.time_digits[cur] + delta;
            if (new_val < 0) new_val = (int16_t)max_d[cur];
            else if (new_val > (int16_t)max_d[cur]) new_val = 0;
            g_sys.time_digits[cur] = (uint8_t)new_val;
        } else if (evt == ENC_EVT_CLICK) {
            time_commit();
            g_sys.current_screen = SCREEN_TIME_ADJUST;
        } else if (evt == ENC_EVT_LONG_PRESS) {
            g_sys.selected_item = 0;
            g_sys.current_screen = SCREEN_MAIN;
        }
        break;
    }

default:
        break;
    }
}

#endif /* BOOTLOADER_BUILD */


