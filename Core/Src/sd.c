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
            // f_close(&file);
            return FR_INT_ERR;
        }
        
        // check START flag
        if(memcmp(frame_data_arr, START_FLAG_VAL, START_FLAG_LEN) != 0) {
            myprintf("START_FLAG not matching for frame %d. FRAME_FLAG=%.3s\r\n", i, frame_data_arr);
            // f_close(&file);
            return FR_INT_ERR;
        }

        IFLOG MYTIMER_measure_time(MYTIMER_START);

        ST7735_DrawImage(0, 0, vid_width, vid_height, (frame_data_arr + START_FLAG_LEN));

        IFLOG elapsed_time = MYTIMER_measure_time(MYTIMER_END);
        IFLOG myprintf("Frame draw time: %dms\r\n", elapsed_time);
    }

    HAL_Delay(1000);

    free(frame_data_arr);
    f_close(&file);

    return FR_OK;

    // while(1) {
    //     FIL file;
    //     FRESULT res = f_open(&file, "/vid/1.bmp", FA_READ);
        
    //     if(res != FR_OK) {
    //         f_close(&file);
    //         return -1;
    //     }

    //     myprintf("I was able to open the %s for reading!\r\n", "/vid/test.txt");

    //     //Read 30 bytes from "test.txt" on the SD card
    //     BYTE readBuf[50];

    //     //We can either use f_read OR f_gets to get data out of files
    //     //f_gets is a wrapper on f_read that does some string formatting for us
    //     TCHAR * rres = f_gets((TCHAR * ) readBuf, sizeof(readBuf), &file);
    //     if (rres != 0) {
    //         myprintf("Read string from %s contents: %s\r\n", "/vid/test.txt", readBuf);
    //     } else {
    //         myprintf("f_gets error (%i)\r\n", res);
    //     }

    //     f_close(&file);

    //     HAL_Delay(1000);
    // }
    // ---

    // fres = get_vid_info(vid_info);
}

void SD_unmount() {
    f_mount(NULL, "", 0);  // We're done, so de-mount the drive
}


// void sd_test() {
//     myprintf("\r\n~ SD card demo by kiwih ~\r\n\r\n");

//     HAL_Delay(500); //a short delay is important to let the SD card settle

//     //some variables for FatFs
//     FATFS FatFs; //Fatfs handle
//     FIL fil; //File handle
//     FRESULT fres; //Result after operations

//     //Open the file system
//     fres = f_mount( & FatFs, "", 1); //1=mount now
//     if (fres != FR_OK) {
//         myprintf("f_mount error (%i)\r\n", fres);
//         while (1);
//     }

//     //Let's get some statistics from the SD card
//     DWORD free_clusters, free_sectors, total_sectors;

//     FATFS * getFreeFs;

//     fres = f_getfree("", & free_clusters, & getFreeFs);
//     if (fres != FR_OK) {
//         myprintf("f_getfree error (%i)\r\n", fres);
//         while (1);
//     }

//     //Formula comes from ChaN's documentation
//     total_sectors = (getFreeFs->n_fatent - 2) * getFreeFs->csize;
//     free_sectors = free_clusters * getFreeFs->csize;

//     myprintf("SD card stats:\r\n%10lu KiB total drive space.\r\n%10lu KiB available.\r\n", total_sectors / 2, free_sectors / 2);

//     FRESULT list_res = list_dir("/stm32dir");

    // //Now let's try to open file "test.txt"
    // fres = f_open( & fil, "test.txt", FA_READ);
    // if (fres != FR_OK) {
    //     myprintf("f_open error (%i)\r\n", fres);
    //     while (1);
    // }
    // myprintf("I was able to open 'test.txt' for reading!\r\n");

    // //Read 30 bytes from "test.txt" on the SD card
    // BYTE readBuf[30];

    // //We can either use f_read OR f_gets to get data out of files
    // //f_gets is a wrapper on f_read that does some string formatting for us
    // TCHAR * rres = f_gets((TCHAR * ) readBuf, 30, & fil);
    // if (rres != 0) {
    //     myprintf("Read string from 'test.txt' contents: %s\r\n", readBuf);
    // } else {
    //     myprintf("f_gets error (%i)\r\n", fres);
    // }

//     //Be a tidy kiwi - don't forget to close your file!
//     f_close( & fil);

//     //Now let's try and write a file "write.txt"
//     fres = f_open( & fil, "write.txt", FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
//     if (fres == FR_OK) {
//         myprintf("I was able to open 'write.txt' for writing\r\n");
//     } else {
//         myprintf("f_open error (%i)\r\n", fres);
//     }

//     //Copy in a string
//     strncpy((char * ) readBuf, "a new file is made!", 20);
//     UINT bytesWrote;
//     fres = f_write( & fil, readBuf, 20, & bytesWrote);
//     if (fres == FR_OK) {
//         myprintf("Wrote %i bytes to 'write.txt'!\r\n", bytesWrote);
//     } else {
//         myprintf("f_write error (%i)\r\n", fres);
//     }

//     //Be a tidy kiwi - don't forget to close your file!
//     f_close( & fil);

//     //We're done, so de-mount the drive
//     f_mount(NULL, "", 0);
// }

// FRESULT list_dir(const char *path)
// {
//     FRESULT res;
//     DIR dir;
//     FILINFO fno;
//     int nfile, ndir;

//     res = f_opendir(&dir, path);                   /* Open the directory */

//     if (res == FR_OK) {
//         nfile = ndir = 0;
//         for (;;) {
//             res = f_readdir(&dir, &fno);           /* Read a directory item */
//             if (fno.fname[0] == 0) break;          /* Error or end of dir */
//             if (fno.fattrib & AM_DIR) {            /* It is a directory */
//                 myprintf("   <DIR>   %s\n", fno.fname);
//                 ndir++;
//             } else {                               /* It is a file */
//                 myprintf("%10u %s\n", fno.fsize, fno.fname);
//                 nfile++;
//             }
//         }
//         f_closedir(&dir);

//         myprintf("%d dirs, %d files.\n", ndir, nfile);
//     } else {
//         myprintf("Failed to open \"%s\". (%u)\n", path, res);
//     }
//     return res;
// }
