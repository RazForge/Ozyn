#!/usr/bin/env python3
"""Live camera feed with MediaPipe hand landmarks — standalone viewer."""
import numpy as np
import cv2
import mediapipe as mp
from mediapipe.tasks.python import BaseOptions
from mediapipe.tasks.python import vision
from mediapipe.tasks.python.vision import drawing_utils, HandLandmarksConnections

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

cap = cv2.VideoCapture(0)
cap.set(cv2.CAP_PROP_FRAME_WIDTH, 640)
cap.set(cv2.CAP_PROP_FRAME_HEIGHT, 480)

print("[LIVE FEED] Press ESC to quit")
frame_count = 0

while True:
    ret, bgr = cap.read()
    if not ret:
        break

    frame_count += 1
    rgb = cv2.cvtColor(bgr, cv2.COLOR_BGR2RGB)
    mp_image = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
    result = landmarker.detect_for_video(mp_image, frame_count)

    h, w, _ = bgr.shape

    if not result.hand_landmarks or len(result.hand_landmarks) == 0:
        cv2.putText(bgr, "NO HAND DETECTED", (w//2 - 150, h//2),
                    cv2.FONT_HERSHEY_SIMPLEX, 1.2, (0, 0, 255), 3)
        cv2.putText(bgr, f"Frame: {frame_count}", (10, h - 10),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)
        cv2.imshow("Ozayn Live Feed", bgr)
        if cv2.waitKey(1) & 0xFF == 27:
            break
        continue

    for hi in range(min(len(result.hand_landmarks), 2)):
        hand = result.hand_landmarks[hi]

        # Handedness
        handed = "RIGHT"
        if result.handedness and hi < len(result.hand_landmarks):
            handed = result.handedness[hi][0].category_name

        # Draw landmarks and connections
        drawing_utils.draw_landmarks(
            bgr, hand, HandLandmarksConnections.HAND_CONNECTIONS
        )

        # Fingers up
        fingers_up = 0
        if hand[4].x < hand[3].x:
            fingers_up |= 0x01
        for i, tid in enumerate([8, 12, 16, 20], start=1):
            if hand[tid].y < hand[tid - 2].y:
                fingers_up |= (1 << i)

        # Draw fingertip labels
        for i, tid in enumerate(TIP_IDS):
            lm = hand[tid]
            px, py = int(lm.x * w), int(lm.y * h)
            cv2.putText(bgr, TIP_NAMES[i], (px + 10, py + 4),
                        cv2.FONT_HERSHEY_SIMPLEX, 0.45, (0, 255, 255), 1)

        # Pinch distance
        dx = hand[4].x - hand[8].x
        dy = hand[4].y - hand[8].y
        pinch = (dx*dx + dy*dy) ** 0.5

        # Finger list
        fnames = []
        if fingers_up & 0x01: fnames.append("THUMB")
        if fingers_up & 0x02: fnames.append("INDEX")
        if fingers_up & 0x04: fnames.append("MIDDLE")
        if fingers_up & 0x08: fnames.append("RING")
        if fingers_up & 0x10: fnames.append("PINKY")
        fstr = "+".join(fnames) if fnames else "NONE"

        # Info bar
        y0 = 30 + hi * 60
        cv2.putText(bgr, f"Hand {hi+1}: {handed}", (10, y0),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 255, 0), 2)
        cv2.putText(bgr, f"Fingers: {fstr}", (10, y0 + 22),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.55, (0, 200, 255), 1)
        cv2.putText(bgr, f"Pinch: {pinch:.3f}", (10, y0 + 42),
                    cv2.FONT_HERSHEY_SIMPLEX, 0.5, (200, 200, 200), 1)

    cv2.putText(bgr, f"Frame: {frame_count} | Hands: {min(len(result.hand_landmarks), 2)}",
                (10, h - 10), cv2.FONT_HERSHEY_SIMPLEX, 0.6, (255, 255, 255), 1)

    # Frame reduction zone
    cv2.rectangle(bgr, (50, 50), (w - 50, h - 50), (100, 100, 100), 1)

    cv2.imshow("Ozayn Live Feed", bgr)
    if cv2.waitKey(1) & 0xFF == 27:
        break

cap.release()
cv2.destroyAllWindows()
landmarker.close()
print(f"[LIVE FEED] Done — {frame_count} frames")
