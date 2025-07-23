import re
import os
import glob
import cv2
from concurrent.futures import ProcessPoolExecutor
import argparse
from dataclasses import dataclass

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
    os.system(f"python lvgl-convert.py --ofmt C --cf RGB565_SWAPPED {dir}/{i}.png")
    print(f"Generated: {i}.c")

# n frames: 1.png, 2.png ... n.png
def lvgl_convert_to_c(n, dir):
    with ProcessPoolExecutor() as executor:
        futures = [executor.submit(lvgl_convert_to_c_single, i, dir) for i in range(1, n + 1)]
        for f in futures:
            f.result()  # wait for all to finish

# Format [width upper][width lower][height upper][height lower][num_frames upper][num_frames lower]
# [START flag 'FRM'][data ...][START flag 'FRM'][data ...] ...
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

def vid_to_frames(video_path, output_dir, target_width, target_height, start_time=None, end_time=None):
    os.makedirs(output_dir, exist_ok=True)

    cap = cv2.VideoCapture(video_path)
    if not cap.isOpened():
        print("Error: Cannot open video.")
        exit()

    fps = cap.get(cv2.CAP_PROP_FPS)  
    total_frames = int(cap.get(cv2.CAP_PROP_FRAME_COUNT))  
    duration_sec = total_frames / fps  

    if duration_sec > 60:  # Confirm for long videos
        confirm = input("Converting large video clip. Confirm? [y/N]: ")
        if confirm.strip().lower() != 'y':
            print("Aborted.")
            exit()

    # extract start and end frame numbers
    start_frame = int(start_time * fps) if start_time is not None else 0  
    end_frame = int(end_time * fps) if end_time is not None else total_frames  

    cap.set(cv2.CAP_PROP_POS_FRAMES, start_frame)  

    frame_index = 1
    while cap.get(cv2.CAP_PROP_POS_FRAMES) < end_frame:  # stop based on end_frame
        ret, frame = cap.read()
        if not ret:
            break

        resized = cv2.resize(frame, (target_width, target_height), interpolation=cv2.INTER_AREA)

        out_path = os.path.join(output_dir, f"{frame_index}.png") 
        cv2.imwrite(out_path, resized) # Save as PNG
        frame_index += 1

    cap.release()

    num_frames = frame_index - 1
    print(f"Saved {num_frames} frames to '{output_dir}/'")
    return num_frames

def clear_dirs(folders):
    for folder in folders:
        if not folder.startswith("./"):  # only clear local dirs to the cwd
            continue

        for item in glob.glob(os.path.join(folder, "*")):
            if os.path.basename(item) == ".gitkeep":
                continue
            if os.path.isdir(item):
                os.system(f'rm -rf "{item}"')  # remove directory
            else:
                os.remove(item)  # remove file

        print(f"Cleared {folder} (skipped .gitkeep)")

def time_str_to_seconds(tstr):
    if not tstr:
        return None
    try:
        minutes, seconds = map(int, tstr.split(':'))
        return minutes * 60 + seconds
    except ValueError:
        print("Invalid time format. Use MM:SS")
        exit()

def accept_args():
    parser = argparse.ArgumentParser(description="Convert video to frames.")
    parser.add_argument("video_input", help="Path to input video")
    parser.add_argument("--start", help="Start time MM:SS", default=None)
    parser.add_argument("--end", help="End time MM:SS", default=None)
    
    args = parser.parse_args()
    return args

@dataclass
class Config:
    frames_dir = "./vid_frames"     # intermediate PNG frames directory
    c_frame_dir = "./output"        # intermediate C arrays directory (Fixed dir by LVGL script)
    vid_bin_dir = "./video_output"  # Output directory for video binary
    target_width = 128              # Output video binary width
    target_height = 160             # Output video binary height

def main():
    config = Config()
    args = accept_args()

    start_sec = time_str_to_seconds(args.start)  
    end_sec = time_str_to_seconds(args.end)      

    clear_dirs([config.frames_dir, config.c_frame_dir, config.vid_bin_dir])

    # convert to PNG frames
    n = vid_to_frames(args.video_input, config.frames_dir, config.target_width, config.target_height, start_sec, end_sec)

    # convert to C arrays
    lvgl_convert_to_c(n, config.frames_dir)
    clear_dirs([config.frames_dir])

    # convert to video binary
    c_to_vid_bin(n, config.c_frame_dir, config.vid_bin_dir)
    clear_dirs([config.c_frame_dir])

if __name__ == "__main__":
    main()