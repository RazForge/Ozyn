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
MODEL_PATH = sys.argv[1] if len(sys.argv) > 1 and sys.argv[1] != "--show" else "lib/hand_landmarker.task"

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

# MediaPipe drawing utils
mp_draw = mp.solutions.drawing_utils
mp_hands = mp.solutions.hands

# Hand landmark connections for drawing
HAND_CONNECTIONS = mp_hands.HAND_CONNECTIONS

sys.stderr.write("[hand_tracker] Ready (2 hands, 21 landmarks)")
if SHOW_MODE:
    sys.stderr.write(" [LIVE FEED]")
sys.stderr.write("\n")
sys.stderr.flush()

if SHOW_MODE:
    import cv2

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

    # Build response
    if not result.hand_landmarks or len(result.hand_landmarks) == 0:
        sys.stdout.buffer.write(b'N')
        sys.stdout.buffer.flush()

        if SHOW_MODE:
            bgr = cv2.cvtColor(img_array, cv2.COLOR_RGB2BGR)
            cv2.putText(bgr, "NO HAND DETECTED", (10, 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 1, (0, 0, 255), 2)
            cv2.putText(bgr, f"Frame: {FRAME_COUNT}", (10, h - 10),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
            cv2.imshow("Ozayn Camera Feed", bgr)
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

    # Live feed display
    if SHOW_MODE:
        bgr = cv2.cvtColor(img_array, cv2.COLOR_RGB2BGR)

        for hi in range(num_hands):
            hand = result.hand_landmarks[hi]

            # Draw connections
            h_list = []
            for lm in hand:
                h_list.append(type('LM', (), {'x': lm.x, 'y': lm.y, 'z': lm.z})())
            fake_hand = type('Hands', (), {'landmark': h_list})()
            mp_draw.draw_landmarks(bgr, fake_hand, HAND_CONNECTIONS)

            # Draw landmark dots with IDs
            for i, lm in enumerate(hand):
                px, py = int(lm.x * w), int(lm.y * h)
                color = (0, 255, 0) if i in TIP_IDS else (200, 200, 200)
                cv2.circle(bgr, (px, py), 4, color, -1)
                cv2.putText(bgr, str(i), (px + 5, py - 5),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.3, (255, 255, 0), 1)

            # Draw fingertip names
            tip_names = ["THUMB", "INDEX", "MIDDLE", "RING", "PINKY"]
            for i, tid in enumerate(TIP_IDS):
                lm = hand[tid]
                px, py = int(lm.x * w), int(lm.y * h)
                cv2.putText(bgr, tip_names[i], (px + 8, py + 3),
                            cv2.FONT_HERSHEY_SIMPLEX, 0.4, (0, 255, 255), 1)

            # Status bar
            handed_str = "RIGHT" if handed == 0 else "LEFT"
            finger_names = []
            if fingers_up & 0x01: finger_names.append("THUMB")
            if fingers_up & 0x02: finger_names.append("INDEX")
            if fingers_up & 0x04: finger_names.append("MIDDLE")
            if fingers_up & 0x08: finger_names.append("RING")
            if fingers_up & 0x10: finger_names.append("PINKY")
            fingers_str = "+".join(finger_names) if finger_names else "NONE"

            # Thumb-index distance
            dx = hand[4].x - hand[8].x
            dy = hand[4].y - hand[8].y
            pinch = (dx*dx + dy*dy) ** 0.5

            status = f"Hand: {handed_str} | Fingers: {fingers_str} | Pinch: {pinch:.3f}"
            cv2.putText(bgr, status, (10, 30 + hi * 30),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.6, (0, 255, 0), 2)

        cv2.putText(bgr, f"Frame: {FRAME_COUNT} | Hands: {num_hands}", (10, h - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
        cv2.imshow("Ozayn Camera Feed", bgr)
        if cv2.waitKey(1) & 0xFF == 27:
            break

landmarker.close()
sys.stderr.write("[hand_tracker] Stopped\n")
sys.stderr.flush()

if SHOW_MODE:
    cv2.destroyAllWindows()
