
/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : usbd_cdc_if.h
 * @version        : v1.0_Cube
 * @brief          : Header for usbd_cdc_if.c file.
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

/* Define to prevent recursive inclusion -------------------------------------*/
#ifndef __USBD_CDC_IF_H__
#define __USBD_CDC_IF_H__

#ifdef __cplusplus
extern "C" {
#endif

/* Includes ------------------------------------------------------------------*/
#include "usbd_cdc.h"

/* USER CODE BEGIN INCLUDE */
#include "pwm_control.h"
/* USER CODE END INCLUDE */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
 * @brief For USB device.
 * @{
 */

/** @defgroup USBD_CDC_IF USBD_CDC_IF
 * @brief USB VCP device module
 * @{
 */

/** @defgroup USBD_CDC_IF_Exported_Defines
 * @{
 */

#define APP_RX_DATA_SIZE 2048
#define APP_TX_DATA_SIZE 2048

/* USER CODE BEGIN EXPORTED_DEFINES */

/* USER CODE END EXPORTED_DEFINES */

/**
 * @}
 */

/** @defgroup USBD_CDC_IF_Exported_Types
 * @{
 */

/* USER CODE BEGIN EXPORTED_TYPES */

/* USER CODE END EXPORTED_TYPES */

/**
 * @}
 */

/** @defgroup USBD_CDC_IF_Exported_Macros
 * @{
 */

/* USER CODE BEGIN EXPORTED_MACRO */

/* USER CODE END EXPORTED_MACRO */

/**
 * @}
 */

/** @defgroup USBD_CDC_IF_Exported_Variables
 * @{
 */

extern USBD_CDC_ItfTypeDef USBD_Interface_fops_FS;

/* USER CODE BEGIN EXPORTED_VARIABLES */

/* USER CODE END EXPORTED_VARIABLES */

/**
 * @}
 */

/** @defgroup USBD_CDC_IF_Exported_FunctionsPrototype
 * @{
 */

uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len);

/* USER CODE BEGIN EXPORTED_FUNCTIONS */

/* USER CODE END EXPORTED_FUNCTIONS */

/**
 * @}
 */

/**
 * @}
 */

/**
 * @}
 */

#ifdef __cplusplus
}
#endif

#endif /* __USBD_CDC_IF_H__ */
