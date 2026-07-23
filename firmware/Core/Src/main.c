/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "ssd1306.h"
#include "ssd1306_tests.h" // Optional
#include "ssd1306_fonts.h"
#include <stdio.h>
#include "lfr.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;

I2C_HandleTypeDef hi2c1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_ADC1_Init(void);
static void MX_I2C1_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
uint32_t read_adc_channel(uint32_t channel) {
    ADC_ChannelConfTypeDef sConfig = {0};
    sConfig.Channel = channel;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLETIME_71CYCLES_5;
    if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK) {
        Error_Handler();
    }
    HAL_ADC_Start(&hadc1);
    HAL_ADC_PollForConversion(&hadc1, 100);
    uint32_t val = HAL_ADC_GetValue(&hadc1);
    HAL_ADC_Stop(&hadc1);
    return val;
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_I2C1_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  /* USER CODE BEGIN 2 */
  LFR_Init();
  HAL_TIM_Base_Start_IT(&htim2);

  ssd1306_Init();
  HAL_Delay(100);
  ssd1306_Fill(Black);
  ssd1306_SetCursor(14, 23);
  ssd1306_WriteString("Team Axon", Font_11x18, White);
  ssd1306_UpdateScreen();
  HAL_Delay(2000);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  typedef enum {
      STATE_MAIN_MENU,
      STATE_PID_MENU,
      STATE_TUNE_VAL,
      STATE_STOPWATCH,
      STATE_RUN_TIMES,
      STATE_TUNE_SPEED
  } MenuState_t;

  MenuState_t current_state = STATE_MAIN_MENU;
  uint8_t main_menu_index = 0;
  uint8_t pid_menu_index = 0;

  // ==========================================
  // HARDCODED DEFAULT SETTINGS
  // Change these values to set the startup PID 
  // and speed directly from the code!
  // ==========================================
  float pid_p = 0.2f;      // Proportional (Start 0.1 - 0.3)
  float pid_i = 0.0f;      // Integral (Keep at 0 until P and D are perfect)
  float pid_d = 3.0f;      // Derivative (Start 1.0 - 5.0)
  int robot_speed = 1500;  // Base speed (0 to 3599) - Lowered to 1500 for testing
  int motor_offset = 0;    // Reset offset to 0. A massive offset saturates the motors!
  // ==========================================

  uint8_t prev_up = 0;
  uint8_t prev_down = 0;
  uint8_t prev_select = 0;
  uint8_t prev_start = 0;

  uint32_t stopwatch_start_time = 0;
  uint32_t run_times[15] = {0};
  uint8_t num_run_times = 0;
  uint8_t run_times_scroll = 0;

  const char* menu_options[3] = {
      "PID Tuning",
      "Speed Tuning",
      "Run Times"
  };

  const char* pid_options[4] = {
      "Tune P",
      "Tune I",
      "Tune D",
      "Back"
  };

  while (1)
  {
      uint32_t l_sw_val = LFR_GetSwitchL();
      uint32_t r_sw_val = LFR_GetSwitchR();

      uint8_t btn_up = (l_sw_val > 400 && l_sw_val < 900) ? 1 : 0;
      uint8_t btn_down = (l_sw_val >= 900 && l_sw_val < 1350) ? 1 : 0;
      uint8_t btn_select = (l_sw_val >= 1350 && l_sw_val < 1800) ? 1 : 0;

      uint8_t btn_start = (r_sw_val > 400 && r_sw_val < 900) ? 1 : 0;
      uint8_t btn_calibrate = (r_sw_val >= 900 && r_sw_val < 1350) ? 1 : 0;

      // Start Button Logic (Stopwatch)
      if (btn_start && !prev_start) {
          if (current_state == STATE_MAIN_MENU) {
              current_state = STATE_STOPWATCH;
              stopwatch_start_time = HAL_GetTick();
              
              LFR_SetPID(pid_p, pid_i, pid_d);
              LFR_SetBaseSpeed(robot_speed);
              LFR_SetMotorOffset(motor_offset);
              LFR_Start();
          } else if (current_state == STATE_STOPWATCH) {
              LFR_Stop();
              
              uint32_t elapsed = HAL_GetTick() - stopwatch_start_time;

              // Shift history down and insert at top
              for (int i = 14; i > 0; i--) {
                  run_times[i] = run_times[i-1];
              }
              run_times[0] = elapsed;
              if (num_run_times < 15) num_run_times++;

              // Return to main menu
              current_state = STATE_MAIN_MENU;
          }
      }
      prev_start = btn_start;

      // Calibrate Button Logic
      if (btn_calibrate && current_state == STATE_MAIN_MENU) {
          LFR_Calibrate();
          
          // Make the robot rotate automatically during calibration
          LFR_SetMotors(1500, -1500);

          const char* dots_str[4] = {"", ".", "..", "..."};
          uint32_t cal_start = HAL_GetTick();
          uint32_t elapsed = 0;
          
          while ((elapsed = HAL_GetTick() - cal_start) <= 10000) {
              ssd1306_Fill(Black);

              char cal_str[20];
              sprintf(cal_str, "Calibrating%s", dots_str[(elapsed / 500) % 4]);
              ssd1306_SetCursor(18, 15);
              ssd1306_WriteString(cal_str, Font_7x10, White);

              // Draw progress bar outline
              ssd1306_DrawRectangle(14, 32, 114, 42, White);

              // Fill progress bar (max width 96)
              int fill_width = (elapsed * 96) / 10000;
              if (fill_width > 0) {
                  ssd1306_FillRectangle(16, 34, 16 + fill_width, 40, White);
              }

              // Percentage text
              char pct_str[10];
              sprintf(pct_str, "%lu%%", (elapsed * 100) / 10000);
              ssd1306_SetCursor(55, 48);
              ssd1306_WriteString(pct_str, Font_6x8, White);

              ssd1306_UpdateScreen();
          }

          // Calibration complete message
          LFR_EndCalibrate();
          
          // Stop rotating
          LFR_SetMotors(0, 0);
          
          ssd1306_Fill(Black);
          ssd1306_SetCursor(28, 25);
          ssd1306_WriteString("Complete!", Font_7x10, White);
          ssd1306_UpdateScreen();
          HAL_Delay(1500);

          // Force clear previous button states to prevent phantom clicks
          prev_up = prev_down = prev_select = 0;
          continue; // Restart the loop
      }

      // State machine for menu navigation
      if (btn_up && !prev_up) {
          if (current_state == STATE_MAIN_MENU) {
              main_menu_index = (main_menu_index == 0) ? 2 : main_menu_index - 1;
          } else if (current_state == STATE_PID_MENU) {
              pid_menu_index = (pid_menu_index == 0) ? 3 : pid_menu_index - 1;
          } else if (current_state == STATE_TUNE_VAL) {
              if (pid_menu_index == 0) pid_p += 0.1f;
              else if (pid_menu_index == 1) pid_i += 0.0001f;
              else if (pid_menu_index == 2) pid_d += 0.1f;
          } else if (current_state == STATE_RUN_TIMES) {
              if (run_times_scroll > 0) run_times_scroll--;
          } else if (current_state == STATE_TUNE_SPEED) {
              if (robot_speed <= 4080) robot_speed += 10;
              else robot_speed = 4090; // clamp
          }
      }
      if (btn_down && !prev_down) {
          if (current_state == STATE_MAIN_MENU) {
              main_menu_index = (main_menu_index == 2) ? 0 : main_menu_index + 1;
          } else if (current_state == STATE_PID_MENU) {
              pid_menu_index = (pid_menu_index == 3) ? 0 : pid_menu_index + 1;
          } else if (current_state == STATE_TUNE_VAL) {
              if (pid_menu_index == 0) pid_p -= 0.1f;
              else if (pid_menu_index == 1) pid_i -= 0.0001f;
              else if (pid_menu_index == 2) pid_d -= 0.1f;
          } else if (current_state == STATE_RUN_TIMES) {
              if (num_run_times > 4 && run_times_scroll < num_run_times - 4) {
                  run_times_scroll++;
              }
          } else if (current_state == STATE_TUNE_SPEED) {
              if (robot_speed >= 10) robot_speed -= 10;
              else robot_speed = 0; // clamp
          }
      }
      if (btn_select && !prev_select) {
          if (current_state == STATE_MAIN_MENU) {
              if (main_menu_index == 0) { // PID Tuning
                  current_state = STATE_PID_MENU;
                  pid_menu_index = 0;
              } else if (main_menu_index == 1) { // Speed Tuning
                  current_state = STATE_TUNE_SPEED;
              } else if (main_menu_index == 2) { // Run Times
                  current_state = STATE_RUN_TIMES;
                  run_times_scroll = 0;
              }
          } else if (current_state == STATE_PID_MENU) {
              if (pid_menu_index == 3) { // Back
                  current_state = STATE_MAIN_MENU;
              } else {
                  current_state = STATE_TUNE_VAL;
              }
          } else if (current_state == STATE_TUNE_VAL) {
              // Press select again to confirm and return to PID menu
              current_state = STATE_PID_MENU;
          } else if (current_state == STATE_RUN_TIMES) {
              // Press select to return to main menu
              current_state = STATE_MAIN_MENU;
          } else if (current_state == STATE_TUNE_SPEED) {
              current_state = STATE_MAIN_MENU;
          }
      }

      prev_up = btn_up;
      prev_down = btn_down;
      prev_select = btn_select;

      // Render Menu
      ssd1306_Fill(Black);

      if (current_state == STATE_MAIN_MENU || current_state == STATE_PID_MENU) {
          uint8_t num_options = (current_state == STATE_MAIN_MENU) ? 3 : 4;
          uint8_t active_idx = (current_state == STATE_MAIN_MENU) ? main_menu_index : pid_menu_index;
          const char** options_arr = (current_state == STATE_MAIN_MENU) ? menu_options : pid_options;

          // Draw shallow arc/curve on the left
          ssd1306_Line(2, 5, 6, 18, White);
          ssd1306_Line(6, 18, 10, 32, White);
          ssd1306_Line(10, 32, 6, 46, White);
          ssd1306_Line(6, 46, 2, 59, White);

          // Top Item (Previous)
          int prev_idx = (active_idx == 0) ? num_options - 1 : active_idx - 1;
          ssd1306_DrawCircle(6, 15, 2, White);
          ssd1306_SetCursor(16, 11);
          ssd1306_WriteString((char*)options_arr[prev_idx], Font_6x8, White);

          // Center Item (Selected)
          ssd1306_DrawCircle(10, 32, 3, White);
          ssd1306_FillCircle(10, 32, 1, White);
          ssd1306_Line(13, 32, 18, 32, White);

          // Highlight bar
          ssd1306_FillRectangle(18, 23, 127, 41, White);
          ssd1306_SetCursor(22, 27);
          ssd1306_WriteString((char*)options_arr[active_idx], Font_7x10, Black);

          // Bottom Item (Next)
          int next_idx = (active_idx == num_options - 1) ? 0 : active_idx + 1;
          ssd1306_DrawCircle(6, 49, 2, White);
          ssd1306_SetCursor(16, 45);
          ssd1306_WriteString((char*)options_arr[next_idx], Font_6x8, White);
      }
      else if (current_state == STATE_TUNE_VAL) {
          // Tune Value Screen
          ssd1306_SetCursor(36, 5);
          if (pid_menu_index == 0) ssd1306_WriteString("Tuning P", Font_7x10, White);
          else if (pid_menu_index == 1) ssd1306_WriteString("Tuning I", Font_7x10, White);
          else if (pid_menu_index == 2) ssd1306_WriteString("Tuning D", Font_7x10, White);

          char val_str[16];
          float val = (pid_menu_index == 0) ? pid_p : (pid_menu_index == 1) ? pid_i : pid_d;

          // Manual float to string to avoid printf %f missing library issues
          int sign = (val < 0) ? -1 : 1;
          val *= sign;
          int int_part = (int)val;

          if (pid_menu_index == 1) { // 4 decimals for I
              int frac_part = (int)((val - int_part) * 10000.0f + 0.5f);
              if (frac_part >= 10000) { int_part++; frac_part -= 10000; }
              sprintf(val_str, "%s%d.%04d", (sign < 0) ? "-" : "", int_part, frac_part);
          } else { // 1 decimal for P and D
              int frac_part = (int)((val - int_part) * 10.0f + 0.5f);
              if (frac_part >= 10) { int_part++; frac_part -= 10; }
              sprintf(val_str, "%s%d.%01d", (sign < 0) ? "-" : "", int_part, frac_part);
          }

          // Draw neat bounding box
          ssd1306_DrawRectangle(10, 20, 118, 48, White);
          ssd1306_SetCursor(20, 27);
          ssd1306_WriteString(val_str, Font_11x18, White);

          // Draw UP / DOWN hints
          ssd1306_SetCursor(13, 54);
          ssd1306_WriteString("[UP/DN to adjust]", Font_6x8, White);
      }
      else if (current_state == STATE_TUNE_SPEED) {
          // Tune Speed Screen
          ssd1306_SetCursor(22, 5);
          ssd1306_WriteString("Tuning Speed", Font_7x10, White);

          char val_str[16];
          int len = sprintf(val_str, "%d", robot_speed);

          // Draw neat bounding box
          ssd1306_DrawRectangle(10, 20, 118, 48, White);

          // Dynamically center the number based on its length
          int x_pos = 10 + (108 - (len * 11)) / 2;
          ssd1306_SetCursor(x_pos, 27);
          ssd1306_WriteString(val_str, Font_11x18, White);

          // Draw UP / DOWN hints
          ssd1306_SetCursor(13, 54);
          ssd1306_WriteString("[UP/DN to adjust]", Font_6x8, White);
      }
      else if (current_state == STATE_STOPWATCH) {
          // Stopwatch Screen
          ssd1306_SetCursor(38, 5);
          ssd1306_WriteString("STOPWATCH", Font_7x10, White);

          uint32_t elapsed = HAL_GetTick() - stopwatch_start_time;
          uint32_t ms = elapsed % 1000;
          uint32_t sec = (elapsed / 1000) % 60;
          uint32_t min = (elapsed / 60000);

          char time_str[16];
          sprintf(time_str, "%02lu:%02lu.%03lu", min, sec, ms);

          // Draw neat bounding box
          ssd1306_DrawRectangle(10, 20, 118, 48, White);
          ssd1306_SetCursor(18, 27);
          ssd1306_WriteString(time_str, Font_11x18, White);

          ssd1306_SetCursor(16, 54);
          ssd1306_WriteString("[START to return]", Font_6x8, White);
      }
      else if (current_state == STATE_RUN_TIMES) {
          ssd1306_SetCursor(32, 0);
          ssd1306_WriteString("RUN TIMES", Font_7x10, White);
          ssd1306_Line(0, 12, 127, 12, White);

          if (num_run_times == 0) {
              ssd1306_SetCursor(10, 30);
              ssd1306_WriteString("No runs yet!", Font_7x10, White);
          } else {
              for (int i = 0; i < 4; i++) {
                  int idx = run_times_scroll + i;
                  if (idx >= num_run_times) break;

                  uint32_t elapsed = run_times[idx];
                  uint32_t ms = elapsed % 1000;
                  uint32_t sec = (elapsed / 1000) % 60;
                  uint32_t min = (elapsed / 60000);

                  char time_str[24];
                  sprintf(time_str, "%2d.%02lu:%02lu.%03lu", idx + 1, min, sec, ms);

                  ssd1306_SetCursor(5, 16 + i * 11);
                  ssd1306_WriteString(time_str, Font_7x10, White);
              }

              // Draw scroll indicator if list overflows
              if (num_run_times > 4) {
                  int scroll_y = 15 + (run_times_scroll * 30) / (num_run_times - 4);
                  ssd1306_DrawRectangle(124, 15, 127, 55, White);
                  ssd1306_FillRectangle(124, scroll_y, 127, scroll_y + 10, White);
              }
          }

          ssd1306_SetCursor(13, 56);
          ssd1306_WriteString("[SELECT to return]", Font_6x8, White);
      }

      ssd1306_UpdateScreen();
      HAL_Delay(50); // Small delay for button debouncing
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief ADC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_ADC1_Init(void)
{

  /* USER CODE BEGIN ADC1_Init 0 */

  /* USER CODE END ADC1_Init 0 */

  ADC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN ADC1_Init 1 */

  /* USER CODE END ADC1_Init 1 */

  /** Common config
  */
  hadc1.Instance = ADC1;
  hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
  hadc1.Init.ContinuousConvMode = DISABLE;
  hadc1.Init.DiscontinuousConvMode = DISABLE;
  hadc1.Init.ExternalTrigConv = ADC_SOFTWARE_START;
  hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
  hadc1.Init.NbrOfConversion = 1;
  if (HAL_ADC_Init(&hadc1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Regular Channel
  */
  sConfig.Channel = ADC_CHANNEL_0;
  sConfig.Rank = ADC_REGULAR_RANK_1;
  sConfig.SamplingTime = ADC_SAMPLETIME_1CYCLE_5;
  if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN ADC1_Init 2 */

  /* USER CODE END ADC1_Init 2 */

}

/**
  * @brief I2C1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C1_Init(void)
{

  /* USER CODE BEGIN I2C1_Init 0 */

  /* USER CODE END I2C1_Init 0 */

  /* USER CODE BEGIN I2C1_Init 1 */

  /* USER CODE END I2C1_Init 1 */
  hi2c1.Instance = I2C1;
  hi2c1.Init.ClockSpeed = 100000;
  hi2c1.Init.DutyCycle = I2C_DUTYCYCLE_2;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief TIM1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM1_Init(void)
{

  /* USER CODE BEGIN TIM1_Init 0 */

  /* USER CODE END TIM1_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};
  TIM_OC_InitTypeDef sConfigOC = {0};
  TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

  /* USER CODE BEGIN TIM1_Init 1 */

  /* USER CODE END TIM1_Init 1 */
  htim1.Instance = TIM1;
  htim1.Init.Prescaler = 0;
  htim1.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim1.Init.Period = 3599;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
  if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sConfigOC.OCMode = TIM_OCMODE_PWM1;
  sConfigOC.Pulse = 0;
  sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
  sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
  sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
  sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
  sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
  {
    Error_Handler();
  }
  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_ENABLE;
  if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM1_Init 2 */

  /* USER CODE END TIM1_Init 2 */
  HAL_TIM_MspPostInit(&htim1);

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 71;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 999;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, GPIO_PIN_0|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15, GPIO_PIN_RESET);

  /*Configure GPIO pins : PA5 PA6 PA7 */
  GPIO_InitStruct.Pin = GPIO_PIN_5|GPIO_PIN_6|GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pins : PB0 PB12 PB13 PB14
                           PB15 */
  GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_12|GPIO_PIN_13|GPIO_PIN_14
                          |GPIO_PIN_15;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim) {
    if (htim->Instance == TIM2) {
        LFR_ControlLoop();
    }
}
/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
