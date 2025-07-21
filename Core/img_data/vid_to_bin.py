import re
from pathlib import Path
import os
import cv2
from concurrent.futures import ProcessPoolExecutor

def extract_resolution(c_file_path):
    with open(c_file_path, 'r') as f:
        content = f.read()

    # Extract width and height
    width_match = re.search(r'\.w\s*=\s*(\d+)', content)
    height_match = re.search(r'\.h\s*=\s*(\d+)', content)

    if not width_match or not height_match:
        raise ValueError("Width or height not found in the file.")

    width = int(width_match.group(1))
    height = int(height_match.group(1))

    if width > 0xFFFF or height > 0xFFFF:
        raise ValueError("Width/Height exceed uint16 range.")

    return width, height

def extract_c_to_binary(c_file_path):
    with open(c_file_path, 'r') as f:
        content = f.read()

    # Extract array data (first large uint8_t array found)
    array_match = re.search(r'uint8_t\s+\w+\s*\[\s*\]\s*=\s*\{(.*?)\};', content, re.S)
    if not array_match:
        raise ValueError("Array data not found.")

    # Convert to list of integers
    array_raw = array_match.group(1).replace('\n', '').replace('\r', '')
    array_values = [int(x.strip(), 0) for x in array_raw.split(',') if x.strip()]

    # Build binary output
    bin_data = bytearray()
    bin_data.extend(array_values)

    return bin_data

def lvgl_convert_to_c_single(i, dir):
    os.system(f"python lvgl-conv.py --ofmt C --cf RGB565_SWAPPED {dir}/{i}.png")
    print(f"Generated: {i}.c")

# n frames: 1.png, 2.png ... n.png
def lvgl_convert_to_c(n, dir):
    with ProcessPoolExecutor() as executor:
        futures = [executor.submit(lvgl_convert_to_c_single, i, dir) for i in range(1, n + 1)]
        for f in futures:
            f.result()  # wait for all to finish

    # for i in range(1, n + 1):
    #     os.system(f"python lvgl-conv.py --ofmt C --cf RGB565_SWAPPED {dir}/{i}.png")
    #     print(f"Generated: {i}.c")

# Format [width upper][width lower][height upper][height lower][num_frames upper][num_frames lower]
# [START byte][data ...][START byte][data ...] ...
# ---------------------------
# width         - 2 bytes
# height        - 2 bytes
# num_frames    - 2 bytes
# START frame   - 3 bytes (fixed value 'FRM')
# ---------------------------
def c_to_vid_bin(n, input_dir, out_dir):
    out_fname = out_dir + "/video.bin"
    vid_width, vid_height = extract_resolution(input_dir + "/1.c")
    START_BYTE_0 = 0x46
    START_BYTE_1 = 0x52
    START_BYTE_2 = 0x4D

    with open(out_fname, 'wb') as out_file:
        bin_data = bytearray()

        # append video resolution
        bin_data.append((vid_width >> 8) & 0xFF)
        bin_data.append(vid_width & 0xFF)
        bin_data.append((vid_height >> 8) & 0xFF)
        bin_data.append(vid_height & 0xFF)

        # append number of frames
        bin_data.append((n >> 8) & 0xFF)
        bin_data.append(n & 0xFF)

        # append frames data
        # Since pixels are RGB565, there will be double the bytes of resolution
        # Ex: 128*160 = 20480
        # frame_data = 40960 bytes (2 bytes per pixel)
        for i in range(1, n + 1):
            bin_data.append(START_BYTE_0)
            bin_data.append(START_BYTE_1)
            bin_data.append(START_BYTE_2)

            frame_data = extract_c_to_binary(f"{input_dir}/{i}.c")
            print(f"frame_data {i} len={len(frame_data)}")
            bin_data.extend(frame_data)
            
        out_file.write(bin_data)

def vid_to_frames(video_path, output_dir, target_width, target_height):
    # Create output directory if it doesn't exist
    os.makedirs(output_dir, exist_ok=True)

    # Open the video
    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print("Error: Cannot open video.")
        exit()

    frame_index = 1
    while True:
        ret, frame = cap.read()
        if not ret:
            break  # End of video

        resized = cv2.resize(frame, (target_width, target_height), interpolation=cv2.INTER_AREA)

        # Save as PNG
        out_path = os.path.join(output_dir, f"{frame_index}.png")
        cv2.imwrite(out_path, resized)
        frame_index += 1

    cap.release()

    num_frames = frame_index - 1
    print(f"Saved {num_frames} frames to '{output_dir}/'")

    return num_frames

def clear_dirs(folders):
    for folder in folders:
        if not folder.startswith("./"): # only clear local dirs to the cwd
            continue 
        
        print(f"cleared {folder}")
        os.system(f"rm -rf {folder}/*")

def main():
    frames_dir = "./vid_frames"
    c_frame_dir = "./output" # fixed dir by LVGL script
    vid_bin_dir = "./video_output"
    video_input = "./video_input.mp4"
    target_width = 128
    target_height = 160

    clear_dirs([frames_dir, c_frame_dir, vid_bin_dir])

    n = vid_to_frames(video_input, frames_dir, target_width, target_height)

    lvgl_convert_to_c(n, frames_dir)
    c_to_vid_bin(n, c_frame_dir, vid_bin_dir)

    clear_dirs([frames_dir, c_frame_dir])

if __name__ == "__main__":
    main()