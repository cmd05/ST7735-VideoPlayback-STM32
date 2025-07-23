#include <stm32f4xx_hal.h>
#include <string.h>

#include "sd_playback.h"
#include "st7735.h"
#include "utils.h"

#define VID_BIN_PATH "/vid/video.bin"
#define ENABLE_LOG   1
#define IFLOG        if(ENABLE_LOG)

typedef enum DebugTimer_Arg {
    DebugTimer_START,
    DebugTimer_END,
} DebugTimer_Arg;

// return measured time in milliseconds (ms)
static uint32_t DebugTimer_MeasureTime(DebugTimer_Arg arg) {
    static uint32_t start_time = 0;
    static uint32_t end_time = 0;

    if(arg == DebugTimer_START) {
        start_time = HAL_GetTick();
    }
    
    if(arg == DebugTimer_END) {
        end_time = HAL_GetTick();
        return (end_time - start_time);
    }

    return 0;
}

FRESULT SDPlayback_Begin() {
    myprintf("\r\n~ SD card Initialize ~\r\n\r\n");

    HAL_Delay(500); // delay before initialization

    FATFS FatFs;
    FRESULT fres;

    // mount the file system
    fres = f_mount(&FatFs, "", 1); // 1 = mount now
    if (fres != FR_OK) {
        myprintf("f_mount error (%i)\r\n", fres);
        return fres;
    }

    // get statistics from SD card
    DWORD free_clusters, free_sectors, total_sectors;
    FATFS* getFreeFs;

    fres = f_getfree("", &free_clusters, &getFreeFs);
    if (fres != FR_OK) {
        myprintf("f_getfree error (%i)\r\n", fres);
        return fres;
    }

    // formula comes from ChaN's documentation
    total_sectors = (getFreeFs->n_fatent - 2) * getFreeFs->csize;
    free_sectors = free_clusters * getFreeFs->csize;
    myprintf("SD card stats:\r\n%10lu KiB total drive space.\r\n%10lu KiB available.\r\n", total_sectors / 2, free_sectors / 2);

    // ------------------- VIDEO PLAYBACK -----------------------------
    FIL file;
    const char* vid_path = VID_BIN_PATH;
    fres = f_open(&file, vid_path, FA_READ);
    
    if(fres != FR_OK) {
        myprintf("Failed to open %s. f_open error (%i)\r\n", vid_path, fres);

        f_close(&file);
        return fres;
    } else {
        myprintf("Opened %s for reading!\r\n", vid_path);
    }

    // Read header
    BYTE header[6];
    UINT bytes_read;
    
    fres = f_read(&file, header, sizeof(header), &bytes_read);

    if (fres == FR_OK) {
        myprintf("Read header from %s.\r\n", vid_path);
    } else {
        myprintf("Failed to read header. f_read error (%d)\r\n", fres);
    }

    uint16_t vid_width, vid_height, vid_num_frames;
    vid_width = (header[0] << 8) | (header[1] & 0xFF);
    vid_height = (header[2] << 8) | (header[3] & 0xFF);
    vid_num_frames = (header[4] << 8) | (header[5] & 0xFF);

    // Framewise read information
    const uint8_t* START_FLAG_VAL = (const uint8_t*) "FRM";
    uint8_t START_FLAG_LEN = 3;

    uint8_t bytes_per_pixel = 2; // RGB565
    uint32_t frame_read_len = (vid_width * vid_height * bytes_per_pixel) + START_FLAG_LEN;
    uint32_t frame_read_num_bytes = frame_read_len * sizeof(uint8_t);

    uint8_t* frame_data_arr = malloc(frame_read_num_bytes);

    uint32_t elapsed_time = 0; // debug: time measurement

    // Read framewise from video
    for(int i = 0; i < vid_num_frames; i++) {
        IFLOG DebugTimer_MeasureTime(DebugTimer_START);

        fres = f_read(&file, frame_data_arr, frame_read_num_bytes, &bytes_read);
        
        IFLOG elapsed_time = DebugTimer_MeasureTime(DebugTimer_END);
        IFLOG myprintf("Frame read time: %dms\r\n", elapsed_time);

        if(fres != FR_OK) {
            myprintf("Failed to read frame %d\r\n. f_read error (%d)", i, fres);
            break;
        }

        // check START flag
        if(memcmp(frame_data_arr, START_FLAG_VAL, START_FLAG_LEN) != 0) {
            myprintf("START_FLAG not matching for frame %d. FRAME_FLAG=%.3s\r\n", i, frame_data_arr);
            break;
        }

        IFLOG DebugTimer_MeasureTime(DebugTimer_START);

        ST7735_DrawImage(0, 0, vid_width, vid_height, (frame_data_arr + START_FLAG_LEN));

        IFLOG elapsed_time = DebugTimer_MeasureTime(DebugTimer_END);
        IFLOG myprintf("Frame draw time: %dms\r\n", elapsed_time);
    }

    HAL_Delay(1000);

    free(frame_data_arr);
    f_close(&file);

    // ----------------------------------------------------------------

    return fres;
}

void SDPlayback_Unmount() {
    f_mount(NULL, "", 0);
}