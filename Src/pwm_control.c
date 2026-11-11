
#include "pwm_control.h"
#include "main.h"

#include <stdio.h>
#include <string.h>

extern TIM_HandleTypeDef htim1;

// USB receive ring buffer
#define RX_SIZE 256U
#define LINE_SIZE 64U

static uint8_t rx_buffer[RX_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

static char command_line[LINE_SIZE];
static uint32_t command_length = 0;
static uint8_t line_overflow = 0;

// Determine actual TIM1 clock from RCC
static uint32_t TIM1_GetClock(void)
{
    uint32_t pclk = HAL_RCC_GetPCLK2Freq();

    uint32_t prescaler =
        (RCC->CFGR & RCC_CFGR_PPRE2)
        >> RCC_CFGR_PPRE2_Pos;

    // If APB2 prescaler is 1, TIM1 clock = PCLK2
    // Otherwise, TIM1 clock = 2 * PCLK2
    if (prescaler < 4U) {
        return pclk;
    }

    return 2U * pclk;
}

int PWM_Control_Set(uint32_t frequency,
                    uint32_t duty)
{
    if (frequency < 10U ||
        frequency > 100000U ||
        duty > 100U) {
        return 0;
    }

    uint64_t timer_clock = TIM1_GetClock();

    // Calculate prescaler to keep ARR within 16 bits
    uint64_t divider =
        (timer_clock +
         (uint64_t)frequency * 65536U - 1U)
        / ((uint64_t)frequency * 65536U);

    if (divider < 1U) {
        divider = 1U;
    }

    if (divider > 65536U) {
        return 0;
    }

    uint32_t psc = (uint32_t)(divider - 1U);

    // Round to nearest timer period
    uint64_t denominator =
        (uint64_t)(psc + 1U) * frequency;

    uint32_t period =
        (uint32_t)((timer_clock + denominator / 2U)
                   / denominator);

    if (period < 2U || period > 65536U) {
        return 0;
    }

    uint32_t arr = period - 1U;

    uint32_t compare =
        (uint32_t)(((uint64_t)period * duty + 50U)
                   / 100U);

    // Stop output before changing timer settings
    HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);

    __HAL_TIM_SET_PRESCALER(&htim1, psc);
    __HAL_TIM_SET_AUTORELOAD(&htim1, arr);
    __HAL_TIM_SET_COMPARE(&htim1,
                          TIM_CHANNEL_1,
                          compare);

    __HAL_TIM_SET_COUNTER(&htim1, 0);

    // Load new prescaler, ARR and CCR values
    htim1.Instance->EGR = TIM_EGR_UG;

    // Restart hardware PWM
    if (HAL_TIM_PWM_Start(&htim1,
                         TIM_CHANNEL_1) != HAL_OK) {
        return 0;
    }

    return 1;
}

void PWM_Control_Init(void)
{
    PWM_Control_Set(20000U, 50U);
}

// Called from USB CDC receive callback
void PWM_Control_USB_Receive(const uint8_t *data,
                             uint32_t length)
{
    for (uint32_t i = 0; i < length; i++) {

        uint16_t next =
            (uint16_t)((rx_head + 1U) % RX_SIZE);

        // Drop incoming bytes if ring buffer is full
        if (next == rx_tail) {
            break;
        }

        rx_buffer[rx_head] = data[i];
        rx_head = next;
    }
}

static void Process_Command(const char *line)
{
    unsigned long frequency = 0;
    unsigned long duty = 0;
    char extra;

    // Expected input: PWM 20000 50
    int fields = sscanf(line,
                        "PWM %lu %lu %c",
                        &frequency,
                        &duty,
                        &extra);

    if (fields == 2) {
        PWM_Control_Set((uint32_t)frequency,
                        (uint32_t)duty);
    }
}

void PWM_Control_Process(void)
{
    while (rx_tail != rx_head) {

        char c = (char)rx_buffer[rx_tail];

        rx_tail =
            (uint16_t)((rx_tail + 1U) % RX_SIZE);

        // Ignore carriage return
        if (c == '\r') {
            continue;
        }

        // Newline finishes a command
        if (c == '\n') {

            if (!line_overflow &&
                command_length > 0U) {

                command_line[command_length] = '\0';
                Process_Command(command_line);
            }

            command_length = 0;
            line_overflow = 0;
            continue;
        }

        if (!line_overflow) {

            if (command_length < LINE_SIZE - 1U) {
                command_line[command_length++] = c;
            } else {
                line_overflow = 1;
            }
        }
    }
}
