#!/usr/bin/env python3
"""
Ozayn Hand Tracker — Python subprocess for gesture engine.
Communicates with C parent via binary stdin/stdout protocol.

Usage:
  hand_tracker.py <model_path> [--show]

  --show: Display live camera feed with hand landmarks overlay

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

SHOW_MODE = "--show" in sys.argv
MODEL_PATH = "lib/hand_landmarker.task"
for arg in sys.argv[1:]:
    if arg != "--show":
        MODEL_PATH = arg

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

sys.stderr.write("[hand_tracker] Ready (2 hands, 21 landmarks)")
if SHOW_MODE:
    sys.stderr.write(" [LIVE FEED]")
sys.stderr.write("\n")
sys.stderr.flush()

if SHOW_MODE:
    import cv2
    from mediapipe.tasks.python.vision import drawing_utils, HandLandmarksConnections

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
        if SHOW_MODE:
            bgr = cv2.cvtColor(img_array, cv2.COLOR_RGB2BGR)
            cv2.putText(bgr, "NO HAND DETECTED", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            cv2.putText(bgr, f"Frame: {FRAME_COUNT}", (10, h - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
            cv2.imshow("Ozayn Live Feed", bgr)
            if cv2.waitKey(1) & 0xFF == 27:
                break
        continue

    num_hands = min(len(result.hand_landmarks), 2)
    resp = b'D' + struct.pack('B', num_hands)

    for hi in range(num_hands):
        hand = result.hand_landmarks[hi]

        handed = 0
        if result.handedness and hi < len(result.handedness):
            handed = 0 if result.handedness[hi][0].category_name == "Right" else 1

        resp += struct.pack('B', handed)

        for lm in hand:
            x = max(0, min(65535, int(lm.x * 65535)))
            y = max(0, min(65535, int(lm.y * 65535)))
            resp += struct.pack('<HH', x, y)

        fingers_up = 0
        if hand[4].x < hand[3].x:
            fingers_up |= 0x01
        for i, tid in enumerate([8, 12, 16, 20], start=1):
            if hand[tid].y < hand[tid - 2].y:
                fingers_up |= (1 << i)

        resp += struct.pack('B', fingers_up)

    sys.stdout.buffer.write(resp)
    sys.stdout.buffer.flush()

    if SHOW_MODE:
        bgr = cv2.cvtColor(img_array, cv2.COLOR_RGB2BGR)
        TIP_NAMES = ["THUMB", "INDEX", "MIDDLE", "RING", "PINKY"]

        for hi in range(num_hands):
            hand = result.hand_landmarks[hi]
            drawing_utils.draw_landmarks(bgr, hand, HandLandmarksConnections.HAND_CONNECTIONS)

            fingers_up = 0
            if hand[4].x < hand[3].x:
                fingers_up |= 0x01
            for i, tid in enumerate([8, 12, 16, 20], start=1):
                if hand[tid].y < hand[tid - 2].y:
                    fingers_up |= (1 << i)

            for i, tid in enumerate(TIP_IDS):
                lm = hand[tid]
                px, py = int(lm.x * w), int(lm.y * h)
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
            handed_str = "RIGHT" if handed == 0 else "LEFT"
            cv2.putText(bgr, f"Hand {hi+1}: {handed_str}", (10, y0),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
            cv2.putText(bgr, f"Fingers: {fstr}", (10, y0 + 22),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 200, 255), 1)
            cv2.putText(bgr, f"Pinch: {pinch:.3f}", (10, y0 + 42),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)

        cv2.putText(bgr, f"Frame: {FRAME_COUNT} | Hands: {num_hands}",
                    (10, h - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
        cv2.rectangle(bgr, (50, 50), (w - 50, h - 50), (100, 100, 100), 1)
        cv2.imshow("Ozayn Live Feed", bgr)
        if cv2.waitKey(1) & 0xFF == 27:
            break

landmarker.close()
sys.stderr.write("[hand_tracker] Stopped\n")
sys.stderr.flush()
if SHOW_MODE:
    cv2.destroyAllWindows()
