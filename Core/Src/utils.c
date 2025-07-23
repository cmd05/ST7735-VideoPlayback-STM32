#include "utils.h"
#include <stdarg.h>  //for va_list var arg functions
#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart2;

void myprintf(const char *fmt, ...) {
    static char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    int len = strlen(buffer);
    HAL_UART_Transmit(&huart2, (uint8_t *)buffer, len, -1);
}