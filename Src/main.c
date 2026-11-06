/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : STM32F411CEU6 Speed Signal Translator
  *                   USB CDC commands -> TIM1 PWM (PA8)
  *                   TIM2 input capture (PA0) -> USB CDC frequency report
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
#include "usb_device.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "usbd_cdc_if.h"
#include "usbd_cdc.h"
#include "usbd_def.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define USB_RX_RING_SIZE       256U
#define USB_COMMAND_SIZE        64U
#define USB_TX_QUEUE_SLOTS      8U
#define USB_TX_MESSAGE_SIZE    96U
#define TIM2_TICK_FREQUENCY 1000000UL
#define STATUS_PERIOD_MS      500U
#define FEEDBACK_TIMEOUT_MS  1000U
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */
/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */
/* Defined by the CubeMX-generated USB device code (Src/usb_device.c).
   Explicit declaration fixes: 'hUsbDeviceFS' undeclared in main.c. */
extern USBD_HandleTypeDef hUsbDeviceFS;

/* USB RX: interrupt callback writes, main loop reads. */
static volatile uint8_t usb_rx_ring[USB_RX_RING_SIZE];
static volatile uint16_t usb_rx_head = 0U;
static volatile uint16_t usb_rx_tail = 0U;
static volatile uint8_t usb_rx_overflow = 0U;

/* Frequency input: capture interrupt writes, main loop reads. */
static volatile uint32_t feedback_hz = 0U;
static volatile uint32_t previous_capture = 0U;
static volatile uint32_t last_capture_ms = 0U;
static volatile uint8_t previous_capture_valid = 0U;

/* PWM duty is modified only by main loop commands. */
static uint8_t pwm_duty_percent = 0U;
static uint32_t last_status_ms = 0U;

/* USB TX: queue buffers remain valid throughout an asynchronous transfer.
   This is used from the main loop only. */
static uint8_t usb_tx_queue[USB_TX_QUEUE_SLOTS][USB_TX_MESSAGE_SIZE];
static uint16_t usb_tx_length[USB_TX_QUEUE_SLOTS];
static uint8_t usb_tx_head = 0U;
static uint8_t usb_tx_tail = 0U;
static uint8_t usb_tx_in_progress = 0U;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);
/* USER CODE BEGIN PFP */
static void PWM_SetDuty(uint8_t duty_percent);
static void USB_QueueText(const char *text);
static void USB_ServiceTx(void);
static void USB_PollCommands(void);
static void Process_Command(const char *command);
static void Send_Status(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/**
 * @brief Called by CDC_Receive_FS() in Src/usbd_cdc_if.c.
 *        Copy USB input into a ring buffer; parsing takes place in main loop.
 * @note  Runs in USB interrupt context. No blocking work or USB transmit here.
 */
void App_USB_Rx(const uint8_t *data, uint32_t len)
{
  if (data == NULL)
  {
    return;
  }

  for (uint32_t i = 0U; i < len; ++i)
  {
    uint16_t next = (uint16_t)((usb_rx_head + 1U) % USB_RX_RING_SIZE);

    if (next == usb_rx_tail)
    {
      usb_rx_overflow = 1U;
      break;  /* Do not overwrite unread data. */
    }

    usb_rx_ring[usb_rx_head] = data[i];
    usb_rx_head = next;
  }
}

/** @brief Change the TIM1 CH1 PWM duty (0-100%) on PA8. */
static void PWM_SetDuty(uint8_t duty_percent)
{
  if (duty_percent > 100U)
  {
    duty_percent = 100U;
  }

  pwm_duty_percent = duty_percent;

  /* TIM1 clock = 96 MHz; prescaler = 0; ARR = 4799 -> PWM = 20 kHz. */
  uint32_t period_ticks = __HAL_TIM_GET_AUTORELOAD(&htim1) + 1U;
  uint32_t compare_ticks = (period_ticks * duty_percent) / 100U;

  __HAL_TIM_SET_COMPARE(&htim1, TIM_CHANNEL_1, compare_ticks);
}

/** @brief Add a short message to the main-loop USB transmit queue. */
static void USB_QueueText(const char *text)
{
  if (text == NULL || hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED)
  {
    return;
  }

  size_t length = strlen(text);
  if (length == 0U || length >= USB_TX_MESSAGE_SIZE)
  {
    return;
  }

  uint8_t next = (uint8_t)((usb_tx_head + 1U) % USB_TX_QUEUE_SLOTS);
  if (next == usb_tx_tail)
  {
    return;  /* Queue full; drop this message (no blocking in firmware). */
  }

  memcpy(usb_tx_queue[usb_tx_head], text, length);
  usb_tx_length[usb_tx_head] = (uint16_t)length;
  usb_tx_head = next;
}

/** @brief Send queued messages and keep each buffer alive until TX completes. */
static void USB_ServiceTx(void)
{
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED ||
      hUsbDeviceFS.pClassData == NULL)
  {
    /* Clear stale messages after disconnect/reset. */
    usb_tx_in_progress = 0U;
    usb_tx_head = 0U;
    usb_tx_tail = 0U;
    return;
  }

  USBD_CDC_HandleTypeDef *cdc =
      (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

  if (usb_tx_in_progress != 0U)
  {
    if (cdc->TxState != 0U)
    {
      return;
    }

    /* The previous transfer is complete; release that queue slot. */
    usb_tx_in_progress = 0U;
    usb_tx_tail = (uint8_t)((usb_tx_tail + 1U) % USB_TX_QUEUE_SLOTS);
  }

  if (usb_tx_tail == usb_tx_head || cdc->TxState != 0U)
  {
    return;
  }

  if (CDC_Transmit_FS(usb_tx_queue[usb_tx_tail],
                      usb_tx_length[usb_tx_tail]) == USBD_OK)
  {
    usb_tx_in_progress = 1U;
  }
}

/** @brief Send the current PWM duty and measured input frequency. */
static void Send_Status(void)
{
  char message[USB_TX_MESSAGE_SIZE];
  uint32_t measured_hz = feedback_hz;

  (void)snprintf(message, sizeof(message),
                 "PWM=%u%% FREQ=%lu Hz\r\n",
                 (unsigned int)pwm_duty_percent,
                 (unsigned long)measured_hz);
  USB_QueueText(message);
}

/** @brief Handle a newline-terminated command from the Mac. */
static void Process_Command(const char *command)
{
  if (strcmp(command, "STOP") == 0)
  {
    PWM_SetDuty(0U);
    USB_QueueText("OK STOP\r\n");
  }
  else if (strcmp(command, "STATUS") == 0 ||
           strcmp(command, "FREQ?") == 0)
  {
    Send_Status();
  }
  else if (strcmp(command, "PING") == 0)
  {
    USB_QueueText("PONG\r\n");
  }
  else if (strncmp(command, "PWM ", 4U) == 0)
  {
    char *endptr = NULL;
    long value = strtol(command + 4, &endptr, 10);

    /* Allow trailing spaces but reject invalid or out-of-range numbers. */
    while (endptr != NULL && *endptr == ' ')
    {
      ++endptr;
    }

    if (endptr == command + 4 ||
        endptr == NULL || *endptr != '\0' ||
        value < 0L || value > 100L)
    {
      USB_QueueText("ERR PWM RANGE (0-100)\r\n");
    }
    else
    {
      PWM_SetDuty((uint8_t)value);
      USB_QueueText("OK PWM\r\n");
    }
  }
  else
  {
    USB_QueueText("ERR UNKNOWN COMMAND\r\n");
  }
}

/** @brief Assemble commands without blocking the MCU. */
static void USB_PollCommands(void)
{
  static char command[USB_COMMAND_SIZE];
  static uint8_t command_index = 0U;
  static uint8_t discard_line = 0U;

  if (usb_rx_overflow != 0U)
  {
    /* Overflow invalidates a partial command. Discard queued bytes. */
    uint32_t saved_primask = __get_PRIMASK();
    __disable_irq();
    usb_rx_tail = usb_rx_head;
    usb_rx_overflow = 0U;
    __set_PRIMASK(saved_primask);

    command_index = 0U;
    discard_line = 1U;
    USB_QueueText("ERR RX OVERFLOW\r\n");
  }

  while (usb_rx_tail != usb_rx_head)
  {
    uint8_t ch = usb_rx_ring[usb_rx_tail];
    usb_rx_tail = (uint16_t)((usb_rx_tail + 1U) % USB_RX_RING_SIZE);

    if (ch == '\r' || ch == '\n')
    {
      if (discard_line == 0U && command_index != 0U)
      {
        command[command_index] = '\0';
        Process_Command(command);
      }

      command_index = 0U;
      discard_line = 0U;
    }
    else if (discard_line != 0U)
    {
      /* Ignore input until the next newline. */
    }
    else if (command_index < USB_COMMAND_SIZE - 1U)
    {
      command[command_index++] = (char)ch;
    }
    else
    {
      command_index = 0U;
      discard_line = 1U;
      USB_QueueText("ERR LINE TOO LONG\r\n");
    }
  }
}

/**
 * @brief HAL callback for a rising edge captured on TIM2 CH1 (PA0).
 *        TIM2 runs at 1 MHz, so each tick is one microsecond.
 * @note  TIM2 is a 32-bit timer with ARR = 0xFFFFFFFF. Unsigned subtraction
 *        correctly handles one counter wrap between consecutive edges.
 */
void HAL_TIM_IC_CaptureCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM2 &&
      htim->Channel == HAL_TIM_ACTIVE_CHANNEL_1)
  {
    uint32_t current_capture =
        HAL_TIM_ReadCapturedValue(htim, TIM_CHANNEL_1);

    if (previous_capture_valid != 0U)
    {
      uint32_t delta_ticks = current_capture - previous_capture;

      if (delta_ticks > 0U)
      {
        feedback_hz = TIM2_TICK_FREQUENCY / delta_ticks;
      }
    }

    previous_capture = current_capture;
    last_capture_ms = HAL_GetTick();
    previous_capture_valid = 1U;
  }
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
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_TIM1_Init();
  MX_TIM2_Init();
  MX_USB_DEVICE_Init();

  /* USER CODE BEGIN 2 */
  PWM_SetDuty(0U);  /* Power-on default: no PWM drive. */

  if (HAL_TIM_IC_Start_IT(&htim2, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }

  if (HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    /* USER CODE BEGIN 3 */
    USB_PollCommands();

    uint32_t now = HAL_GetTick();

    /* If no rising edge was seen for 1 second, report 0 Hz. */
    if (previous_capture_valid != 0U &&
        (uint32_t)(now - last_capture_ms) >= FEEDBACK_TIMEOUT_MS)
    {
      uint32_t saved_primask = __get_PRIMASK();
      __disable_irq();

      if (previous_capture_valid != 0U &&
          (uint32_t)(now - last_capture_ms) >= FEEDBACK_TIMEOUT_MS)
      {
        feedback_hz = 0U;
        previous_capture_valid = 0U;
      }
      __set_PRIMASK(saved_primask);
    }

    if ((uint32_t)(now - last_status_ms) >= STATUS_PERIOD_MS)
    {
      last_status_ms = now;
      Send_Status();
    }

    USB_ServiceTx();
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

  /** Configure the main internal regulator output voltage */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 25;
  RCC_OscInitStruct.PLL.PLLN = 192;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
                              | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
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
  htim1.Init.Period = 4799;
  htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim1.Init.RepetitionCounter = 0;
  htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
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

  sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
  sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
  sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
  sBreakDeadTimeConfig.DeadTime = 0;
  sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
  sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
  sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
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
  TIM_IC_InitTypeDef sConfigIC = {0};

  /* USER CODE BEGIN TIM2_Init 1 */
  /* USER CODE END TIM2_Init 1 */

  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 95;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 4294967295UL;
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

  if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }

  sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }

  sConfigIC.ICPolarity = TIM_INPUTCHANNELPOLARITY_RISING;
  sConfigIC.ICSelection = TIM_ICSELECTION_DIRECTTI;
  sConfigIC.ICPrescaler = TIM_ICPSC_DIV1;
  sConfigIC.ICFilter = 0;
  if (HAL_TIM_IC_ConfigChannel(&htim2, &sConfigIC, TIM_CHANNEL_1) != HAL_OK)
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
  /* USER CODE BEGIN MX_GPIO_Init_1 */
  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();

  /* USER CODE BEGIN MX_GPIO_Init_2 */
  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

/**
  * @brief This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef USE_FULL_ASSERT
/**
  * @brief Reports the name of the source file and the source line number.
  * @param file: Source file name
  * @param line: Source line number
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  (void)file;
  (void)line;
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
