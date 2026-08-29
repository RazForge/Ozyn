#!/usr/bin/env python3
"""
Ozayn Hand Tracker — Python subprocess for gesture engine.
Communicates with C parent via binary stdin/stdout protocol.

Protocol:
  C -> Python: 'F' + uint16 width + uint16 height + RGB data (w*h*3 bytes)
  Python -> C: 'N' (no hand, 1 byte)
              'D' + uint8 num_hands + per hand:
                uint8 handedness (0=right, 1=left)
                21 * uint16 x (0-65535)
                21 * uint16 y (0-65535)
                uint8 fingers_up (bit0=thumb, bit1=index, bit2=middle, bit3=ring, bit4=pinky)
"""
import sys
import struct
import numpy as np

from mediapipe.tasks.python import BaseOptions
from mediapipe.tasks.python import vision
import mediapipe as mp

MODEL_PATH = sys.argv[1] if len(sys.argv) > 1 else "lib/hand_landmarker.task"

# Create hand landmarker — detect up to 2 hands
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
FRAME_COUNT = 0

sys.stderr.write("[hand_tracker] Ready (2 hands, 21 landmarks)\n")
sys.stderr.flush()

while True:
    header = sys.stdin.buffer.read(1)
    if not header or header == b'':
        break
    if header != b'F':
        continue

    dim_data = sys.stdin.buffer.read(4)
    if len(dim_data) < 4:
        break
    w, h = struct.unpack('<HH', dim_data)

    rgb_data = sys.stdin.buffer.read(w * h * 3)
    if len(rgb_data) < w * h * 3:
        break

    img_array = np.frombuffer(rgb_data, dtype=np.uint8).reshape((h, w, 3))
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=img_array)

    FRAME_COUNT += 1
    result = landmarker.detect_for_video(mp_image, FRAME_COUNT)

    if not result.hand_landmarks or len(result.hand_landmarks) == 0:
        sys.stdout.buffer.write(b'N')
        sys.stdout.buffer.flush()
        continue

    num_hands = min(len(result.hand_landmarks), 2)
    resp = b'D' + struct.pack('B', num_hands)

    for hi in range(num_hands):
        hand = result.hand_landmarks[hi]

        # Handedness (MediaPipe returns it reversed relative to camera)
        handed = 0  # default right
        if result.handedness and hi < len(result.handedness):
            handed = 0 if result.handedness[hi][0].category_name == "Right" else 1

        resp += struct.pack('B', handed)

        # All 21 landmarks as uint16 pairs
        for lm in hand:
            x = max(0, min(65535, int(lm.x * 65535)))
            y = max(0, min(65535, int(lm.y * 65535)))
            resp += struct.pack('<HH', x, y)

        # Fingers up bitmap
        fingers_up = 0
        # Thumb: tip x < ip x (mirrored camera)
        if hand[4].x < hand[3].x:
            fingers_up |= 0x01
        # Other fingers: tip y < pip y
        for i, tid in enumerate([8, 12, 16, 20], start=1):
            if hand[tid].y < hand[tid - 2].y:
                fingers_up |= (1 << i)

        resp += struct.pack('B', fingers_up)

    sys.stdout.buffer.write(resp)
    sys.stdout.buffer.flush()

landmarker.close()
sys.stderr.write("[hand_tracker] Stopped\n")
sys.stderr.flush()
