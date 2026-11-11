
/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : STM32F411 USB CDC PWM Controller
 *
 * Project: UTRA Speed Signal Translator
 *
 * Features:
 * - 96 MHz system clock
 * - 48 MHz USB clock
 * - USB CDC communication with Mac
 * - TIM1 Channel 1 PWM output on PA8
 * - Adjustable PWM frequency and duty cycle
 * - TIM2 Input Capture for future speed feedback
 *
 ******************************************************************************
 */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usb_device.h"

/* USER CODE BEGIN Includes */
#include "pwm_control.h"
/* USER CODE END Includes */

/* Private variables ---------------------------------------------------------*/

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/

void SystemClock_Config(void);

static void MX_GPIO_Init(void);
static void MX_TIM1_Init(void);
static void MX_TIM2_Init(void);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/**
 * @brief Main program
 * @retval int
 */
int main(void)
{
    /* Reset peripherals and initialize HAL */
    HAL_Init();

    /* Configure system clock to 96 MHz */
    SystemClock_Config();

    /* Initialize peripherals */
    MX_GPIO_Init();

    /* Initialize TIM1 PWM */
    MX_TIM1_Init();

    /* Initialize TIM2 Input Capture */
    MX_TIM2_Init();

    /* Initialize USB CDC */
    MX_USB_DEVICE_Init();

    /* USER CODE BEGIN 2 */

    /*
     * Start PWM at:
     * Frequency = 20 kHz
     * Duty Cycle = 50%
     */
    PWM_Control_Init();

    /* USER CODE END 2 */

    /* Infinite loop */
    /* USER CODE BEGIN WHILE */
    while (1)
    {
        /* USER CODE END WHILE */

        /* USER CODE BEGIN 3 */

        /*
         * Process incoming USB CDC commands.
         *
         * Example commands:
         *
         * PWM 20000 50
         * PWM 10000 25
         * PWM 50000 75
         *
         * Format:
         * PWM <frequency_Hz> <duty_percent>
         */

        PWM_Control_Process();

        /* USER CODE END 3 */
    }
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    /* Enable power control clock */
    __HAL_RCC_PWR_CLK_ENABLE();

    /* Voltage regulator scaling */
    __HAL_PWR_VOLTAGESCALING_CONFIG(
        PWR_REGULATOR_VOLTAGE_SCALE1
    );

    /*
     * External crystal = 25 MHz
     *
     * PLLM = 25
     * PLLN = 192
     * PLLP = 2
     * PLLQ = 4
     *
     * PLL input = 25 / 25 = 1 MHz
     * VCO = 1 * 192 = 192 MHz
     *
     * SYSCLK = 192 / 2 = 96 MHz
     * USB CLK = 192 / 4 = 48 MHz
     */

    RCC_OscInitStruct.OscillatorType =
        RCC_OSCILLATORTYPE_HSE;

    RCC_OscInitStruct.HSEState =
        RCC_HSE_ON;

    RCC_OscInitStruct.PLL.PLLState =
        RCC_PLL_ON;

    RCC_OscInitStruct.PLL.PLLSource =
        RCC_PLLSOURCE_HSE;

    RCC_OscInitStruct.PLL.PLLM = 25;
    RCC_OscInitStruct.PLL.PLLN = 192;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 4;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct)
        != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * Configure CPU and peripheral clocks
     *
     * SYSCLK = 96 MHz
     * HCLK   = 96 MHz
     * PCLK1  = 48 MHz
     * PCLK2  = 96 MHz
     *
     * TIM1 clock = 96 MHz
     * TIM2 clock = 96 MHz
     */

    RCC_ClkInitStruct.ClockType =
        RCC_CLOCKTYPE_HCLK |
        RCC_CLOCKTYPE_SYSCLK |
        RCC_CLOCKTYPE_PCLK1 |
        RCC_CLOCKTYPE_PCLK2;

    RCC_ClkInitStruct.SYSCLKSource =
        RCC_SYSCLKSOURCE_PLLCLK;

    RCC_ClkInitStruct.AHBCLKDivider =
        RCC_SYSCLK_DIV1;

    RCC_ClkInitStruct.APB1CLKDivider =
        RCC_HCLK_DIV2;

    RCC_ClkInitStruct.APB2CLKDivider =
        RCC_HCLK_DIV1;

    if (HAL_RCC_ClockConfig(
            &RCC_ClkInitStruct,
            FLASH_LATENCY_3) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief TIM1 Initialization
 *
 * PWM output:
 * PA8 = TIM1 Channel 1
 *
 * Initial frequency: 20 kHz
 * Initial duty cycle: 50%
 *
 * @retval None
 */
static void MX_TIM1_Init(void)
{
    TIM_OC_InitTypeDef sConfigOC = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};

    /* Enable TIM1 peripheral clock */
    __HAL_RCC_TIM1_CLK_ENABLE();

    /*
     * TIM1 clock = 96 MHz
     *
     * Prescaler = 0
     * Period = 4799
     *
     * PWM frequency:
     *
     * 96 MHz / ((0 + 1) * (4799 + 1))
     * = 20 kHz
     */

    htim1.Instance = TIM1;

    htim1.Init.Prescaler = 0;

    htim1.Init.CounterMode =
        TIM_COUNTERMODE_UP;

    htim1.Init.Period = 4799;

    htim1.Init.ClockDivision =
        TIM_CLOCKDIVISION_DIV1;

    htim1.Init.RepetitionCounter = 0;

    htim1.Init.AutoReloadPreload =
        TIM_AUTORELOAD_PRELOAD_DISABLE;

    if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
    {
        Error_Handler();
    }

    /* Timer master configuration */

    sMasterConfig.MasterOutputTrigger =
        TIM_TRGO_RESET;

    sMasterConfig.MasterSlaveMode =
        TIM_MASTERSLAVEMODE_DISABLE;

    if (HAL_TIMEx_MasterConfigSynchronization(
            &htim1,
            &sMasterConfig) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * PWM Channel 1 configuration
     *
     * Pulse = 2400
     *
     * Duty cycle:
     * 2400 / 4800 = 50%
     */

    sConfigOC.OCMode =
        TIM_OCMODE_PWM1;

    sConfigOC.Pulse = 2400;

    sConfigOC.OCPolarity =
        TIM_OCPOLARITY_HIGH;

    sConfigOC.OCNPolarity =
        TIM_OCNPOLARITY_HIGH;

    sConfigOC.OCFastMode =
        TIM_OCFAST_DISABLE;

    sConfigOC.OCIdleState =
        TIM_OCIDLESTATE_RESET;

    sConfigOC.OCNIdleState =
        TIM_OCNIDLESTATE_RESET;

    if (HAL_TIM_PWM_ConfigChannel(
            &htim1,
            &sConfigOC,
            TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }
}

/**
 * @brief TIM2 Input Capture Initialization
 *
 * Purpose:
 * Measure incoming frequency from motor controller.
 *
 * Timer clock: 96 MHz
 * Prescaler: 95
 *
 * Counter frequency:
 *
 * 96 MHz / (95 + 1) = 1 MHz
 *
 * Timer resolution: 1 microsecond
 *
 * Input Capture Channel: 1
 *
 * @retval None
 */
static void MX_TIM2_Init(void)
{
    TIM_IC_InitTypeDef sConfigIC = {0};

    /* Enable TIM2 clock */
    __HAL_RCC_TIM2_CLK_ENABLE();

    /* Configure TIM2 */

    htim2.Instance = TIM2;

    htim2.Init.Prescaler = 95;

    htim2.Init.CounterMode =
        TIM_COUNTERMODE_UP;

    htim2.Init.Period = 0xFFFFFFFFU;

    htim2.Init.ClockDivision =
        TIM_CLOCKDIVISION_DIV1;

    htim2.Init.AutoReloadPreload =
        TIM_AUTORELOAD_PRELOAD_DISABLE;

    /* Initialize TIM2 Input Capture */

    if (HAL_TIM_IC_Init(&htim2) != HAL_OK)
    {
        Error_Handler();
    }

    /* Configure TIM2 Channel 1 */

    sConfigIC.ICPolarity =
        TIM_INPUTCHANNELPOLARITY_RISING;

    sConfigIC.ICSelection =
        TIM_ICSELECTION_DIRECTTI;

    sConfigIC.ICPrescaler =
        TIM_ICPSC_DIV1;

    sConfigIC.ICFilter = 0;

    if (HAL_TIM_IC_ConfigChannel(
            &htim2,
            &sConfigIC,
            TIM_CHANNEL_1) != HAL_OK)
    {
        Error_Handler();
    }

    /*
     * Input Capture is configured but not started.
     *
     * Future implementation:
     *
     * HAL_TIM_IC_Start_IT(
     *     &htim2,
     *     TIM_CHANNEL_1
     * );
     *
     * Configure the TIM2 input GPIO and NVIC
     * before enabling capture interrupts.
     */
}

/**
 * @brief GPIO Initialization
 *
 * PA8 = TIM1 Channel 1 PWM Output
 *
 * @retval None
 */
static void MX_GPIO_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    /* Enable GPIOA clock */
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /*
     * Configure PA8 for TIM1 PWM output
     */

    GPIO_InitStruct.Pin =
        GPIO_PIN_8;

    GPIO_InitStruct.Mode =
        GPIO_MODE_AF_PP;

    GPIO_InitStruct.Pull =
        GPIO_NOPULL;

    GPIO_InitStruct.Speed =
        GPIO_SPEED_FREQ_HIGH;

    GPIO_InitStruct.Alternate =
        GPIO_AF1_TIM1;

    HAL_GPIO_Init(
        GPIOA,
        &GPIO_InitStruct
    );
}

/**
 * @brief Error Handler
 * @retval None
 */
void Error_Handler(void)
{
    __disable_irq();

    while (1)
    {
        /* Remain here on initialization error */
    }
}

#ifdef USE_FULL_ASSERT

/**
 * @brief Assert failed
 * @param file Source file
 * @param line Line number
 */
void assert_failed(uint8_t *file,
                   uint32_t line)
{
    (void)file;
    (void)line;
}

#endif
