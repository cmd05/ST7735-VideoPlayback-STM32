#include "user_spi_callbacks.h"

void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi) {
	if(hspi == &SD_SPI_HANDLE) SD_dma_tx_done = 1;
	else if(hspi == &ST7735_SPI_PORT) ST7735_dma_tx_done = 1;
}

void HAL_SPI_TxRxCpltCallback(SPI_HandleTypeDef *hspi) {
	if(hspi == &SD_SPI_HANDLE) SD_dma_txrx_done = 1;
}