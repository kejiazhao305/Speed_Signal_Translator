/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : usbd_cdc_if.c
  * @version        : v1.0_Cube
  * @brief          : USB CDC interface for STM32F411 Speed Signal Translator
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
#include "usbd_cdc_if.h"

/* USER CODE BEGIN INCLUDE */
#include <stdint.h>
#include <string.h>

/* Implemented in Src/main.c. Must quickly buffer incoming USB bytes. */
extern void App_USB_Rx(const uint8_t *data, uint32_t len);
/* USER CODE END INCLUDE */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* Private variables ---------------------------------------------------------*/
/* USER CODE END PV */

/** @addtogroup STM32_USB_OTG_DEVICE_LIBRARY
  * @brief USB device library.
  * @{
  */

/** @addtogroup USBD_CDC_IF
  * @{
  */

/** @defgroup USBD_CDC_IF_Private_TypesDefinitions Private types
  * @{
  */
/* USER CODE BEGIN PRIVATE_TYPES */
/* USER CODE END PRIVATE_TYPES */
/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Defines Private defines
  * @{
  */
/* USER CODE BEGIN PRIVATE_DEFINES */
/* USER CODE END PRIVATE_DEFINES */
/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Macros Private macros
  * @{
  */
/* USER CODE BEGIN PRIVATE_MACRO */
/* USER CODE END PRIVATE_MACRO */
/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_Variables Private variables
  * @{
  */
/* Receive and transmit buffers allocated by CubeMX. */
uint8_t UserRxBufferFS[APP_RX_DATA_SIZE];
uint8_t UserTxBufferFS[APP_TX_DATA_SIZE];

/* USER CODE BEGIN PRIVATE_VARIABLES */
/* USER CODE END PRIVATE_VARIABLES */
/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Exported_Variables Exported variables
  * @{
  */
extern USBD_HandleTypeDef hUsbDeviceFS;

/* USER CODE BEGIN EXPORTED_VARIABLES */
/* USER CODE END EXPORTED_VARIABLES */
/**
  * @}
  */

/** @defgroup USBD_CDC_IF_Private_FunctionPrototypes Function prototypes
  * @{
  */
static int8_t CDC_Init_FS(void);
static int8_t CDC_DeInit_FS(void);
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length);
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len);
static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum);

/* USER CODE BEGIN PRIVATE_FUNCTIONS_DECLARATION */
/* USER CODE END PRIVATE_FUNCTIONS_DECLARATION */
/**
  * @}
  */

USBD_CDC_ItfTypeDef USBD_Interface_fops_FS =
{
  CDC_Init_FS,
  CDC_DeInit_FS,
  CDC_Control_FS,
  CDC_Receive_FS,
  CDC_TransmitCplt_FS
};

/* Private functions ---------------------------------------------------------*/

/**
  * @brief  Initialize the CDC interface.
  * @retval USBD_OK on success
  */
static int8_t CDC_Init_FS(void)
{
  /* USER CODE BEGIN 3 */
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, 0);
  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, UserRxBufferFS);
  return (USBD_OK);
  /* USER CODE END 3 */
}

/**
  * @brief  Deinitialize the CDC interface.
  * @retval USBD_OK on success
  */
static int8_t CDC_DeInit_FS(void)
{
  /* USER CODE BEGIN 4 */
  return (USBD_OK);
  /* USER CODE END 4 */
}

/**
  * @brief  Manage CDC class-specific control requests.
  * @param  cmd     CDC request code
  * @param  pbuf    Request data buffer
  * @param  length  Request data length
  * @retval USBD_OK on success
  */
static int8_t CDC_Control_FS(uint8_t cmd, uint8_t *pbuf, uint16_t length)
{
  /* USER CODE BEGIN 5 */
  (void)pbuf;
  (void)length;

  switch (cmd)
  {
    case CDC_SEND_ENCAPSULATED_COMMAND:
      break;
    case CDC_GET_ENCAPSULATED_RESPONSE:
      break;
    case CDC_SET_COMM_FEATURE:
      break;
    case CDC_GET_COMM_FEATURE:
      break;
    case CDC_CLEAR_COMM_FEATURE:
      break;
    case CDC_SET_LINE_CODING:
      break;
    case CDC_GET_LINE_CODING:
      break;
    case CDC_SET_CONTROL_LINE_STATE:
      break;
    case CDC_SEND_BREAK:
      break;
    default:
      break;
  }

  return (USBD_OK);
  /* USER CODE END 5 */
}

/**
  * @brief  USB OUT data received on CDC endpoint.
  * @param  Buf  Incoming bytes (valid only during this callback)
  * @param  Len  Number of incoming bytes
  * @retval USBD_OK on success
  */
static int8_t CDC_Receive_FS(uint8_t *Buf, uint32_t *Len)
{
  /* USER CODE BEGIN 6 */
  if (Buf == NULL || Len == NULL)
  {
    return (USBD_FAIL);
  }

  /* Copy bytes into main.c's RX ring before rearming USB reception. */
  if (*Len > 0U)
  {
    App_USB_Rx(Buf, *Len);
  }

  USBD_CDC_SetRxBuffer(&hUsbDeviceFS, Buf);
  USBD_CDC_ReceivePacket(&hUsbDeviceFS);
  return (USBD_OK);
  /* USER CODE END 6 */
}

/**
  * @brief  Send data over USB CDC (nonblocking).
  * @note   Returns USBD_BUSY while the prior transfer is active.
  *         Copies data into a persistent buffer to avoid using a temporary
  *         local char array after the caller has returned.
  * @param  Buf  Bytes to send
  * @param  Len  Number of bytes to send
  * @retval USBD_OK, USBD_BUSY or USBD_FAIL
  */
uint8_t CDC_Transmit_FS(uint8_t *Buf, uint16_t Len)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 7 */
  if (hUsbDeviceFS.dev_state != USBD_STATE_CONFIGURED ||
      hUsbDeviceFS.pClassData == NULL)
  {
    return USBD_FAIL;
  }

  if (Buf == NULL || Len > sizeof(UserTxBufferFS))
  {
    return USBD_FAIL;
  }

  if (Len == 0U)
  {
    return USBD_OK;
  }

  USBD_CDC_HandleTypeDef *hcdc =
      (USBD_CDC_HandleTypeDef *)hUsbDeviceFS.pClassData;

  if (hcdc->TxState != 0U)
  {
    return USBD_BUSY;
  }

  /* UserTxBufferFS stays valid until the USB transfer completes. */
  memcpy(UserTxBufferFS, Buf, Len);
  USBD_CDC_SetTxBuffer(&hUsbDeviceFS, UserTxBufferFS, Len);
  result = USBD_CDC_TransmitPacket(&hUsbDeviceFS);
  /* USER CODE END 7 */
  return result;
}

/**
  * @brief  USB CDC transmit complete callback.
  * @retval USBD_OK on success
  */
static int8_t CDC_TransmitCplt_FS(uint8_t *Buf, uint32_t *Len, uint8_t epnum)
{
  uint8_t result = USBD_OK;
  /* USER CODE BEGIN 13 */
  UNUSED(Buf);
  UNUSED(Len);
  UNUSED(epnum);
  /* USER CODE END 13 */
  return result;
}

/* USER CODE BEGIN PRIVATE_FUNCTIONS_IMPLEMENTATION */
/* USER CODE END PRIVATE_FUNCTIONS_IMPLEMENTATION */

/**
  * @}
  */
/**
  * @}
  */
