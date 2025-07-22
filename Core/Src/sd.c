#include "fatfs.h"
#include <stm32f4xx_hal.h>
#include <string.h>
#include "sd.h"

#include "st7735.h"

#define ENABLE_LOG 1
#define IFLOG if(ENABLE_LOG)

void VideoInformation_default_init(VideoInformation* vid_info) {
    vid_info->vid_extension = "BMP";
    vid_info->vid_dir = "/vid";
    vid_info->num_frames = 0;
}

const char* VideoInformation_get_extension(const char* filename) {
    const char* dot = strrchr(filename, '.');
    if (!dot || dot == filename) return "";
    return dot + 1;
}

static FRESULT get_vid_info(VideoInformation* vid_info) {
    FRESULT res;
    DIR dir;
    FILINFO fno;

    res = f_opendir(&dir, vid_info->vid_dir); /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno); /* Read a directory item */
            if (fno.fname[0] == 0) break; /* Error or end of dir */
            if (!(fno.fattrib & AM_DIR)) {
                /* It is a file */
                myprintf("%10u %s\n", fno.fsize, fno.fname);

                // check extension first time
                // if (vid_info->num_frames == 0) {
                //     const char* ext = VideoInformation_get_extension(fno.fname);
                //     if (strcasecmp(ext, vid_info->vid_extension)) {
                //         myprintf("File extension %s does not match required %s", ext, vid_info->vid_extension);
                //         return FR_INVALID_NAME;
                //     }
                // }

                vid_info->num_frames++;
            }
        }

        f_closedir(&dir);
        myprintf("%d files.\n", vid_info->num_frames);
    } else {
        myprintf("Failed to open \"%s\". (%u)\n", vid_info->vid_dir, res);
    }

    return res;
}

typedef enum MYTIMER_ARG {
    MYTIMER_START,
    MYTIMER_END,
} MYTIMER_ARG;

static uint32_t MYTIMER_measure_time(MYTIMER_ARG arg) {
    static uint32_t start_time = 0;
    static uint32_t end_time = 0;

    if(arg == MYTIMER_START) {
        start_time = HAL_GetTick();
        return 0;
    }

    if(arg == MYTIMER_END) {
        end_time = HAL_GetTick();
        return (end_time - start_time);
    }
}

FRESULT SD_get_vid_info(VideoInformation* vid_info) {
    myprintf("\r\n~ SD card Init Info ~\r\n\r\n");

    HAL_Delay(500);

    FATFS FatFs; // FATFS handle
    FRESULT fres;

    /// open the file system
    fres = f_mount(&FatFs, "", 1); // 1=mount now
    if (fres != FR_OK) {
        myprintf("f_mount error (%i)\r\n", fres);
        return fres;
    }

    /// get statistics from SD card
    DWORD free_clusters, free_sectors, total_sectors;
    FATFS* getFreeFs;

    fres = f_getfree("", &free_clusters, &getFreeFs);
    if (fres != FR_OK) {
        myprintf("f_getfree error (%i)\r\n", fres);
        return fres;
    }

    /// Formula comes from ChaN's documentation
    total_sectors = (getFreeFs->n_fatent - 2) * getFreeFs->csize;
    free_sectors = free_clusters * getFreeFs->csize;
    myprintf("SD card stats:\r\n%10lu KiB total drive space.\r\n%10lu KiB available.\r\n", total_sectors / 2, free_sectors / 2);

    // ---------------------------------------------------------------
    // test reading
    FIL file;
    const char* vid_path = "/vid/video.bin";
    FRESULT res = f_open(&file, vid_path, FA_READ);
    
    if(res != FR_OK) {
        f_close(&file);
        return -1;
    }

    myprintf("I was able to open the %s for reading!\r\n", vid_path);

    // Read resolution
    uint16_t vid_width, vid_height, vid_num_frames;

    BYTE header[6];
    UINT bytes_read;
    // TCHAR* res_status = f_gets((TCHAR *) header, sizeof(header)+1, &file);
    res = f_read(&file, header, sizeof(header), &bytes_read);

    if (res == FR_OK) {
        myprintf("Read header from %s: %s\r\n", vid_path, header);
    } else {
        myprintf("f_gets error (%i)\r\n", res);
    }

    vid_width = (header[0] << 8) | (header[1] & 0xFF);
    vid_height = (header[2] << 8) | (header[3] & 0xFF);
    vid_num_frames = (header[4] << 8) | (header[5] & 0xFF);

    uint8_t* START_FLAG_VAL = "FRM"; // dont compare null terminator
    uint8_t START_FLAG_LEN = 3;

    uint8_t bytes_per_pixel = 2; // RGB565
    uint32_t frame_read_len = (vid_width * vid_height * bytes_per_pixel) + START_FLAG_LEN;
    uint32_t frame_read_num_bytes = frame_read_len * sizeof(uint8_t);

    uint8_t* frame_data_arr = malloc(frame_read_num_bytes);
    
    uint32_t elapsed_time = 0; // time measurements

    for(int i = 0; i < vid_num_frames; i++) {
        IFLOG MYTIMER_measure_time(MYTIMER_START);

        res = f_read(&file, frame_data_arr, frame_read_num_bytes, &bytes_read);
        
        IFLOG elapsed_time = MYTIMER_measure_time(MYTIMER_END);
        IFLOG myprintf("Frame read time: %dms\r\n", elapsed_time);

        if(res != FR_OK) {
            myprintf("Failed to read frame %d\r\n", i);
            break;
        }
        
        // check START flag
        if(memcmp(frame_data_arr, START_FLAG_VAL, START_FLAG_LEN) != 0) {
            myprintf("START_FLAG not matching for frame %d. FRAME_FLAG=%.3s\r\n", i, frame_data_arr);
            break;
        }

        IFLOG MYTIMER_measure_time(MYTIMER_START);

        ST7735_DrawImage(0, 0, vid_width, vid_height, (frame_data_arr + START_FLAG_LEN));

        IFLOG elapsed_time = MYTIMER_measure_time(MYTIMER_END);
        IFLOG myprintf("Frame draw time: %dms\r\n", elapsed_time);
    }

    HAL_Delay(1000);

    free(frame_data_arr);
    f_close(&file);

    return res;
}

void SD_unmount() {
    f_mount(NULL, "", 0);  // We're done, so de-mount the drive
}