
#ifndef PWM_CONTROL_H
#define PWM_CONTROL_H

#include <stdint.h>

// Call once after TIM1 initialization
void PWM_Control_Init(void);

// Receive USB CDC bytes
void PWM_Control_USB_Receive(const uint8_t *data,
                             uint32_t length);

// Process commands in the main loop
void PWM_Control_Process(void);

// Set PWM frequency and duty cycle
// Returns 1 on success, 0 on invalid input
int PWM_Control_Set(uint32_t frequency,
                    uint32_t duty);

#endif
