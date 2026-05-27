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
#include "adc.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include <stdlib.h>
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

/* USER CODE BEGIN PV */
/* 7-segment common-anode code table (0-9), PB0=a, PB1=b, ..., PB7=dp */
const uint8_t SEG_CODE[] = {
    0xC0, 0xF9, 0xA4, 0xB0, 0x99, 0x92, 0x82, 0xF8, 0x80, 0x90
};

volatile uint16_t adc_val;
volatile float temperature = 25.0f;
volatile float alarm_high = 50.0f;
volatile float alarm_low  = 20.0f;
volatile uint8_t display_mode = 0;   // 0: temperature, 1: upper alarm, 2: lower alarm
volatile uint8_t digit_index  = 0;
volatile uint16_t blink_tick  = 0;
volatile uint8_t uart_rx_byte = 0;   // UART receive buffer
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */
float ADC_Read_Temperature(void);
uint8_t Key_Scan(void);
void Display_Scan(uint8_t pos);
void Alarm_Check(void);
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

/* Send a string via UART2 using HAL (bypasses newlib retargeting) */
static void uart_send(const char *p)
{
    while (*p)
    {
        while ((USART2->SR & USART_SR_TXE) == 0);
        USART2->DR = (uint8_t)*p++;
    }
}

/* __io_putchar for printf compatibility (called by _write in syscalls.c) */
int __io_putchar(int ch)
{
    while ((USART2->SR & USART_SR_TXE) == 0);
    USART2->DR = (uint8_t)ch;
    return ch;
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
  MX_TIM2_Init();
  MX_USART2_UART_Init();
  /* USER CODE BEGIN 2 */
  /*
   * Start ADC1 in continuous mode (direct register access).
   * MX_ADC1_Init already configured CONT=1. We just enable and trigger.
   */
  ADC1->CR2 |= ADC_CR2_ADON;       // Enable ADC
  for (volatile int i = 0; i < 10000; i++);  // stabilization delay
  ADC1->CR2 |= ADC_CR2_SWSTART;    // Start first conversion (CONT=1 keeps it going)

  HAL_TIM_Base_Start_IT(&htim2);
  HAL_UART_Receive_IT(&huart2, (uint8_t *)&uart_rx_byte, 1);
  uart_send("--- Digital Temp Transmitter ---\r\n");
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
    static uint32_t last_adc = 0;
    if (HAL_GetTick() - last_adc >= 20)  // 25x sim slowdown → ~500ms real
    {
        last_adc = HAL_GetTick();
        temperature = ADC_Read_Temperature();

        int t_int = (int)(temperature * 10 + 0.5f);
        int ah_int = (int)(alarm_high * 10 + 0.5f);
        int al_int = (int)(alarm_low * 10 + 0.5f);
        char buf[64];
        sprintf(buf, "T=%d.%d  H=%d.%d  L=%d.%d  ADC=%u\r\n",
                t_int / 10, t_int % 10,
                ah_int / 10, ah_int % 10,
                al_int / 10, al_int % 10,
                adc_val);
        uart_send(buf);
    }

    uint8_t key = Key_Scan();
    if (key == 1)   // PE0: cycle display mode
    {
        display_mode = (display_mode + 1) % 3;
    }
    else if (key == 2)   // PE1: +0.1
    {
        if (display_mode == 1)
        {
            alarm_high += 0.1f;
            if (alarm_high > 100.0f) alarm_high = 100.0f;
        }
        else if (display_mode == 2)
        {
            alarm_low += 0.1f;
            if (alarm_low > alarm_high) alarm_low = alarm_high;
        }
    }
    else if (key == 3)   // PE2: -0.1
    {
        if (display_mode == 1)
        {
            alarm_high -= 0.1f;
            if (alarm_high < alarm_low) alarm_high = alarm_low;
        }
        else if (display_mode == 2)
        {
            alarm_low -= 0.1f;
            if (alarm_low < 0.0f) alarm_low = 0.0f;
        }
    }

    Alarm_Check();
		HAL_Delay(10);
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/**
 * @brief  Read temperature from ADC (potentiometer simulation)
 * @retval Temperature value in Celsius (0.0 ~ 100.0)
 */
float ADC_Read_Temperature(void)
{
    /*
     * Single conversion via direct register access.
     * ADC enabled once at init (HAL_ADC_Start with CONT=0).
     * Each read: trigger SWSTART, wait EOC, read DR.
     */
    ADC1->CR2 |= ADC_CR2_SWSTART;       // Trigger one conversion

    uint32_t t = 1000000;
    while (!(ADC1->SR & ADC_SR_EOC) && --t);

    adc_val = ADC1->DR;

    float voltage = adc_val * 3.3f / 4096.0f;
    float temp = voltage * 100.0f / 3.3f;
    if (temp < 0.0f)   temp = 0.0f;
    if (temp > 100.0f) temp = 100.0f;
    return temp;
}

/**
 * @brief  Scan key inputs (debounced)
 * @retval 0: no key, 1: Mode, 2: Up, 3: Down
 */
uint8_t Key_Scan(void)
{
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_0) == GPIO_PIN_SET)
    {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_0) == GPIO_PIN_SET)
        {
            while (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_0) == GPIO_PIN_SET);
            return 1; // Mode
        }
    }
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_1) == GPIO_PIN_SET)
    {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_1) == GPIO_PIN_SET)
        {
            while (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_1) == GPIO_PIN_SET);
            return 2; // Up
        }
    }
    if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_SET)
    {
        HAL_Delay(20);
        if (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_SET)
        {
            while (HAL_GPIO_ReadPin(GPIOE, GPIO_PIN_2) == GPIO_PIN_SET);
            return 3; // Down
        }
    }
    return 0;
}

/**
 * @brief  Dynamic display scan - drive one digit at a time
 * @param  pos: digit position 0~3 (left to right)
 */
void Display_Scan(uint8_t pos)
{
    /* Turn off all digits first */
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11, GPIO_PIN_RESET);

    /* Get value to display (scaled by 10) */
    uint16_t disp_val;
    if (display_mode == 0)
        disp_val = (uint16_t)(temperature * 10 + 0.5f);
    else if (display_mode == 1)
        disp_val = (uint16_t)(alarm_high * 10 + 0.5f);
    else
        disp_val = (uint16_t)(alarm_low * 10 + 0.5f);

    uint8_t seg = 0xFF;  // default: all segments off (common-anode)
    uint8_t dp  = 0;

    /*
     * Digit layout (4 digits, pos 0=leftmost .. 3=rightmost):
     *   For disp_val >= 1000:  [hundreds] [tens.] [ones] [tenths]   e.g. 100.0
     *   For 100..999:          [tens]     [ones.] [tenths] [blank]   e.g. 25.6
     *   For 10..99:            [blank]    [ones.] [tenths] [blank]   e.g. 5.6
     *   For 0..9:              [blank]    [0.]    [tenths] [blank]   e.g. 0.5
     */
    switch (pos)
    {
        case 0: /* leftmost */
            if (disp_val >= 1000)
                seg = SEG_CODE[disp_val / 1000];
            else if (disp_val >= 100)
                seg = SEG_CODE[(disp_val / 100) % 10];
            /* else: blank (0xFF) */
            break;

        case 1: /* second digit, always has decimal point */
            if (disp_val >= 1000)
                seg = SEG_CODE[(disp_val / 100) % 10];
            else if (disp_val >= 100)
                seg = SEG_CODE[(disp_val / 10) % 10];
            else if (disp_val >= 10)
                seg = SEG_CODE[disp_val / 10];
            else
                seg = SEG_CODE[0];
            dp = 1;
            break;

        case 2: /* third digit */
            if (disp_val >= 1000)
                seg = SEG_CODE[(disp_val / 10) % 10];
            else
                seg = SEG_CODE[disp_val % 10];
            break;

        case 3: /* rightmost */
            if (disp_val >= 1000)
                seg = SEG_CODE[disp_val % 10];
            /* else: blank (0xFF) */
            break;
    }

    /* Apply decimal point (active low for dp segment = bit 7) */
    if (dp) seg &= 0x7F;

    /* Output segment data to PB0~PB7 */
    GPIOB->ODR = (GPIOB->ODR & 0xFF00) | seg;

    /* Turn on the selected digit */
    switch (pos)
    {
        case 0: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_11, GPIO_PIN_SET); break;
        case 1: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_10, GPIO_PIN_SET); break;
        case 2: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9,  GPIO_PIN_SET); break;
        case 3: HAL_GPIO_WritePin(GPIOA, GPIO_PIN_8,  GPIO_PIN_SET); break;
    }
}

/**
 * @brief  Check temperature against alarm thresholds, update LEDs
 */
void Alarm_Check(void)
{
    /* Upper alarm: Red LED (PC1) — ON when temp exceeds upper limit */
    if (temperature > alarm_high)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_1, GPIO_PIN_RESET);

    /* Lower alarm: Green LED (PC2) — ON when temp drops below lower limit */
    if (temperature < alarm_low)
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_SET);
    else
        HAL_GPIO_WritePin(GPIOC, GPIO_PIN_2, GPIO_PIN_RESET);
}

/**
 * @brief  TIM2 period elapsed callback: scan display + blink yellow LED
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
    if (htim == &htim2)
    {
        /* Dynamic display scan (500Hz / 4 = 125Hz per digit) */
        Display_Scan(digit_index);
        digit_index = (digit_index + 1) % 4;

        /* Yellow LED (PC0): blink ~2Hz when temp is in normal range */
        if (temperature >= alarm_low && temperature <= alarm_high)
        {
            blink_tick++;
            if (blink_tick >= 2)  // 25x sim slowdown → ~5Hz real
            {
                blink_tick = 0;
                HAL_GPIO_TogglePin(GPIOC, GPIO_PIN_0);
            }
        }
        else
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_0, GPIO_PIN_RESET);
            blink_tick = 0;
        }
    }
}

/**
 * @brief  UART receive callback: remote control of alarm threshold
 *         Send 'U' to increase, 'D' to decrease
 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart == &huart2)
    {
        switch (uart_rx_byte)
        {
            case 'U':   // upper alarm +0.1
                alarm_high += 0.1f;
                if (alarm_high > 100.0f) alarm_high = 100.0f;
                break;
            case 'D':   // upper alarm -0.1
                alarm_high -= 0.1f;
                if (alarm_high < alarm_low) alarm_high = alarm_low;
                break;
            case 'u':   // lower alarm +0.1
                alarm_low += 0.1f;
                if (alarm_low > alarm_high) alarm_low = alarm_high;
                break;
            case 'd':   // lower alarm -0.1
                alarm_low -= 0.1f;
                if (alarm_low < 0.0f) alarm_low = 0.0f;
                break;
            default:
                break;
        }

        /* Re-arm receive for next byte */
        HAL_UART_Receive_IT(&huart2, (uint8_t *)&uart_rx_byte, 1);
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
