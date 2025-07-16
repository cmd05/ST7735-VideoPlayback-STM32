#include "fatfs.h"
#include <stm32f4xx_hal.h>
#include <string.h>
#include "sd.h"

void VideoInformation_default_init(VideoInformation* vid_info) {
  vid_info->vid_extension = "BMP";
  vid_info->vid_dir = "/st7735-vid";
  vid_info->num_frames = 0;
}

const char* VideoInformation_get_extension(const char* filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

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

static FRESULT get_vid_info(VideoInformation* vid_info)
{
    FRESULT res;
    DIR dir;
    FILINFO fno;

    res = f_opendir(&dir, vid_info->vid_dir);                   /* Open the directory */
    if (res == FR_OK) {
        for (;;) {
            res = f_readdir(&dir, &fno);           /* Read a directory item */
            if (fno.fname[0] == 0) break;          /* Error or end of dir */
            if (!(fno.fattrib & AM_DIR)) {            /* It is a file */
                myprintf("%10u %s\n", fno.fsize, fno.fname);

                // check extension first time
                if(vid_info->num_frames == 0) {
                    const char* ext = VideoInformation_get_extension(fno.fname);
                    if(!strcasecmp(ext, vid_info->vid_extension)) {
                        myprintf("File extension %s does not match required %s", ext, vid_info->vid_extension);
                        return FR_INVALID_NAME; 
                    }
                }

                vid_info->num_frames++;
            }
        }

        f_closedir(&dir);
        myprintf("%d files.\n", vid_info->num_frames);
    } else {
        myprintf("Failed to open \"%s\". (%u)\n", path, res);
    }

    return res;
}

void SD_get_vid_info() {
  myprintf("\r\n~ SD card Init Info ~\r\n\r\n");

  HAL_Delay(500);

  FATFS FatFs; // FATfs handle
  FIL fil;
  FRESULT fres;
}

void sd_test() {
  myprintf("\r\n~ SD card demo by kiwih ~\r\n\r\n");

  HAL_Delay(500); //a short delay is important to let the SD card settle

  //some variables for FatFs
  FATFS FatFs; 	//Fatfs handle
  FIL fil; 		//File handle
  FRESULT fres; //Result after operations

  //Open the file system
  fres = f_mount(&FatFs, "", 1); //1=mount now
  if (fres != FR_OK) {
    myprintf("f_mount error (%i)\r\n", fres);
   while(1);
  }

  //Let's get some statistics from the SD card
  DWORD free_clusters, free_sectors, total_sectors;

  FATFS* getFreeFs;

  fres = f_getfree("", &free_clusters, &getFreeFs);
  if (fres != FR_OK) {
    myprintf("f_getfree error (%i)\r\n", fres);
    while(1);
  }

  //Formula comes from ChaN's documentation
  total_sectors = (getFreeFs->n_fatent - 2) * getFreeFs->csize;
  free_sectors = free_clusters * getFreeFs->csize;

  myprintf("SD card stats:\r\n%10lu KiB total drive space.\r\n%10lu KiB available.\r\n", total_sectors / 2, free_sectors / 2);

  FRESULT list_res = list_dir("/stm32dir");

  //Now let's try to open file "test.txt"
  fres = f_open(&fil, "test.txt", FA_READ);
  if (fres != FR_OK) {
    myprintf("f_open error (%i)\r\n", fres);
    while(1);
  }
  myprintf("I was able to open 'test.txt' for reading!\r\n");

  //Read 30 bytes from "test.txt" on the SD card
  BYTE readBuf[30];

  //We can either use f_read OR f_gets to get data out of files
  //f_gets is a wrapper on f_read that does some string formatting for us
  TCHAR* rres = f_gets((TCHAR*)readBuf, 30, &fil);
  if(rres != 0) {
	  myprintf("Read string from 'test.txt' contents: %s\r\n", readBuf);
  } else {
	  myprintf("f_gets error (%i)\r\n", fres);
  }

  //Be a tidy kiwi - don't forget to close your file!
  f_close(&fil);

  //Now let's try and write a file "write.txt"
  fres = f_open(&fil, "write.txt", FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
  if(fres == FR_OK) {
	  myprintf("I was able to open 'write.txt' for writing\r\n");
  } else {
	  myprintf("f_open error (%i)\r\n", fres);
  }

  //Copy in a string
  strncpy((char*)readBuf, "a new file is made!", 20);
  UINT bytesWrote;
  fres = f_write(&fil, readBuf, 20, &bytesWrote);
  if(fres == FR_OK) {
	  myprintf("Wrote %i bytes to 'write.txt'!\r\n", bytesWrote);
  } else {
  	myprintf("f_write error (%i)\r\n", fres);
  }

  //Be a tidy kiwi - don't forget to close your file!
  f_close(&fil);

  //We're done, so de-mount the drive
  f_mount(NULL, "", 0);
}