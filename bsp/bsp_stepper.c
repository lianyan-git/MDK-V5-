#ifndef BOOTLOADER_BUILD
#include "bsp_stepper.h"
#include "pin_config.h"
#include "stm32f10x.h"
#include "system_time.h"

#define STEP_PERIOD_US 1000

static volatile uint8_t stepper_enabled = 0;
static volatile uint8_t motor_running = 0;
static volatile int32_t motor_remaining_steps = 0;
static volatile uint8_t motor_direction_cw = 1;
static volatile uint16_t step_period_us = STEP_PERIOD_US;
static volatile uint32_t last_step_time = 0;
static volatile int32_t target_steps = 0;

void Stepper_Init(void)
{
    GPIO_InitTypeDef g;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    g.GPIO_Pin = PIN_STEP_EN_PIN | PIN_STEP_STEP_PIN | PIN_STEP_DIR_PIN;
    g.GPIO_Mode = GPIO_Mode_Out_PP;
    g.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(PIN_STEP_EN_PORT, &g);

    GPIO_SetBits(PIN_STEP_EN_PORT, PIN_STEP_EN_PIN);   // 默认禝用 (高电平有效使�?
    GPIO_ResetBits(PIN_STEP_EN_PORT, PIN_STEP_STEP_PIN);
    GPIO_ResetBits(PIN_STEP_EN_PORT, PIN_STEP_DIR_PIN);
}

void Stepper_Enable(uint8_t enable)
{
    stepper_enabled = enable;
    if (enable) {
        GPIO_ResetBits(PIN_STEP_EN_PORT, PIN_STEP_EN_PIN);  // 使能电机
    } else {
        GPIO_SetBits(PIN_STEP_EN_PORT, PIN_STEP_EN_PIN);    // 关闭电机
        motor_running = 0;
        motor_remaining_steps = 0;
    }
}

static void set_dir(uint8_t fwd)
{
    motor_direction_cw = fwd;
    if (fwd) GPIO_SetBits(PIN_STEP_EN_PORT, PIN_STEP_DIR_PIN);
    else GPIO_ResetBits(PIN_STEP_EN_PORT, PIN_STEP_DIR_PIN);
}

void Stepper_SetSpeed(uint16_t steps_per_sec)
{
    if (steps_per_sec == 0) steps_per_sec = 1;
    uint32_t period = 1000000UL / steps_per_sec;
    if (period < 50) period = 50;
    step_period_us = (uint16_t)period;
}

void Stepper_Move(int32_t steps)
{
    if (!stepper_enabled) return;
    if (steps > 0) {
        set_dir(1);
        motor_remaining_steps = steps;
    } else if (steps < 0) {
        set_dir(0);
        motor_remaining_steps = steps;
    } else {
        motor_remaining_steps = 0;
    }
    if (motor_remaining_steps != 0) {
        motor_running = 1;
        last_step_time = SystemTime_Millis();
    }
}

void Stepper_SetOscillate(int32_t steps)
{
    target_steps = steps;
    /* ?????????????????????????? */
    if (stepper_enabled && target_steps != 0 && !motor_running) {
        motor_running = 1;
        last_step_time = SystemTime_Millis();
        set_dir(target_steps < 0 ? 0 : 1);
        motor_remaining_steps = target_steps;
    }
}

void Stepper_Update(void)
{
    if (!stepper_enabled || !motor_running) {
        return;
    }

    uint32_t now_ms = SystemTime_Millis();
    if ((uint32_t)(now_ms - last_step_time) < ((step_period_us + 999) / 1000)) {
        return;
    }

    GPIO_SetBits(PIN_STEP_EN_PORT, PIN_STEP_STEP_PIN);
    __NOP(); __NOP(); __NOP();
    GPIO_ResetBits(PIN_STEP_EN_PORT, PIN_STEP_STEP_PIN);
    last_step_time = now_ms;

    if (motor_remaining_steps > 0) motor_remaining_steps--;
    else if (motor_remaining_steps < 0) motor_remaining_steps++;

    if (motor_remaining_steps == 0) {
        motor_running = 0;
        if (target_steps < 0) {
            target_steps++;
            Stepper_Move(1);
        } else if (target_steps > 0) {
            target_steps--;
            Stepper_Move(-1);
        }
    }
}

uint8_t Stepper_IsRunning(void)
{
    return motor_running;
}
#endif /* BOOTLOADER_BUILD */

