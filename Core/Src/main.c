/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2024 STMicroelectronics.
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
#include "ina219.h"
#include "ssd1306.h"
#include <math.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define STATE_CHARGING 1
#define STATE_POWERED 2
#define STATE_IRQ 4

#define LOW_BATTERY 3.65
#define MAX_BATTERY 4.15
#define CHRG_START 4.05
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
I2C_HandleTypeDef hi2c1;

RTC_HandleTypeDef hrtc;

/* USER CODE BEGIN PV */
uint8_t state = 0;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_I2C1_Init(void);
static void MX_RTC_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void set_battery_power(uint8_t enable) {
    HAL_GPIO_WritePin(BatteryPower_GPIO_Port, BatteryPower_Pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void set_battery_charging(uint8_t enable) {
	state = (state & ~STATE_CHARGING) | (enable ? STATE_CHARGING : 0);
	HAL_GPIO_WritePin(BatteryCharging_GPIO_Port, BatteryCharging_Pin, enable ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void set_state_led(uint8_t enable) {
	HAL_GPIO_WritePin(StateLed_GPIO_Port, StateLed_Pin, enable ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

void float_to_string(float num, char *str) {
    float decimal_part = fabs(num);
    int int_part = (int)decimal_part;
    decimal_part -= int_part;

    // Convert integer part to string
    int i = 0;
    while (int_part > 0) {
        str[i++] = '0' + (int_part % 10);
        int_part /= 10;
    }
    if (num < 0) {
        str[i++] = '-';
    }
    str[i] = '\0';

    // Reverse the integer part string
    int j = 0, k = i - 1;
    while (j < k) {
        char temp = str[j];
        str[j] = str[k];
        str[k] = temp;
        j++;
        k--;
    }

    // Append decimal point
    str[i++] = '.';

    // Convert decimal part to string (approximate)
    for (int j = 0; j < 2; j++) {
        decimal_part *= 10;
        int digit = (int)decimal_part;
        str[i++] = '0' + digit;
        decimal_part -= digit;
    }

    str[i] = '\0';
}

void process(void) {
	state |= STATE_IRQ;
    HAL_ResumeTick();

    state = (state & ~STATE_POWERED) | (HAL_GPIO_ReadPin(ExternalPower_GPIO_Port, ExternalPower_Pin) == GPIO_PIN_SET ? STATE_POWERED : 0);
	float voltage = ina219_getBusVoltage();
	float current = ina219_getCurrent() * -1; // ina219 connected in reverse
	set_state_led(0);
	if ((state & STATE_POWERED) == 0) {
		set_battery_charging(0);
		set_battery_power(voltage >= LOW_BATTERY ? 1 : 0);
	} else {
		set_battery_power(0);
		if ((state & STATE_CHARGING) == 0) {
			if (voltage < CHRG_START) {
				set_battery_charging(1);
			}
		} else {
			if (voltage >= MAX_BATTERY) {
				set_battery_charging(0);
				set_state_led(1);
			}
		}
	}
	ssd1306_Fill(Black);
	ssd1306_SetCursor(1, 1);
	ssd1306_WriteString("Battery-X", Font_7x10, White);
	ssd1306_SetCursor(1, 14);
	ssd1306_WriteString("State: ", Font_7x10, White);
	if ((state & STATE_POWERED) == 0) {
		ssd1306_WriteString("discharging", Font_7x10, White);
	} else if ((state & STATE_CHARGING) != 0) {
		ssd1306_WriteString("charging", Font_7x10, White);
	} else {
		ssd1306_WriteString("A/C", Font_7x10, White);
	}
	char num[20];
	ssd1306_SetCursor(1, 25);
	ssd1306_WriteString("Voltage: ", Font_7x10, White);
	float_to_string(voltage, num);
	ssd1306_WriteString(num, Font_7x10, White);
	ssd1306_WriteString("V", Font_7x10, White);
	ssd1306_SetCursor(1, 36);
	ssd1306_WriteString("Current: ", Font_7x10, White);
	float_to_string(current, num);
	ssd1306_WriteString(num, Font_7x10, White);
	ssd1306_WriteString("mA", Font_7x10, White);
    ssd1306_UpdateScreen();

    HAL_SuspendTick();
    HAL_PWR_EnableSEVOnPend();
	__HAL_PWR_CLEAR_FLAG(PWR_FLAG_WU | PWR_FLAG_SB);
	__HAL_RTC_ALARM_CLEAR_FLAG(&hrtc, RTC_FLAG_ALRAF);
	NVIC_ClearPendingIRQ(RTC_IRQn);
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnterSLEEPMode(PWR_MAINREGULATOR_ON, PWR_SLEEPENTRY_WFE);
	state = (state & ~STATE_IRQ);
}

void HAL_RTC_AlarmAEventCallback(RTC_HandleTypeDef *hrtc) {
	RTC_AlarmTypeDef alarm;
	HAL_RTC_GetAlarm(hrtc, &alarm, RTC_ALARM_A, FORMAT_BIN);
	if (alarm.AlarmTime.Seconds > 58) {
		alarm.AlarmTime.Seconds = 0;
	} else {
		alarm.AlarmTime.Seconds = alarm.AlarmTime.Seconds + 1;
	}
	while(HAL_RTC_SetAlarm_IT(hrtc, &alarm, FORMAT_BIN) != HAL_OK);

	if ((state & STATE_IRQ) == 0) {
		process();
	}
}

void HAL_GPIO_EXTI_Callback(uint16_t pin) {
	UNUSED(pin);
//	if (pin != ExternalPower_Pin) {
//		return;
//	}
	if ((state & STATE_IRQ) == 0) {
		process();
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
  MX_I2C1_Init();
  MX_RTC_Init();
  /* USER CODE BEGIN 2 */
  set_state_led(1);
  while (!ina219_Init()) {
	  HAL_Delay(10);
  }
  HAL_Delay(250);
  set_state_led(0);
  HAL_Delay(100);
  ina219_Calibrate();
  set_state_led(1);
  HAL_Delay(100);
  set_state_led(0);
  HAL_Delay(100);
  set_state_led(1);
  HAL_Delay(100);
  set_state_led(0);

  ssd1306_Init();


  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
	  HAL_Delay(1000);
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
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSE;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_I2C1|RCC_PERIPHCLK_RTC;
  PeriphClkInit.I2c1ClockSelection = RCC_I2C1CLKSOURCE_SYSCLK;
  PeriphClkInit.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
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
  hi2c1.Init.Timing = 0x2000090E;
  hi2c1.Init.OwnAddress1 = 0;
  hi2c1.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c1.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c1.Init.OwnAddress2 = 0;
  hi2c1.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c1.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c1.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c1) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c1, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c1, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C1_Init 2 */

  /* USER CODE END I2C1_Init 2 */

}

/**
  * @brief RTC Initialization Function
  * @param None
  * @retval None
  */
static void MX_RTC_Init(void)
{

  /* USER CODE BEGIN RTC_Init 0 */

  /* USER CODE END RTC_Init 0 */

  RTC_TimeTypeDef sTime = {0};
  RTC_DateTypeDef sDate = {0};
  RTC_AlarmTypeDef sAlarm = {0};

  /* USER CODE BEGIN RTC_Init 1 */

  /* USER CODE END RTC_Init 1 */

  /** Initialize RTC Only
  */
  hrtc.Instance = RTC;
  hrtc.Init.HourFormat = RTC_HOURFORMAT_24;
  hrtc.Init.AsynchPrediv = 127;
  hrtc.Init.SynchPrediv = 311;
  hrtc.Init.OutPut = RTC_OUTPUT_DISABLE;
  hrtc.Init.OutPutPolarity = RTC_OUTPUT_POLARITY_HIGH;
  hrtc.Init.OutPutType = RTC_OUTPUT_TYPE_OPENDRAIN;
  if (HAL_RTC_Init(&hrtc) != HAL_OK)
  {
    Error_Handler();
  }

  /* USER CODE BEGIN Check_RTC_BKUP */

  /* USER CODE END Check_RTC_BKUP */

  /** Initialize RTC and set the Time and Date
  */
  sTime.Hours = 0x0;
  sTime.Minutes = 0x0;
  sTime.Seconds = 0x0;
  sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sTime.StoreOperation = RTC_STOREOPERATION_RESET;
  if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  sDate.WeekDay = RTC_WEEKDAY_MONDAY;
  sDate.Month = RTC_MONTH_JANUARY;
  sDate.Date = 0x1;
  sDate.Year = 0x0;

  if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }

  /** Enable the Alarm A
  */
  sAlarm.AlarmTime.Hours = 0x0;
  sAlarm.AlarmTime.Minutes = 0x0;
  sAlarm.AlarmTime.Seconds = 0x4;
  sAlarm.AlarmTime.SubSeconds = 0x0;
  sAlarm.AlarmTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
  sAlarm.AlarmTime.StoreOperation = RTC_STOREOPERATION_RESET;
  sAlarm.AlarmMask = RTC_ALARMMASK_DATEWEEKDAY|RTC_ALARMMASK_HOURS
                              |RTC_ALARMMASK_MINUTES;
  sAlarm.AlarmSubSecondMask = RTC_ALARMSUBSECONDMASK_ALL;
  sAlarm.AlarmDateWeekDaySel = RTC_ALARMDATEWEEKDAYSEL_DATE;
  sAlarm.AlarmDateWeekDay = 0x1;
  sAlarm.Alarm = RTC_ALARM_A;
  if (HAL_RTC_SetAlarm_IT(&hrtc, &sAlarm, RTC_FORMAT_BCD) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN RTC_Init 2 */

  /* USER CODE END RTC_Init 2 */

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
  __HAL_RCC_GPIOF_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, BatteryCharging_Pin|StateLed_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(BatteryPower_GPIO_Port, BatteryPower_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pins : BatteryCharging_Pin StateLed_Pin */
  GPIO_InitStruct.Pin = BatteryCharging_Pin|StateLed_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /*Configure GPIO pin : ExternalPower_Pin */
  GPIO_InitStruct.Pin = ExternalPower_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(ExternalPower_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pin : BatteryPower_Pin */
  GPIO_InitStruct.Pin = BatteryPower_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(BatteryPower_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI0_1_IRQn, 0, 0);
  HAL_NVIC_EnableIRQ(EXTI0_1_IRQn);

/* USER CODE BEGIN MX_GPIO_Init_2 */
/* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */

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

#ifdef  USE_FULL_ASSERT
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
