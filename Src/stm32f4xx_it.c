
/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt service routines
  *
  * Project: UTRA Speed Signal Translator
  * MCU: STM32F411CEU6
  *
  * TIM2_CH2: Motor speed feedback on PA1
  * USB OTG FS: USB CDC communication
  ******************************************************************************
  */
/* USER CODE END Header */

#include "main.h"
#include "stm32f4xx_it.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

/* USER CODE END Includes */

/* External variables --------------------------------------------------------*/

extern TIM_HandleTypeDef htim2;
extern PCD_HandleTypeDef hpcd_USB_OTG_FS;

/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/* Cortex-M4 Processor Interruption and Exception Handlers                    */
/******************************************************************************/

void NMI_Handler(void)
{
    while (1)
    {
    }
}

void HardFault_Handler(void)
{
    while (1)
    {
    }
}

void MemManage_Handler(void)
{
    while (1)
    {
    }
}

void BusFault_Handler(void)
{
    while (1)
    {
    }
}

void UsageFault_Handler(void)
{
    while (1)
    {
    }
}

void SVC_Handler(void)
{
}

void DebugMon_Handler(void)
{
}

void PendSV_Handler(void)
{
}

void SysTick_Handler(void)
{
    HAL_IncTick();
}

/******************************************************************************/
/* Peripheral Interrupt Handlers                                              */
/******************************************************************************/

/**
  * @brief TIM2 global interrupt handler
  *
  * The motor controller supplies feedback pulses on PA1.
  *
  * TIM2 Channel 2 captures the timer value on
  * every valid rising edge.
  *
  * HAL_TIM_IRQHandler() invokes
  * HAL_TIM_IC_CaptureCallback() in main.c.
  */
void TIM2_IRQHandler(void)
{
    /* USER CODE BEGIN TIM2_IRQn 0 */

    /* USER CODE END TIM2_IRQn 0 */

    HAL_TIM_IRQHandler(&htim2);

    /* USER CODE BEGIN TIM2_IRQn 1 */

    /* USER CODE END TIM2_IRQn 1 */
}

/**
  * @brief USB OTG FS interrupt handler
  *
  * Handles USB device communication.
  */
void OTG_FS_IRQHandler(void)
{
    /* USER CODE BEGIN OTG_FS_IRQn 0 */

    /* USER CODE END OTG_FS_IRQn 0 */

    HAL_PCD_IRQHandler(&hpcd_USB_OTG_FS);

    /* USER CODE BEGIN OTG_FS_IRQn 1 */

    /* USER CODE END OTG_FS_IRQn 1 */
}
