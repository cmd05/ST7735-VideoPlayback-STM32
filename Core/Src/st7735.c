#include "st7735.h"
#include "malloc.h"
#include "string.h"

#define USE_DMA
volatile int ST7735_dma_tx_done = 0;

// delay marker has only MSbit set. number_of_args will not use the MSB
// if only DELAY_MARKER, then number_of_args will be zero
#define DELAY_MARKER 0x80

// Based on Adafruit ST7735 library for Arduino (refer to rcmd1[] and rcmd2green[])
// Format
// {
//     num_cmds,
//     cmd, num_args (+ DELAY_MARKER),
//         args...,
//         (delay_period)
//      ...
// }
//
// Ex:
//     ST77XX_COLMOD,  1+ST_CMD_DELAY, //  Set color mode, 1 arg + delay:
//       0x05,                         //     16-bit color
//       10,                           //     10 ms delay
//
static const uint8_t init_cmds1[] = {
    4, // 4 commands in list

    /// "Boot up": (Sleep out, Normal display mode on, Idle mode off)
    // based on 9.14.2. Power Flow Chart
    ST7735_SWRESET,         // Software reset, 0 args, w/delay
        DELAY_MARKER, 150,  // 150ms delay
    ST7735_SLPOUT,          // Out of sleep mode, 0 args, w/delay
        DELAY_MARKER, 255,  // 150ms delay

    /// Set parameters
    // Use default values for FRMCTR, PWCTR, VMCTR

    // RGB pixel data order is set by default (IFPF[2:0])
    ST7735_MADCTL, 1,       // Defines read/write scanning direction of frame memory, 1 arg:
        ST7735_ROTATION,    // various parameter bits

    ST7735_COLMOD, 1,       // Set color mode, 1 arg, no delay:
        0x05                // 16 bit color
};

// Since we have set MV for MADCTL; CASET and RASET will be initialized based on it,
// so these commands can be skipped.

// The address window is based on CASET and RASET
// ST7735_SetAddressWindow() will reset CASET and RASET according to its arguments
static const uint8_t init_cmds2[] = {
    2,                      // 2 commands in list

    ST7735_CASET, 4,        // Column addr set, 4 args, no delay:
        0x00, 0x00,             // XSTART=0
        // (!(ST7735_ROTATION & ST7735_MADCTL_MV) ? 127 : 159)  
        0x00, 0x7F,             // XEND=127,
    ST7735_RASET, 4,        // Row addr set, 4 args, no delay:
        0x00, 0x00,             // XSTART=0
        // (!(ST7735_ROTATION & ST7735_MADCTL_MV) ? 159 : 127)  
        0x00, 0x7F,             // XEND=127,
};

static const uint8_t init_cmds3[] = {
        4,                              //  2 commands in list:
        // Much better colors
        ST7735_GMCTRP1, 16      , //  Gamma Adjustments (pos. polarity), 16 args, no delay:
            0x02, 0x1c, 0x07, 0x12,
            0x37, 0x32, 0x29, 0x2d,
            0x29, 0x25, 0x2B, 0x39,
            0x00, 0x01, 0x03, 0x10,
        ST7735_GMCTRN1, 16      , //  Gamma Adjustments (neg. polarity), 16 args, no delay:
            0x03, 0x1d, 0x07, 0x06,
            0x2E, 0x2C, 0x29, 0x2D,
            0x2E, 0x2E, 0x37, 0x3F,
            0x00, 0x00, 0x02, 0x10,
        ST7735_NORON, DELAY_MARKER,     //  Normal display on, no args, w/delay
            10,                         //     10 ms delay
        ST7735_DISPON, DELAY_MARKER,    //  Main screen turn on, no args w/delay
            100                         //     100 ms delay
};

static void ST7735_Select() {
    // CS line is low, when SPI communication occurs
    HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_RESET);
}

void ST7735_Unselect() {
    HAL_GPIO_WritePin(ST7735_CS_GPIO_Port, ST7735_CS_Pin, GPIO_PIN_SET);
}

static void ST7735_Reset() {
    // Generate a reset sequence
    HAL_GPIO_WritePin(ST7735_RES_GPIO_Port, ST7735_RES_Pin, GPIO_PIN_RESET);
    HAL_Delay(5); // ms
    HAL_GPIO_WritePin(ST7735_RES_GPIO_Port, ST7735_RES_Pin, GPIO_PIN_SET);
}

static void ST7735_WriteCommand(uint8_t cmd) {
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_RESET);
    HAL_SPI_Transmit(&ST7735_SPI_PORT, &cmd, sizeof(cmd), HAL_MAX_DELAY);
}

static void ST7735_WriteData(uint8_t* buff, size_t buff_size) {
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);
#ifdef USE_DMA
	ST7735_dma_tx_done = 0;
    HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, buff, buff_size);
    while(!ST7735_dma_tx_done);
#else
    HAL_SPI_Transmit(&ST7735_SPI_PORT, buff, buff_size, HAL_MAX_DELAY);
#endif
}

static void ST7735_ExecuteCommandList(const uint8_t* cmd_arr) {
    uint8_t num_commands, num_args;
    uint16_t ms;

    num_commands = *cmd_arr++;
    while(num_commands--) {
        uint8_t cmd = *cmd_arr++;
        ST7735_WriteCommand(cmd);

        num_args = *cmd_arr++;

        // delay marker (0x80) has MSbit set. number of args will not use the MSbit
        ms = num_args & DELAY_MARKER;
        num_args &= ~DELAY_MARKER;
        if(num_args) {
            ST7735_WriteData(cmd_arr, num_args);
            cmd_arr += num_args;
        }

        if(ms) {
            ms = *cmd_arr++;
            if(ms == 255) ms = 500;
            HAL_Delay(ms);
        }
    }
}

static void ST7735_SetAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // column address set
    ST7735_WriteCommand(ST7735_CASET);
    uint8_t data_caset[] = {
        ST7735_XSTART + (x0 >> 8), ST7735_XSTART + (x0 & 0xFF), 
        ST7735_XSTART + (x1 >> 8), ST7735_XSTART + (x1 & 0xFF), 
    };
    ST7735_WriteData(data_caset, sizeof(data_caset));
    
    // row address set
    ST7735_WriteCommand(ST7735_RASET);
    uint8_t data_raset[] = {
        ST7735_XSTART + (y0 >> 8), ST7735_XSTART + (y0 & 0xFF), 
        ST7735_XSTART + (y1 >> 8), ST7735_XSTART + (y1 & 0xFF), 
    };
    ST7735_WriteData(data_raset, sizeof(data_raset));

    // write to RAM
    // image data is set generally after setting the address window
    // if no image data is set, the next sent command is directly executed
    ST7735_WriteCommand(ST7735_RAMWR);
}

void ST7735_Init(void) {
    // 9.13 Power ON/OFF Sequence
    ST7735_Select();
    ST7735_Reset();

    ST7735_ExecuteCommandList(init_cmds1);
    // ST7735_ExecuteCommandList(init_cmds2);
    ST7735_ExecuteCommandList(init_cmds3);

    ST7735_Unselect();
}

void ST7735_DrawPixel(uint16_t x, uint16_t y, uint16_t color) {
    if(x >= ST7735_WIDTH || y >= ST7735_HEIGHT)
        return;

    ST7735_Select(); // select is necessary before set address window, since it uses SPI for ST7735_WriteCommand
    ST7735_SetAddressWindow(x, y, x, y);

    uint8_t data[] = { color >> 8, color & 0xFF};
    ST7735_WriteData(data, sizeof(data));

    ST7735_Unselect();
}

static void ST7735_WriteChar(uint16_t x, uint16_t y, char ch, FontDef font, uint16_t color, uint16_t bgcolor) {
    uint32_t i, b, j;

    ST7735_SetAddressWindow(x, y, x+font.width-1, y+font.height-1);

    for(i = 0; i < font.height; i++) {
        b = font.data[(ch - 32) * font.height + i];
        for(j = 0; j < font.width; j++) {
            if((b << j) & 0x8000)  {
                uint8_t data[] = { color >> 8, color & 0xFF };
                ST7735_WriteData(data, sizeof(data));
            } else {
                uint8_t data[] = { bgcolor >> 8, bgcolor & 0xFF };
                ST7735_WriteData(data, sizeof(data));
            }
        }
    }
}

void ST7735_WriteString(uint16_t x, uint16_t y, const char* str, FontDef font, uint16_t color, uint16_t bgcolor) {
    ST7735_Select();

    while(*str) {
        if(x + font.width >= ST7735_WIDTH) {
            x = 0;
            y += font.height; // new line
            
            if(y + font.height >= ST7735_HEIGHT)
                break;
            
            if(*str == ' ') {
                // skip spaces in the beginning of a new line
                str++;
                continue;
            }
        }
        
        ST7735_WriteChar(x, y, *str, font, color, bgcolor);
        x += font.width;
        str++;
    }

    ST7735_Unselect();
}

void ST7735_FillRectangle(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if(x >= ST7735_WIDTH || y >= ST7735_HEIGHT) return;
    if((x + w - 1) >= ST7735_WIDTH) w = ST7735_WIDTH - x;
    if((y + h - 1) >= ST7735_HEIGHT) h = ST7735_HEIGHT - y;

    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x+w-1, y+h-1);

    uint8_t data[] = {color >> 8, color & 0xFF};
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);

    for(int i = 0; i < h; i++) {
        for(int j = 0; j < w; j++) {
#ifdef USE_DMA
            ST7735_dma_tx_done = 0;
            HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, data, sizeof(data));
            while(!ST7735_dma_tx_done);
#else
            HAL_SPI_Transmit(&ST7735_SPI_PORT, data, sizeof(data), HAL_MAX_DELAY);
#endif
        }
    }

    ST7735_Unselect();
}

void ST7735_FillRectangleFast(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if(x >= ST7735_WIDTH || y >= ST7735_HEIGHT) return;
    if((x + w - 1) >= ST7735_WIDTH) w = ST7735_WIDTH - x;
    if((y + h - 1) >= ST7735_HEIGHT) h = ST7735_HEIGHT - y;

    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x+w-1, y+h-1);

    // 128 * 160 * 2 ~ 40kilobytes for entire screen buffer

    // prepare whole line in a single buffer
    uint8_t pixel[] = {color >> 8, color & 0xFF};
    uint16_t line_bufsize = w * sizeof(pixel);
    uint8_t* line = malloc(line_bufsize);

    for(int i = 0; i < w; i++)
        memcpy(line + i * sizeof(pixel), pixel, sizeof(pixel));
    
    HAL_GPIO_WritePin(ST7735_DC_GPIO_Port, ST7735_DC_Pin, GPIO_PIN_SET);

    for(int i = 0; i < h; i++) {
#ifdef USE_DMA
        ST7735_dma_tx_done = 0;
        HAL_SPI_Transmit_DMA(&ST7735_SPI_PORT, line, line_bufsize);
        while(!ST7735_dma_tx_done);
#else
        HAL_SPI_Transmit(&ST7735_SPI_PORT, line, line_bufsize, HAL_MAX_DELAY);
#endif
    }

    free(line);
    ST7735_Unselect();
}

void ST7735_FillScreen(uint16_t color) {
    ST7735_FillRectangle(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void ST7735_FillScreenFast(uint16_t color) {
    ST7735_FillRectangleFast(0, 0, ST7735_WIDTH, ST7735_HEIGHT, color);
}

void ST7735_DrawImage(uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t* data) {
    if(x >= ST7735_WIDTH || y >= ST7735_HEIGHT) return;
    if((x + w - 1) >= ST7735_WIDTH) w = ST7735_WIDTH - x;
    if((y + h - 1) >= ST7735_HEIGHT) h = ST7735_HEIGHT - y;

    ST7735_Select();
    ST7735_SetAddressWindow(x, y, x+w-1, y+h-1);
    ST7735_WriteData((uint8_t*) data, sizeof(uint16_t) * w * h);
    ST7735_Unselect();
}

void ST7735_InvertColors(bool invert) {
    ST7735_WriteCommand(invert ? ST7735_INVON : ST7735_INVOFF);
}

void ST7735_SetGamma(GammaDef gamma) {
    ST7735_Select();
    ST7735_WriteCommand(ST7735_GAMSET);
    ST7735_WriteData((uint8_t*) &gamma, sizeof(uint8_t));
    ST7735_Unselect();
}