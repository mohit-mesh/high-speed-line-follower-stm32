#include "lfr.h"

// External handles from main.c
extern ADC_HandleTypeDef hadc1;
extern TIM_HandleTypeDef htim1;

// Configuration
#define SENSOR_COUNT 16
#define BLACK_LINE 1  // 1 if black line on white surface, 0 if white line on black surface

// Global variables for Live Expressions (volatile to prevent optimization and ensure ISR visibility)
volatile uint16_t sensor_min[SENSOR_COUNT];
volatile uint16_t sensor_max[SENSOR_COUNT];
volatile uint16_t sensor_raw[SENSOR_COUNT];
volatile uint32_t sensor_calibrated[SENSOR_COUNT];

volatile float lfr_position = 7500.0f;
volatile float lfr_error = 0.0f;

static uint8_t is_running = 0;
static uint8_t is_calibrating = 0;

static float Kp = 0.0f;
static float Ki = 0.0f;
static float Kd = 0.0f;
static int base_speed = 2000;
static int motor_bias = 0;

static float integral = 0.0f;
static float prev_error = 0.0f;

// Switch states read by ISR
volatile uint32_t lfr_switch_l = 0;
volatile uint32_t lfr_switch_r = 0;

// MUX pin configuration (S0=PB0, S1=PA7, S2=PA6, S3=PA5)
static void Select_Mux_Channel(uint8_t channel) {
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0, (channel & 0x01) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_7, (channel & 0x02) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_6, (channel & 0x04) ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5, (channel & 0x08) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

// Fast ADC read for a specific pre-configured channel
static uint32_t Read_ADC_Fast(void) {
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 10);
    uint32_t val = HAL_ADC_GetValue(&hadc1);
    // Don't stop ADC if we are doing many reads in sequence, but standard HAL practice uses Stop
    HAL_ADC_Stop(&hadc1); 
    return val;
}

// Configure ADC for a specific channel
static void Config_ADC_Channel(uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5; // Use 1_CYCLE_5 for faster reads if needed
    HAL_ADC_ConfigChannel(&hadc1, &sConfig);
}

void LFR_Init(void) {
    // Initialize default min/max so range is valid even without calibration
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sensor_min[i] = 0;
        sensor_max[i] = 4095;
    }
    
    // Start PWM
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1); // PWMA
    HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2); // PWMB
    
    // Ensure motors are stopped initially
    LFR_SetMotors(0, 0);
}


void LFR_SetMotors(int16_t left_speed, int16_t right_speed) {
    // Left Motor (Motor A)
    // AIN1 = PB13, AIN2 = PB12
    if (left_speed > 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
    } else if (left_speed < 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_SET);
        left_speed = -left_speed;
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_13, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_12, GPIO_PIN_RESET);
    }
    
    if (left_speed > LFR_MAX_PWM) left_speed = LFR_MAX_PWM;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, left_speed); // PWMA (PA8)

    // Right Motor (Motor B)
    // BIN1 = PB14, BIN2 = PB15
    if (right_speed > 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_SET);
    } else if (right_speed < 0) {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
        right_speed = -right_speed;
    } else {
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_14, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOB, GPIO_PIN_15, GPIO_PIN_RESET);
    }
    
    if (right_speed > LFR_MAX_PWM) right_speed = LFR_MAX_PWM;
    __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_2, right_speed); // PWMB (PA9)
}

void LFR_SetPID(float kp, float ki, float kd) {
    Kp = kp;
    Ki = ki;
    Kd = kd;
}

void LFR_SetBaseSpeed(int speed) {
    base_speed = speed;
}

void LFR_SetMotorOffset(int offset) {
    motor_bias = offset;
}

void LFR_Calibrate(void) {
    is_calibrating = 1;
    // Reset min/max
    for (int i = 0; i < SENSOR_COUNT; i++) {
        sensor_min[i] = 4095;
        sensor_max[i] = 0;
    }
}

void LFR_EndCalibrate(void) {
    is_calibrating = 0;
}

void LFR_Start(void) {
    is_running = 1;
    integral = 0.0f;
    prev_error = 0.0f;
}

void LFR_Stop(void) {
    LFR_SetMotors(0, 0);
    is_running = 0;
}

uint8_t LFR_IsRunning(void) {
    return is_running;
}

uint32_t LFR_GetSwitchL(void) {
    return lfr_switch_l;
}

uint32_t LFR_GetSwitchR(void) {
    return lfr_switch_r;
}

// 1.5 kHz Control Loop called from TIM2 Interrupt
void LFR_ControlLoop(void) {
    // 1. Read Switches
    Config_ADC_Channel(ADC_CHANNEL_0);
    lfr_switch_l = Read_ADC_Fast();
    
    Config_ADC_Channel(ADC_CHANNEL_1);
    lfr_switch_r = Read_ADC_Fast();

    // 2. Read Sensors (Channel 4 = PA4)
    Config_ADC_Channel(ADC_CHANNEL_4);
    
    for (int i = 0; i < SENSOR_COUNT; i++) {
        Select_Mux_Channel(i);
        
        // Delay for a few microseconds to allow the RC circuit of the multiplexer/sensor to settle
        for (volatile int d = 0; d < 100; d++);
        
        sensor_raw[i] = Read_ADC_Fast();
        
        if (is_calibrating) {
            if (sensor_raw[i] < sensor_min[i]) sensor_min[i] = sensor_raw[i];
            if (sensor_raw[i] > sensor_max[i]) sensor_max[i] = sensor_raw[i];
        }
    }
    
    // Stop here if not running
    if (!is_running) {
        return;
    }
    
    // 3. Process Signals
    uint32_t sum = 0;
    uint32_t weighted_sum = 0;
    
    for (int i = 0; i < SENSOR_COUNT; i++) {
        int32_t val = sensor_raw[i];
        
        // Normalize
        int32_t range = sensor_max[i] - sensor_min[i];
        if (range <= 0) range = 1; // Prevent division by zero
        
        val = ((val - sensor_min[i]) * 1000) / range;
        
        if (val < 0) val = 0;
        if (val > 1000) val = 1000;
        
        if (BLACK_LINE) {
            // User hardware: Black line gives lower raw ADC values (0-1900).
            // We invert it so that the black line gives a high value (~1000) for the weighted average.
            val = 1000 - val; 
        } else {
            // White line gives higher raw ADC values (~3700-3900). No need to invert.
            // val = val;
        }
        
        // Noise filter threshold
        // Lowered to 50 to allow smooth interpolation of the line position and prevent
        // harsh discrete steps that cause derivative spikes and wobbling.
        if (val < 50) val = 0;
        
        sensor_calibrated[i] = val;
        
        sum += val;
        weighted_sum += val * (i * 1000);
    }
    
    // 4. Calculate Error
    static float last_position = 7500.0f;
    static uint16_t line_lost_count = 0;
    
    if (sum == 0) {
        line_lost_count++;
        // Line lost — keep turning in the direction it was last seen,
        // by holding the last known position. Forcing to 0 or 15000 causes
        // massive derivative spikes and overturning.
        lfr_position = last_position;
    } else {
        line_lost_count = 0;
        lfr_position = (float)weighted_sum / (float)sum;
        last_position = lfr_position;
    }
    
    lfr_error = lfr_position - 7500.0f;
    
    // 5. PID Calculation
    integral += lfr_error;
    if (integral > 100000.0f) integral = 100000.0f;
    if (integral < -100000.0f) integral = -100000.0f;
    
    static float derivative = 0.0f;
    float raw_derivative = lfr_error - prev_error;
    derivative = (0.7f * derivative) + (0.3f * raw_derivative);
    
    float turn = (Kp * lfr_error) + (Ki * integral) + (Kd * derivative);
    
    prev_error = lfr_error;
    
    // 6. Apply to Motors
    // 
    // KEY IDEA: Slow down the ENTIRE robot on curves, not just the inner wheel.
    // This prevents the outer wheel from driving the robot in a fast arc that
    // overshoots the turn and causes a 180-degree spin.
    
    // Basic Differential Drive
    int16_t current_speed = base_speed;

    // High-Speed Corner Braking:
    // We now use the raw position error to detect corners, rather than the PID 'turn' value.
    // This prevents the braking logic from changing every time you tune Kp!
    float abs_error = (lfr_error > 0) ? lfr_error : -lfr_error;
    if (abs_error > 2500.0f) { // Line is drifting away from the center sensors
        current_speed -= (int16_t)((abs_error - 2500.0f) * 0.5f); 
        if (current_speed < 800) current_speed = 800; // Slam the brakes on sharp corners
    }

    // Limit turn to prevent violent 180-degree overturns on sharp corners.
    // This allows the inner wheel to reverse up to -2500 PWM, giving a very sharp
    // pivot.
    float max_turn = (float)current_speed + 2500.0f;
    if (turn > max_turn) turn = max_turn;
    if (turn < -max_turn) turn = -max_turn;

    int16_t left_speed = current_speed - (int16_t)turn + motor_bias;
    int16_t right_speed = current_speed + (int16_t)turn - motor_bias;
    
    LFR_SetMotors(left_speed, right_speed);
}
