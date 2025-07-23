#include "stm32f4xx_hal.h"
#include "st7735.h"
#include "main.h" // for SD_SPI_HANDLE

extern SPI_HandleTypeDef ST7735_SPI_PORT;
extern volatile int ST7735_dma_tx_done;

extern SPI_HandleTypeDef SD_SPI_HANDLE;
extern volatile int SD_dma_tx_done;
extern volatile int SD_dma_txrx_done;

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi);
void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi);