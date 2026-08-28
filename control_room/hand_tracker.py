#!/usr/bin/env python3
"""
MediaPipe Hand Tracker — subprocess for Ozayn gesture engine.
Communicates with C parent via binary stdin/stdout protocol.

Protocol:
  C -> Python: 'F' + uint16 width + uint16 height + RGB data (w*h*3 bytes)
  Python -> C: 'H' + 5x(uint16 x, uint16 y) + uint8 fingers_up + uint16 pinch_dist
              'N' (no hand detected, 1 byte)
"""
import sys
import struct
import numpy as np

from mediapipe.tasks.python import BaseOptions
from mediapipe.tasks.python import vision
import mediapipe as mp

MODEL_PATH = sys.argv[1] if len(sys.argv) > 1 else "lib/hand_landmarker.task"

# Create hand landmarker
opts = vision.HandLandmarkerOptions(
    base_options=BaseOptions(model_asset_path=MODEL_PATH),
    running_mode=vision.RunningMode.VIDEO,
    num_hands=1,
    min_hand_detection_confidence=0.6,
    min_hand_presence_confidence=0.6,
    min_tracking_confidence=0.5,
)
landmarker = vision.HandLandmarker.create_from_options(opts)

TIP_IDS = [4, 8, 12, 16, 20]  # thumb, index, middle, ring, pinky
FRAME_COUNT = 0

sys.stderr.write("[hand_tracker] Ready\n")
sys.stderr.flush()

while True:
    # Read header from C
    header = sys.stdin.buffer.read(1)
    if not header or header == b'':
        break

    if header != b'F':
        continue

    # Read dimensions
    dim_data = sys.stdin.buffer.read(4)
    if len(dim_data) < 4:
        break
    w, h = struct.unpack('<HH', dim_data)

    # Read RGB data
    expected = w * h * 3
    rgb_data = sys.stdin.buffer.read(expected)
    if len(rgb_data) < expected:
        break

    # Convert to numpy array
    img_array = np.frombuffer(rgb_data, dtype=np.uint8).reshape((h, w, 3))

    # Create MediaPipe Image
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=img_array)

    # Detect hand
    FRAME_COUNT += 1
    result = landmarker.detect_for_video(mp_image, FRAME_COUNT)

    if not result.hand_landmarks or len(result.hand_landmarks) == 0:
        sys.stdout.buffer.write(b'N')
        sys.stdout.buffer.flush()
        continue

    hand = result.hand_landmarks[0]

    # Extract 5 fingertip positions (normalized 0.0-1.0 -> uint16 0-65535)
    tips = []
    for tid in TIP_IDS:
        lm = hand[tid]
        tips.append(max(0, min(65535, int(lm.x * 65535))))
        tips.append(max(0, min(65535, int(lm.y * 65535))))

    # Determine which fingers are up
    fingers_up = 0
    # Thumb: tip x > ip x (for right hand; mirrored so < for camera)
    if hand[4].x < hand[3].x:
        fingers_up |= 0x01
    # Other 4 fingers: tip y < pip y (y is inverted in image coords)
    for i, tid in enumerate([8, 12, 16, 20], start=1):
        if hand[tid].y < hand[tid - 2].y:
            fingers_up |= (1 << i)

    # Pinch distance: thumb tip (4) to index tip (8)
    dx = hand[4].x - hand[8].x
    dy = hand[4].y - hand[8].y
    pinch_raw = (dx*dx + dy*dy) ** 0.5 * 5.0
    pinch = int(min(1.0, max(0.0, pinch_raw)) * 65535)

    # Send response: 'H' + 10 uint16 + uint8 + uint16
    resp = b'H' + struct.pack('<10H', *tips) + struct.pack('B', fingers_up) + struct.pack('<H', pinch)
    sys.stdout.buffer.write(resp)
    sys.stdout.buffer.flush()

landmarker.close()
sys.stderr.write("[hand_tracker] Stopped\n")
sys.stderr.flush()
