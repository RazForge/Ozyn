#!/usr/bin/env python3
"""
Live camera feed — uses the C capture binary to get frames via pipe.
"""
import subprocess
import struct
import numpy as np
import cv2
import sys
import os
import time

IMG_W, IMG_H = 640, 480

print("[LIVE FEED] Starting camera via V4L2 capture...")
print("[LIVE FEED] Press ESC to quit")

# Build and run a tiny C helper that captures YUYV and outputs raw bytes
c_code = r'''
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>

#define W 640
#define H 480

int main(void) {
    int fd = open("/dev/video0", O_RDWR);
    if (fd < 0) { perror("open"); return 1; }

    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = W;
    fmt.fmt.pix.height = H;
    fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
    fmt.fmt.pix.field = V4L2_FIELD_NONE;
    ioctl(fd, VIDIOC_S_FMT, &fmt);

    struct v4l2_requestbuffers req = {0};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    req.memory = V4L2_MEMORY_MMAP;
    ioctl(fd, VIDIOC_REQBUFS, &req);

    struct v4l2_buffer buf = {0};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = 0;
    ioctl(fd, VIDIOC_QUERYBUF, &buf);

    void *mem = mmap(NULL, buf.length, PROT_READ, MAP_SHARED, fd, buf.m.offset);
    int type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    ioctl(fd, VIDIOC_STREAMON, &type);

    /* Output frames as raw YUYV to stdout, one byte 'F' before each frame */
    while (1) {
        ioctl(fd, VIDIOC_QBUF, &buf);
        fd_set fds;
        FD_ZERO(&fds);
        FD_SET(fd, &fds);
        struct timeval tv = {1, 0};
        if (select(fd+1, &fds, NULL, NULL, &tv) <= 0) continue;
        if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) continue;
        putchar('F');
        fflush(stdout);
        fwrite(mem, 1, W * H * 2, stdout);
        fflush(stdout);
    }

    ioctl(fd, VIDIOC_STREAMOFF, &type);
    munmap(mem, buf.length);
    close(fd);
    return 0;
}
'''

# Compile C helper
c_path = "/tmp/ozayn_cap"
with open(c_path + ".c", "w") as f:
    f.write(c_code)

os.system(f"gcc -O2 -o {c_path} {c_path}.c 2>/dev/null")

# Start C capture process
proc = subprocess.Popen([c_path], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)

# Init MediaPipe
from mediapipe.tasks.python import BaseOptions
from mediapipe.tasks.python import vision
from mediapipe.tasks.python.vision import drawing_utils, HandLandmarksConnections
import mediapipe as mp

MODEL_PATH = "lib/hand_landmarker.task"
opts = vision.HandLandmarkerOptions(
    base_options=BaseOptions(model_asset_path=MODEL_PATH),
    running_mode=vision.RunningMode.VIDEO,
    num_hands=2,
    min_hand_detection_confidence=0.5,
    min_hand_presence_confidence=0.5,
    min_tracking_confidence=0.5,
)
landmarker = vision.HandLandmarker.create_from_options(opts)

TIP_IDS = [4, 8, 12, 16, 20]
TIP_NAMES = ["THUMB", "INDEX", "MIDDLE", "RING", "PINKY"]
frame_count = 0

def yuyv_to_bgr(data):
    yuyv = np.frombuffer(data, dtype=np.uint8).reshape((IMG_H, IMG_W, 2))
    # Extract Y, U, V
    y = yuyv[0::, 0::2, 0].astype(np.float32)
    u = yuyv[0::, 0::2, 1].astype(np.float32)
    v = yuyv[1::, 0::2, 1].astype(np.float32)
    # Actually YUYV: Y0 U0 Y1 V0 Y2 U1 Y3 V1
    # U and V are shared between pairs of pixels
    y_full = yuyv[:,:,0].astype(np.float32)
    # Get U and V (they are at odd byte positions for even pixels)
    u_raw = yuyv[:,:,1].astype(np.float32)  # This is wrong for YUYV
    v_raw = yuyv[:,:,3].astype(np.float32)
    # Use OpenCV color conversion
    bgr = cv2.cvtColor(yuyv, cv2.COLOR_YUV2BGR_YUYV)
    return bgr

while True:
    # Read marker byte
    marker = proc.stdout.read(1)
    if not marker or marker != b'F':
        continue

    # Read YUYV frame
    frame_size = IMG_W * IMG_H * 2
    data = b''
    while len(data) < frame_size:
        chunk = proc.stdout.read(frame_size - len(data))
        if not chunk:
            break
        data += chunk

    if len(data) < frame_size:
        continue

    frame_count += 1
    bgr = yuyv_to_bgr(data)

    # MediaPipe
    rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    result = landmarker.detect_for_video(mp_image, frame_count)

    if result.hand_landmarks and len(result.hand_landmarks) > 0:
        num_hands = min(len(result.hand_landmarks), 2)
        for hi in range(num_hands):
            hand = result.hand_landmarks[hi]
            drawing_utils.draw_landmarks(bgr, hand, HandLandmarksConnections.HAND_CONNECTIONS)

            handed = "RIGHT"
            if result.handedness and hi < len(result.handedness):
                handed = result.handedness[hi][0].category_name

            fingers_up = 0
            if hand[4].x < hand[3].x:
                fingers_up |= 0x01
            for i, tid in enumerate([8, 12, 16, 20], start=1):
                if hand[tid].y < hand[tid - 2].y:
                    fingers_up |= (1 << i)

            for i, tid in enumerate(TIP_IDS):
                lm = hand[tid]
                px, py = int(lm.x * IMG_W), int(lm.y * IMG_H)
                cv2.circle(bgr, (px, py), 5, (0, 255, 0), -1)
                cv2.putText(bgr, TIP_NAMES[i], (px + 10, py + 4),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 255, 255), 1)

            dx = hand[4].x - hand[8].x
            dy = hand[4].y - hand[8].y
            pinch = (dx*dx + dy*dy) ** 0.5

            fnames = []
            if fingers_up & 0x01: fnames.append("THUMB")
            if fingers_up & 0x02: fnames.append("INDEX")
            if fingers_up & 0x04: fnames.append("MIDDLE")
            if fingers_up & 0x08: fnames.append("RING")
            if fingers_up & 0x10: fnames.append("PINKY")
            fstr = "+".join(fnames) if fnames else "NONE"

            y0 = 30 + hi * 60
            cv2.putText(bgr, f"Hand: {handed}", (10, y0),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            cv2.putText(bgr, f"Fingers: {fstr}", (10, y0 + 22),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 200, 255), 1)
            cv2.putText(bgr, f"Pinch: {pinch:.3f}", (10, y0 + 42),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)
    else:
        cv2.putText(bgr, "NO HAND DETECTED", (10, 30),
                    cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)

    cv2.putText(bgr, f"Frame: {frame_count}", (10, IMG_H - 10),
                cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
    cv2.rectangle(bgr, (50, 50), (IMG_W - 50, IMG_H - 50), (100, 100, 100), 1)

    cv2.imshow("Ozayn Live Feed", bgr)
    if cv2.waitKey(1) & 0xFF == 27:
        break

proc.terminate()
landmarker.close()
cv2.destroyAllWindows()
print(f"[LIVE FEED] Done — {frame_count} frames")
