#pragma once

typedef struct VideoInformation {
    const char* vid_extension; // should be .bmp
    const char* vid_dir;
    int num_frames;
} VideoInformation;

void VideoInformation_default_init(VideoInformation* vid_info);
void VideoInformation_get_extension(const char* filename);
void SD_get_vid_info();

void sd_test();