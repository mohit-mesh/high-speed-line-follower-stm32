#ifndef __LFR_H
#define __LFR_H

#include "main.h"

// Define max PWM based on TIM1 period (3599 in main.c)
#define LFR_MAX_PWM 3599

// Initialize LFR hardware and data
void LFR_Init(void);

// Trigger a calibration sequence
void LFR_Calibrate(void);
void LFR_EndCalibrate(void);

// Start the line following control loop
void LFR_Start(void);

// Stop the line following control loop
void LFR_Stop(void);

// Update PID parameters
void LFR_SetPID(float kp, float ki, float kd);

// Set base speed
void LFR_SetBaseSpeed(int speed);

// Set motor trim offset (to fix physical veering/tilting)
void LFR_SetMotorOffset(int offset);

// Motor control functions
void LFR_SetMotors(int16_t left_speed, int16_t right_speed);

// Timer callback for the 1.5 kHz loop
void LFR_ControlLoop(void);

// Get the latest switch values read by the interrupt
uint32_t LFR_GetSwitchL(void);
uint32_t LFR_GetSwitchR(void);

// Check if currently running
uint8_t LFR_IsRunning(void);

#endif /* __LFR_H */
