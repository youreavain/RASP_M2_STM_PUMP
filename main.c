/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * <h2><center>&copy; Copyright (c) 2024 STMicroelectronics.
  * All rights reserved.</center></h2>
  *
  * This software component is licensed by ST under BSD 3-Clause license,
  * the "License"; You may not use this file except in compliance with the
  * License. You may obtain a copy of the License at:
  *                        opensource.org/licenses/BSD-3-Clause
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "string.h"
#include "stdio.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

#ifndef HSEM_ID_0
#define HSEM_ID_0 (0U) /* HW semaphore 0*/
#endif

#define HTTP_URL "http://87.121.228.83:1888/fhir/Observation/216"
#define HTTP_URL_LEN 46
#define PUMP_BUFF_SIZE 250
#define MODEM_BUFF_SIZE 7000
#define TERMINAL_BUFF_SIZE 300
#define DATA_LEN 119

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

UART_HandleTypeDef huart4;
UART_HandleTypeDef huart1;
UART_HandleTypeDef huart3;

/* USER CODE BEGIN PV */

uint8_t MODEM_BUFF[MODEM_BUFF_SIZE] = {0};
uint8_t PUMP_BUFF[PUMP_BUFF_SIZE] = {0};
uint8_t TERMINAL_BUFF[TERMINAL_BUFF_SIZE] = {0};
uint16_t ModemLen = 0;
uint16_t PumpLen = 0;
uint16_t TermLen = 0;
uint16_t total_len = 0;
uint8_t startFlag = 0;
uint8_t pumpFlag = 0;
uint8_t putCommandFlag = 0;
uint8_t flag = 0;

typedef enum {
    STATE_INIT = 0,
    STATE_CONF_CONTTYPE,
    STATE_NETWORK_INFO,
    STATE_URL_INFO,
    STATE_SEND_URL,
    STATE_GET_HTTP,
    STATE_PROCESS_DATA,
	STATE_PUT,
	STATE_READ,
	STATE_REPEAT
} ModemState;

ModemState state = STATE_INIT;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_UART4_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_USART3_UART_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


	void send_AT_command(UART_HandleTypeDef *huart, const char *command) {
		  HAL_UART_Transmit(huart, (uint8_t*)command, strlen(command), HAL_MAX_DELAY);
		  HAL_Delay(3000);
	}

	void processFlag(void) {
		switch(state) {
			case STATE_INIT:
				send_AT_command(&huart4, "AT\r\n");
				break;
			case STATE_CONF_CONTTYPE:
				send_AT_command(&huart4, "AT+QHTTPCFG=\"contenttype\",4\r\n");
				break;
			case STATE_NETWORK_INFO:
				send_AT_command(&huart4, "AT+QENG=\"servingcell\"\r\n");
				break;
			case STATE_URL_INFO: {
				char http_url_command[64];
				sprintf(http_url_command, "AT+QHTTPURL=%d,5\r\n", HTTP_URL_LEN);
				send_AT_command(&huart4, http_url_command);
				break;
			}
			case STATE_SEND_URL:
				send_AT_command(&huart4, HTTP_URL);
				break;
			case STATE_GET_HTTP:
				send_AT_command(&huart4, "AT+QHTTPGET=5\r\n");
				break;
			case STATE_PROCESS_DATA:
				HAL_UARTEx_ReceiveToIdle_IT(&huart1, PUMP_BUFF, PUMP_BUFF_SIZE);
				break;
			case STATE_READ:
				send_AT_command(&huart4, "AT+QHTTPREAD=120\r\n");
				break;
			default: //if there is no case match, optional
				break;
		}
	}

	void processPumpData(void) {
		uint8_t DataPut[(PumpLen*2)];
		int index1 = 0;
		int index2 = 0;

		  for (index1 = 0; index1 < PumpLen; index1++){

			  DataPut[index2] = ((PUMP_BUFF[index1] >> 4) & 0x0F);
			  if ((DataPut[index2] >= 0x00) && (DataPut[index2] <= 0x09)) {
				  DataPut[index2] = DataPut[index2] + 0x30;
			  }
			  else if ((DataPut[index2] >= 0x0A) && (DataPut[index2] <= 0x0F)) {
				  DataPut[index2] = DataPut[index2] + 0x37;
			  }
				  DataPut[(index2+1)] = (PUMP_BUFF[index1] & 0x0F);

			  if ((DataPut[(index2+1)] >= 0x00) && (DataPut[(index2+1)] <= 0x09)) {
				  DataPut[(index2+1)] = DataPut[(index2+1)] + 0x30;
			  }
			  else if ((DataPut[(index2+1)] >= 0x0A) && (DataPut[(index2+1)] <= 0x0F)) {
				  DataPut[(index2+1)] = DataPut[(index2+1)] + 0x37;
			  }
			  index2 = index2 + 2;
			  }
			HAL_UART_Transmit(&huart4, (uint8_t*)"{\"resourceType\":\"Observation\",\"id\":\"216\",\"subject\":{\"reference\":\"Patient/2\"},\"component\":[{\"valueSampledData\":{\"data\":\"", DATA_LEN, HAL_MAX_DELAY);
			HAL_Delay(100);
			HAL_UART_Transmit(&huart4, DataPut, (PumpLen * 2), HAL_MAX_DELAY);
			HAL_Delay(100);
			HAL_UART_Transmit(&huart4, (uint8_t*)"\"}}]}", 5, HAL_MAX_DELAY);
			HAL_Delay(100);
	}

	void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size) {
		if (huart->Instance == UART4) {
			HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_SET);
			ModemLen = Size;

			if (strstr((char *)MODEM_BUFF, "OK\r\n") && state == STATE_INIT) {
				state = STATE_CONF_CONTTYPE;
			}
			else if (strstr((char *)MODEM_BUFF, "+CME ERROR")) {
				startFlag = 0;
				//HAL_NVIC_SystemReset();
			}
			else if (strstr((char *)MODEM_BUFF, "OK\r\n") && state == STATE_CONF_CONTTYPE) {
				state = STATE_NETWORK_INFO;
			}
			else if (strstr((char *)MODEM_BUFF, "NOCONN") && state == STATE_NETWORK_INFO) {
				state = STATE_URL_INFO;
			}
			else if (strstr((char *)MODEM_BUFF,"CONNECT") && state == STATE_URL_INFO) {
				state = STATE_SEND_URL;
			}
			else if (strstr((char *)MODEM_BUFF, "OK\r\n") && state == STATE_SEND_URL) {
				state = STATE_GET_HTTP;
			}
			else if (strstr((char *)MODEM_BUFF, "QHTTPGET: 0,200")) {
				state = STATE_PROCESS_DATA;
			}
			else if ((strstr((char *)MODEM_BUFF, "CONNECT") != NULL) && state == STATE_REPEAT){
				state = STATE_PROCESS_DATA;
			}
			else if ((strstr((char *)MODEM_BUFF, "QHTTPPUT: 0,200") != NULL)){
				HAL_Delay(500);
				state = STATE_PROCESS_DATA;
			}
			else if ((strstr((char *)MODEM_BUFF, "QHTTPPUT: 702")!= NULL)){
				state = STATE_GET_HTTP;
			}
			else if ((strstr((char *)MODEM_BUFF, "QHTTPPUT: 0,400")!= NULL)){
				state = STATE_READ;
			}

			HAL_UARTEx_ReceiveToIdle_IT(&huart4, MODEM_BUFF, MODEM_BUFF_SIZE);
			HAL_UART_Transmit(&huart3, MODEM_BUFF, ModemLen, HAL_MAX_DELAY);
			memset(MODEM_BUFF,0,ModemLen);
			HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);
		}

		else if (huart->Instance == USART3) {
			TermLen = Size;
			HAL_UART_Transmit(&huart4, TERMINAL_BUFF, TermLen, HAL_MAX_DELAY);
			HAL_UARTEx_ReceiveToIdle_IT(&huart3, TERMINAL_BUFF, TERMINAL_BUFF_SIZE);
		}

		else if (huart->Instance == USART1) {
			PumpLen = Size;
			total_len = DATA_LEN + (PumpLen * 2) + 5;

			if (pumpFlag == 0) {
				char put_command[64];
				sprintf(put_command, "AT+QHTTPPUT=%d,20,20\r\n", total_len);
				HAL_UART_Transmit(&huart4, (uint8_t *)put_command, strlen(put_command), HAL_MAX_DELAY);
				state = STATE_REPEAT;
				HAL_Delay(2800);
				pumpFlag = 1;
				}

			else if (pumpFlag == 1){
				processPumpData();
				pumpFlag = 0;
			}
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
/* USER CODE BEGIN Boot_Mode_Sequence_0 */
  int32_t timeout;
/* USER CODE END Boot_Mode_Sequence_0 */

/* USER CODE BEGIN Boot_Mode_Sequence_1 */
  /* Wait until CPU2 boots and enters in stop mode or timeout*/
  timeout = 0xFFFF;
  while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) != RESET) && (timeout-- > 0));
  if ( timeout < 0 )
  {
  Error_Handler();
  }
/* USER CODE END Boot_Mode_Sequence_1 */
  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();
/* USER CODE BEGIN Boot_Mode_Sequence_2 */
/* When system initialization is finished, Cortex-M7 will release Cortex-M4 by means of
HSEM notification */
/*HW semaphore Clock enable*/
__HAL_RCC_HSEM_CLK_ENABLE();
/*Take HSEM */
HAL_HSEM_FastTake(HSEM_ID_0);
/*Release HSEM in order to notify the CPU2(CM4)*/
HAL_HSEM_Release(HSEM_ID_0,0);
/* wait until CPU2 wakes up from stop mode */
timeout = 0xFFFF;
while((__HAL_RCC_GET_FLAG(RCC_FLAG_D2CKRDY) == RESET) && (timeout-- > 0));
if ( timeout < 0 )
{
Error_Handler();
}
/* USER CODE END Boot_Mode_Sequence_2 */

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_UART4_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  /* USER CODE BEGIN 2 */

  HAL_UARTEx_ReceiveToIdle_IT(&huart4, MODEM_BUFF, 7000);
  HAL_UARTEx_ReceiveToIdle_IT(&huart3, TERMINAL_BUFF, 300);

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  if (HAL_GPIO_ReadPin(GPIOC, GPIO_PIN_13)) {
		  startFlag = 1;
	  }
	  if (startFlag == 1) {
	      processFlag();
	  }

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

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);
  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}
  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_DIV1;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }
  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_HSI;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV4;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV1;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV1;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV1;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_0) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief UART4 Initialization Function
  * @param None
  * @retval None
  */
static void MX_UART4_Init(void)
{

  /* USER CODE BEGIN UART4_Init 0 */

  /* USER CODE END UART4_Init 0 */

  /* USER CODE BEGIN UART4_Init 1 */

  /* USER CODE END UART4_Init 1 */
  huart4.Instance = UART4;
  huart4.Init.BaudRate = 115200;
  huart4.Init.WordLength = UART_WORDLENGTH_8B;
  huart4.Init.StopBits = UART_STOPBITS_1;
  huart4.Init.Parity = UART_PARITY_NONE;
  huart4.Init.Mode = UART_MODE_TX_RX;
  huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart4.Init.OverSampling = UART_OVERSAMPLING_16;
  huart4.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart4.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart4.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart4, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart4, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart4) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN UART4_Init 2 */

  /* USER CODE END UART4_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

}

/**
  * @brief USART3 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART3_UART_Init(void)
{

  /* USER CODE BEGIN USART3_Init 0 */

  /* USER CODE END USART3_Init 0 */

  /* USER CODE BEGIN USART3_Init 1 */

  /* USER CODE END USART3_Init 1 */
  huart3.Instance = USART3;
  huart3.Init.BaudRate = 115200;
  huart3.Init.WordLength = UART_WORDLENGTH_8B;
  huart3.Init.StopBits = UART_STOPBITS_1;
  huart3.Init.Parity = UART_PARITY_NONE;
  huart3.Init.Mode = UART_MODE_TX_RX;
  huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart3.Init.OverSampling = UART_OVERSAMPLING_16;
  huart3.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart3.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart3.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart3, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart3, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart3) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART3_Init 2 */

  /* USER CODE END USART3_Init 2 */

}

/**
  * @brief GPIO Initialization Function
  * @param None
  * @retval None
  */
static void MX_GPIO_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOD_CLK_ENABLE();
  __HAL_RCC_GPIOE_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, LD1_Pin|LD3_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(LD2_GPIO_Port, LD2_Pin, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : LD1_Pin LD3_Pin */
  GPIO_InitStruct.Pin = LD1_Pin|LD3_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : LD2_Pin */
  GPIO_InitStruct.Pin = LD2_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(LD2_GPIO_Port, &GPIO_InitStruct);

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

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
