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
    // ST7735_TEON, 1,
    //     0x01,

    // RGB pixel data order is set by default (IFPF[2:0])
    ST7735_MADCTL, 1,       // Defines read/write scanning direction of frame memory, 1 arg:
        ST7735_ROTATION,    // various parameter bits

    ST7735_COLMOD, 1,       // Set color mode, 1 arg, no delay:
        0x05                // 16 bit color
};

// Since we have set MV for MADCTL; CASET and RASET will be initialized based on it,
// so these commands can be skipped.
// The address window is based on CASET and RASET
// We can specify other values for them as per our requirements
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
    4,                        //  4 commands in list:
    ST7735_GMCTRP1, 16      , //  1: Gamma Adjustments (pos. polarity), 16 args, no delay:
      0x02, 0x1c, 0x07, 0x12,
      0x37, 0x32, 0x29, 0x2d,
      0x29, 0x25, 0x2B, 0x39,
      0x00, 0x01, 0x03, 0x10,
    ST7735_GMCTRN1, 16      , //  2: Gamma Adjustments (neg. polarity), 16 args, no delay:
      0x03, 0x1d, 0x07, 0x06,
      0x2E, 0x2C, 0x29, 0x2D,
      0x2E, 0x2E, 0x37, 0x3F,
      0x00, 0x00, 0x02, 0x10,
    ST7735_NORON, DELAY_MARKER,     //  Normal display on, no args, w/delay
        10,                         //     10 ms delay
    ST7735_DISPON, DELAY_MARKER,    //  Main screen turn on, no args w/delay
        100                         //     100 ms delay
};