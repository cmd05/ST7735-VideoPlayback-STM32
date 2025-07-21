/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2025 STMicroelectronics.
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
#include "fatfs.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdarg.h>  //for va_list var arg functions
#include <stdio.h>
#include <string.h>

#include "fonts.h"
#include "sd.h"
#include "st7735.h"
// #include "testimg.h"
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
SPI_HandleTypeDef hspi1;
SPI_HandleTypeDef hspi2;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_SPI1_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_SPI2_Init(void);
/* USER CODE BEGIN PFP */
void myprintf(const char *fmt, ...);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
void myprintf(const char *fmt, ...) {
    static char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    int len = strlen(buffer);
    HAL_UART_Transmit(&huart2, (uint8_t *)buffer, len, -1);
}

char g_image_buff[64];

FRESULT draw_image_sd(const char* fname) {
    FIL file;
    FRESULT res = f_open(&file, "/vid/test.txt", FA_READ);
    
    if(res != FR_OK) {
        f_close(&file);
        return -1;
    }

    myprintf("I was able to open the %s for reading!\r\n", "/vid/test.txt");

    //Read 30 bytes from "test.txt" on the SD card
    BYTE readBuf[50];

    //We can either use f_read OR f_gets to get data out of files
    //f_gets is a wrapper on f_read that does some string formatting for us
    TCHAR * rres = f_gets((TCHAR * ) readBuf, sizeof(readBuf), &file);
    if (rres != 0) {
        myprintf("Read string from %s contents: %s\r\n", "/vid/test.txt", readBuf);
    } else {
        myprintf("f_gets error (%i)\r\n", res);
    }

    f_close(&file);
    return FR_OK;

    /// -----------------

    // FIL file;
    // FRESULT res = f_open(&file, fname, FA_READ);
    
    if(res != FR_OK) {
        f_close(&file);
        return -1;
    }

    myprintf("I was able to open the %s for reading!\r\n", fname);

    UINT bytesRead;
    BYTE header[34];

    res = f_read(&file, header, sizeof(header), &bytesRead);
    if(res != FR_OK) {
        f_close(&file);
        return -2;
    }

    if((header[0] != 0x42) || (header[1] != 0x4D)) {
        f_close(&file);
        return -3;
    }

    uint32_t imageOffset = header[10] | (header[11] << 8) | (header[12] << 16) | (header[13] << 24);
    uint32_t imageWidth = header[18] | (header[19] << 8) | (header[20] << 16) | (header[21] << 24);
    uint32_t imageHeight = header[22] | (header[23] << 8) | (header[24] << 16) | (header[25] << 24);
    uint16_t imagePlanes = header[26] | (header[27] << 8);
    uint16_t imageBitsPerPixel = header[28] | (header[29] << 8);
    uint32_t imageCompression = header[30] | (header[31] << 8) | (header[32] << 16) | (header[33] << 24);

    myprintf("Pixels offset: %lu\r\n ", imageOffset);
    myprintf("WxH: %lux%lu\r\n ", imageWidth, imageHeight);
    myprintf("Planes: %d\r\n ", imagePlanes);
    myprintf("Bits per pixel: %d\r\n ", imageBitsPerPixel);
    myprintf("Compression: %d\r\n ", imageCompression);

    if((imageWidth > ST7735_WIDTH) || (imageHeight > ST7735_HEIGHT)) {
        f_close(&file);
        return -4;
    }

    if((imagePlanes != 1) || (imageBitsPerPixel != 24) || (imageCompression != 0)) {
        f_close(&file);
        return -5;
    }

    res = f_lseek(&file, imageOffset);
    if(res != FR_OK) {
        myprintf("f_lseek() failed, res = %d\r\n ", res);
        f_close(&file);
        return -6;
    }

    // row size is aligned to 4 bytes
    uint8_t imageRow[(ST7735_WIDTH * 3 + 3) & ~3];
    for(uint32_t y = 0; y < imageHeight; y++) {
        uint32_t rowIdx = 0;
        res = f_read(&file, imageRow, sizeof(imageRow), &bytesRead);
        if(res != FR_OK) {
            myprintf("f_read() failed, res = %d\r\n", res);
            f_close(&file);
            return -7;
        }

        for(uint32_t x = 0; x < imageWidth; x++) {
            uint8_t b = imageRow[rowIdx++];
            uint8_t g = imageRow[rowIdx++];
            uint8_t r = imageRow[rowIdx++];
            uint16_t color565 = ST7735_COLOR565(r, g, b);
            ST7735_DrawPixel(x, imageHeight - y - 1, color565);
        }
    }

    res = f_close(&file);
    if(res != FR_OK) {
        myprintf("f_close() failed, res = %d\r\n", res);
        return -8;
    }

    return 0;
}

void init() {
    ST7735_Init();

    ST7735_FillScreenFast(ST7735_BLACK);
    HAL_Delay(1000);

    const char ready[] = "Ready\r\n";
    HAL_UART_Transmit(&huart2, (uint8_t *)ready, sizeof(ready) - 1, HAL_MAX_DELAY);

    // SD Test
    VideoInformation vid_info;
    VideoInformation_default_init(&vid_info);

    FRESULT res = SD_get_vid_info(&vid_info);
    if(res != FR_OK) {
        while(1);
    }
}

uint16_t g_colors[] = {ST7735_BLACK,  ST7735_BLUE, ST7735_RED,
                       ST7735_GREEN,  ST7735_CYAN, ST7735_MAGENTA,
                       ST7735_YELLOW, ST7735_WHITE};
int inc = 0;

void loop() {
    // Check border
    // ST7735_FillScreen(ST7735_GREEN);

    // for(int x = 0; x < ST7735_WIDTH; x++) {
    //     ST7735_DrawPixel(x, 0, ST7735_RED);
    //     ST7735_DrawPixel(x, ST7735_HEIGHT-1, ST7735_RED);
    // }

    // for(int y = 0; y < ST7735_HEIGHT; y++) {
    //     ST7735_DrawPixel(0, y, ST7735_RED);
    //     ST7735_DrawPixel(ST7735_WIDTH-1, y, ST7735_RED);
    // }

    // HAL_Delay(3000);

    // sizeof(test_img_128x128);

    // // main test
    // ST7735_DrawImage(0, 0, 128, 128, test_img_128x128);
    // HAL_Delay(1000);

    // ST7735_WriteString(10, 140, "<3 aquila", Font_11x18, ST7735_RED,
    //                    ST7735_BLACK);
    // HAL_Delay(200);

    // draw_image_sd("/vid/test.txt");
    // HAL_Delay(1000);
    // draw_image_sd("/vid/1.txt");
    // HAL_Delay(1000);
    // draw_image_sd("/vid/1.bmp");
    // HAL_Delay(1000);

    // ST7735_FillScreenFast(g_colors[inc]);
    // HAL_Delay(100);
    // inc++;
    // if(inc > 7) inc = 0;
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
  MX_SPI1_Init();
  MX_USART2_UART_Init();
  MX_SPI2_Init();
  MX_FATFS_Init();
  /* USER CODE BEGIN 2 */
    init();

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
    while (1) {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
        // loop();
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

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE2);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 84;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
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
}

/**
  * @brief SPI1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI1_Init(void)
{

  /* USER CODE BEGIN SPI1_Init 0 */

  /* USER CODE END SPI1_Init 0 */

  /* USER CODE BEGIN SPI1_Init 1 */

  /* USER CODE END SPI1_Init 1 */
  /* SPI1 parameter configuration*/
  hspi1.Instance = SPI1;
  hspi1.Init.Mode = SPI_MODE_MASTER;
  hspi1.Init.Direction = SPI_DIRECTION_2LINES;
  hspi1.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi1.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi1.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi1.Init.NSS = SPI_NSS_SOFT;
  hspi1.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_2;
  hspi1.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi1.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi1.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi1.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI1_Init 2 */

  /* USER CODE END SPI1_Init 2 */

}

/**
  * @brief SPI2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_SPI2_Init(void)
{

  /* USER CODE BEGIN SPI2_Init 0 */

  /* USER CODE END SPI2_Init 0 */

  /* USER CODE BEGIN SPI2_Init 1 */

  /* USER CODE END SPI2_Init 1 */
  /* SPI2 parameter configuration*/
  hspi2.Instance = SPI2;
  hspi2.Init.Mode = SPI_MODE_MASTER;
  hspi2.Init.Direction = SPI_DIRECTION_2LINES;
  hspi2.Init.DataSize = SPI_DATASIZE_8BIT;
  hspi2.Init.CLKPolarity = SPI_POLARITY_LOW;
  hspi2.Init.CLKPhase = SPI_PHASE_1EDGE;
  hspi2.Init.NSS = SPI_NSS_SOFT;
  hspi2.Init.BaudRatePrescaler = SPI_BAUDRATEPRESCALER_256;
  hspi2.Init.FirstBit = SPI_FIRSTBIT_MSB;
  hspi2.Init.TIMode = SPI_TIMODE_DISABLE;
  hspi2.Init.CRCCalculation = SPI_CRCCALCULATION_DISABLE;
  hspi2.Init.CRCPolynomial = 10;
  if (HAL_SPI_Init(&hspi2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN SPI2_Init 2 */

  /* USER CODE END SPI2_Init 2 */

}

/**
  * @brief USART2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART2_UART_Init(void)
{

  /* USER CODE BEGIN USART2_Init 0 */

  /* USER CODE END USART2_Init 0 */

  /* USER CODE BEGIN USART2_Init 1 */

  /* USER CODE END USART2_Init 1 */
  huart2.Instance = USART2;
  huart2.Init.BaudRate = 115200;
  huart2.Init.WordLength = UART_WORDLENGTH_8B;
  huart2.Init.StopBits = UART_STOPBITS_1;
  huart2.Init.Parity = UART_PARITY_NONE;
  huart2.Init.Mode = UART_MODE_TX_RX;
  huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart2.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart2) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART2_Init 2 */

  /* USER CODE END USART2_Init 2 */

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
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOB, SD_CS_Pin|GPIO_PIN_6, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOC, GPIO_PIN_7, GPIO_PIN_RESET);

  /*Configure GPIO pin Output Level */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_9, GPIO_PIN_RESET);

  /*Configure GPIO pin : B1_Pin */
  GPIO_InitStruct.Pin = B1_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  HAL_GPIO_Init(B1_GPIO_Port, &GPIO_InitStruct);

  /*Configure GPIO pins : SD_CS_Pin PB6 */
  GPIO_InitStruct.Pin = SD_CS_Pin|GPIO_PIN_6;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

  /*Configure GPIO pin : PC7 */
  GPIO_InitStruct.Pin = GPIO_PIN_7;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

  /*Configure GPIO pin : PA9 */
  GPIO_InitStruct.Pin = GPIO_PIN_9;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_PULLUP;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

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
    /* User can add his own implementation to report the HAL error return state
     */
    __disable_irq();
    while (1) {
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
    /* User can add his own implementation to report the file name and line
       number, ex: printf("Wrong parameters value: file %s on line %d\r\n",
       file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
