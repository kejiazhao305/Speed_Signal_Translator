
#include "pwm_control.h"
#include "main.h"

#include "usbd_cdc_if.h"
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

extern TIM_HandleTypeDef htim1;

// USB receive ring buffer
#define RX_SIZE 256U
#define LINE_SIZE 64U

static uint8_t rx_buffer[RX_SIZE];
static volatile uint16_t rx_head = 0;
static volatile uint16_t rx_tail = 0;

static volatile uint8_t rx_overrun;
static char reply[112];
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
         (uint64_t)frequency * 65535U - 1U)
        / ((uint64_t)frequency * 65535U);

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

    // Load all three shadow registers together on the next natural update.
    SET_BIT(TIM1->CR1, TIM_CR1_UDIS | TIM_CR1_ARPE);
    SET_BIT(TIM1->CCMR1, TIM_CCMR1_OC1PE);
    __HAL_TIM_SET_PRESCALER(&htim1, psc);
    __HAL_TIM_SET_AUTORELOAD(&htim1, arr);
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare);
    CLEAR_BIT(TIM1->CR1, TIM_CR1_UDIS);
    return 1;
}

void PWM_Control_Init(void)
{
    if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK) Error_Handler();
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
            rx_overrun = 1;
            break;
        }

        rx_buffer[rx_head] = data[i];
        rx_head = next;
    }
}

static void Process_Command(const char *line)
{
    if (strcmp(line, "RPM?") == 0) {
        Speed_Report(reply, sizeof(reply));
        return;
    }
    strcpy(reply, "ERROR expected PWM <10-100000 Hz> <0-100 percent> or RPM?\r\n");
    if (strncmp(line, "PWM ", 4) != 0) return;
    char *end;
    const char *arg = line + 4;
    while (*arg == ' ') arg++;
    if (*arg < '0' || *arg > '9') return;
    errno = 0;
    unsigned long frequency = strtoul(arg, &end, 10);
    if (errno || *end != ' ' || frequency > 100000UL) return;
    while (*end == ' ') end++;
    if (*end < '0' || *end > '9') return;
    unsigned long duty = strtoul(end, &end, 10);
    if (*end || errno || duty > 100UL) return;
    if (PWM_Control_Set(frequency, duty)) strcpy(reply, "OK\r\n");
}

void PWM_Control_Process(void)
{
    if (*reply) {
        __disable_irq();
        uint8_t result = CDC_Transmit_FS((uint8_t *)reply, strlen(reply));
        __enable_irq();
        if (result == USBD_BUSY) return;
        reply[0] = 0;
    }
    while (rx_tail != rx_head) {
        __disable_irq();
        if (rx_overrun) {
            rx_tail = rx_head;
            rx_overrun = 0;
            command_length = 0;
            line_overflow = 1;
            __enable_irq();
            strcpy(reply, "ERROR receive overflow; send newline to resync\r\n");
            return;
        }
        __enable_irq();

        char c = (char)rx_buffer[rx_tail];

        rx_tail =
            (uint16_t)((rx_tail + 1U) % RX_SIZE);

        // Ignore carriage return
        if (c == '\r') {
            c = '\n';
        }

        // Newline finishes a command
        if (c == '\n') {

            if (!line_overflow &&
                command_length > 0U) {

                command_line[command_length] = '\0';
                Process_Command(command_line);
            }

            if (line_overflow) strcpy(reply, "ERROR invalid or oversized line\r\n");
            command_length = 0;
            line_overflow = 0;
            if (*reply) return;
            continue;
        }

        if (!line_overflow) {

            if (c >= ' ' && c <= '~' && command_length < LINE_SIZE - 1U) {
                command_line[command_length++] = c;
            } else {
                line_overflow = 1;
            }
        }
    }
}
