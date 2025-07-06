#pragma once

#include "stm32f4xx_hal.h"
#include "fonts.h"
#include <stdbool.h>
#include <stdint.h>

// Parameter bits for MADCTL command
#define ST7735_MADCTL_MY    0x80
#define ST7735_MADCTL_MX    0x40
#define ST7735_MADCTL_MV    0x20
#define ST7735_MADCTL_ML    0x10
#define ST7735_MADCTL_RGB   0x00
#define ST7735_MADCTL_BGR   0x08
#define ST7735_MADCTL_MH    0x04

// Redefine if necessary
#define ST7735_SPI_PORT hspi1
extern SPI_HandleTypeDef ST7735_SPI_PORT;

// -----------------------------------------------------------------------------

// Color implementation: Use 16 bit / pixel (IFPF[2:0] = 101) (Set using COLMOD command)

// Pin Connections:
//  LED (Backlight) - 3.3V
//  SCK - SPI1 SCK
//  SDA - SPI1 MOSI
//  DC (Data/Command) - GPIO PA9
//  RESET - GPIO PC7
//  CS - GPIO PB6
//  GND - GND
//  VCC - 3.3V
#define ST7735_RES_GPIO_Port    GPIOC
#define ST7735_RES_Pin          GPIO_PIN_7
#define ST7735_CS_GPIO_Port     GPIOB
#define ST7735_CS_Pin           GPIO_PIN_6
#define ST7735_DC_GPIO_Port     GPIOA
#define ST7735_DC_Pin           GPIO_PIN_9

// Display Information:
// Driver IC: ST7735R
// Resolution: 128 * 160 (GM[2:0] = “011”)
// Height: 1.8"
// Plastic overlay: GREENTAB
#define ST7735_IS_160X128 1
#define ST7735_WIDTH      128
#define ST7735_HEIGHT     160
#define ST7735_XSTART     0     // XSTART and YSTART are used only in implementation of ST7735_SetAddressWindow
#define ST7735_YSTART     0
#define ST7735_ROTATION   (0)   // No rotation

// -----------------------------------------------------------------------------

// 1.44" display, default orientation
// #define ST7735_IS_128X128 1
// #define ST7735_WIDTH  128
// #define ST7735_HEIGHT 128
// #define ST7735_XSTART 2
// #define ST7735_YSTART 3
// #define ST7735_ROTATION (ST7735_MADCTL_MX | ST7735_MADCTL_MY | ST7735_MADCTL_BGR)

/* Commands List */
#define ST7735_NOP          0x00
#define ST7735_SWRESET      0x01
#define ST7735_RDDID        0x04
#define ST7735_RDDST        0x09
#define ST7735_RDDPM        0x0A
#define ST7735_RDD_MADCTL   0x0B
#define ST7735_RDD_COLMOD   0x0C
#define ST7735_RDDIM        0x0D
#define ST7735_RDDSM        0x0E
#define ST7735_SLPIN        0x10
#define ST7735_SLPOUT       0x11
#define ST7735_PTLON        0x12
#define ST7735_NORON        0x13
#define ST7735_INVOFF       0x20
#define ST7735_INVON        0x21
#define ST7735_GAMSET       0x26
#define ST7735_DISPOFF      0x28
#define ST7735_DISPON       0x29
#define ST7735_CASET        0x2A
#define ST7735_RASET        0x2B
#define ST7735_RAMWR        0x2C
#define ST7735_RAMRD        0x2E
#define ST7735_PATLR        0x30
#define ST7735_TEOFF        0x34
#define ST7735_TEON         0x35
#define ST7735_MADCTL       0x36
#define ST7735_IDMOFF       0x38
#define ST7735_IDMON        0x39
#define ST7735_COLMOD       0x3A
#define ST7735_RDID1        0xDA
#define ST7735_RDID2        0xDB
#define ST7735_RDID3        0xDC

#define ST7735_FRMCTR1      0xB1
#define ST7735_FRMCTR2      0xB2
#define ST7735_FRMCTR3      0xB3
#define ST7735_INVCTR       0xB4
#define ST7735_DISSET5      0xB6
#define ST7735_PWCTR1       0xC0
#define ST7735_PWCTR2       0xC1
#define ST7735_PWCTR3       0xC2
#define ST7735_PWCTR4       0xC3
#define ST7735_PWCTR5       0xC4
#define ST7735_VMCTR1       0xC5
#define ST7735_VMOFCTR      0xC7
#define ST7735_WRID2        0xD1
#define ST7735_WRID3        0xD2
#define ST7735_PWCTR6       0xFC
#define ST7735_NVCTR1       0xD9
#define ST7735_NVCTR2       0xDE
#define ST7735_NVCTR3       0xDF

#define ST7735_GMCTRP1      0xE0
#define ST7735_GMCTRN1      0xE1

#define ST7735_EXTCTRL      0xF0
#define ST7735_VCOM4L       0xFF

/* Color Definitions */
// 9.8.21. 16bit/pixel format
// RGB 5-6-5-bit input, 65K-Colors (CLUT)
// Bit order: R(5)/G(6)/B(5)
#define ST7735_BLACK    0x0000
#define ST7735_BLUE     0x001F
#define ST7735_RED      0xF800
#define ST7735_GREEN    0x07E0
#define ST7735_CYAN     0x07FF
#define ST7735_MAGENTA  0xF81F
#define ST7735_YELLOW   0xFFE0
#define ST7735_WHITE    0xFFFF

// convert r, g, b as uint8 inputs to single uint16 rgb
// take upper bits of the uint8 value
// (r & 11111000) << 8
// (g & 11111100) << 3
// (b & 11111000) >> 3
#define ST7735_COLOR565(r, g, b) (((r & 0xF8) << 8) | ((g & 0xFC) << 3) | ((b & 0xF8) >> 3))

// GAMSET command curve selection for GS=0
typedef enum {
    GAMMA_10 = 0x01, // 1.0
    GAMMA_25 = 0x02, // 2.5
    GAMMA_22 = 0x04, // 2.2
    GAMMA_18 = 0x08  // 1.8
} GammaDef;

#ifdef __cplusplus
extern "C" {
#endif

// call before initializing any SPI devices
void ST7735_Unselect(void);

void ST7735_Init(void);
void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color);
void ST7735_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font,
                        uint16_t color, uint16_t bgcolor);
void ST7735_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_FillRectangleFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);
void ST7735_FillScreen(uint16_t color);
void ST7735_FillScreenFast(uint16_t color);
void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data);
void ST7735_InvertColors(bool invert);
void ST7735_SetGamma(GammaDef gamma);

#ifdef __cplusplus
}
#endif