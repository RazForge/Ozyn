"""
Ozayn Camera Overlay — Real camera footage with hand skeleton drawn on top.
Mouse cursor moves when your hand moves. Dead simple.
"""

import os
import numpy as np

from PyQt6.QtWidgets import QWidget
from PyQt6.QtCore import Qt, QTimer, pyqtSignal
from PyQt6.QtGui import QPainter, QColor, QPen, QImage, QPixmap

from ozayn.skin_tracker import SkinHandTracker


class CameraOverlay(QWidget):
    """Small floating window: real camera footage + hand skeleton + cursor control."""

    gesture_command = pyqtSignal(dict)

    _MODELS_DIR = os.path.expanduser("~/.ozayn/models")

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowFlags(
            Qt.WindowType.FramelessWindowHint |
            Qt.WindowType.WindowStaysOnTopHint |
            Qt.WindowType.Tool
        )
        self.setAttribute(Qt.WidgetAttribute.WA_TranslucentBackground)
        self.setFixedSize(280, 220)

        self._camera = None
        self._timer = None
        self._hand_landmarker = None
        self._skin_tracker = SkinHandTracker()
        self._hands_lms = []
        self._pixmap = None
        self._frame_count = 0

    def start(self):
        import cv2
        try:
            import mediapipe as mp
            hand_path = os.path.join(self._MODELS_DIR, "hand_landmarker.task")
            if os.path.exists(hand_path):
                BaseOptions = mp.tasks.BaseOptions
                RunningMode = mp.tasks.vision.RunningMode
                self._hand_landmarker = mp.tasks.vision.HandLandmarker.create_from_options(
                    mp.tasks.vision.HandLandmarkerOptions(
                        base_options=BaseOptions(model_asset_path=hand_path),
                        running_mode=RunningMode.IMAGE,
                        num_hands=2,
                        min_hand_detection_confidence=0.5,
                        min_tracking_confidence=0.5,
                    ))
        except Exception:
            self._hand_landmarker = None

        self._camera = cv2.VideoCapture(0)
        if not self._camera.isOpened():
            return

        self._camera.set(cv2.CAP_PROP_FRAME_WIDTH, 320)
        self._camera.set(cv2.CAP_PROP_FRAME_HEIGHT, 240)
        self._camera.set(cv2.CAP_PROP_FPS, 30)
        self._camera.set(cv2.CAP_PROP_BUFFERSIZE, 1)

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._capture)
        self._timer.start(33)
        self.show()

    def stop(self):
        if self._timer:
            self._timer.stop()
        if self._camera:
            self._camera.release()
            self._camera = None

    def _capture(self):
        import cv2

        if not self._camera or not self._camera.isOpened():
            return

        ret, frame = self._camera.read()
        if not ret:
            return

        self._frame_count += 1

        # Flip horizontally so it feels like a mirror
        frame = cv2.flip(frame, 1)
        h, w = frame.shape[:2]

        # --- Hand detection with MediaPipe ---
        self._hands_lms = []
        if self._hand_landmarker:
            try:
                import mediapipe as mp
                rgb = cv2.cvtColor(frame, cv2.COLOR_BGR2RGB)
                mp_img = mp.Image(image_format=mp.ImageFormat.SRGB, data=rgb)
                result = self._hand_landmarker.detect(mp_img)
                if result.hand_landmarks:
                    self._hands_lms = list(result.hand_landmarks)
            except Exception:
                pass

        # --- Draw hand skeleton on the real footage ---
        overlay = frame.copy()
        skin_result = None
        if self._hands_lms:
            colors = [(0, 255, 200), (255, 180, 0)]
            for i, lms in enumerate(self._hands_lms):
                color = colors[i % len(colors)]
                self._draw_hand(overlay, lms, w, h, color)
        else:
            # Skin tracker fallback — draw contour
            skin_result = self._skin_tracker.detect(frame)
            sx, sy, contour = skin_result
            if contour is not None:
                cv2.drawContours(overlay, [contour], -1, (0, 255, 200), 2)
            else:
                self._skin_tracker.reset()

        # --- Move mouse cursor ---
        cmd = {
            "cursor_x": None, "cursor_y": None,
            "click": False, "right_click": False,
            "drag_start": False, "drag_end": False,
            "scroll_delta": 0, "zoom_delta": 0,
            "gesture": "", "mode": "NORMAL",
        }

        try:
            import pyautogui
            screen_w, screen_h = pyautogui.size()

            if self._hands_lms:
                # Use index fingertip from MediaPipe
                tip = self._hands_lms[0][8]
                cx = int(tip.x * screen_w)
                cy = int(tip.y * screen_h)
                pyautogui.moveTo(cx, cy, _pause=False)
                cmd["cursor_x"] = cx
                cmd["cursor_y"] = cy
                cmd["gesture"] = "HAND"
                cmd["mode"] = "NORMAL"
            else:
                if skin_result is None:
                    skin_result = self._skin_tracker.detect(frame)
                sx, sy, _ = skin_result
                if sx is not None:
                    cx = int(sx * screen_w)
                    cy = int(sy * screen_h)
                    pyautogui.moveTo(cx, cy, _pause=False)
                    cmd["cursor_x"] = cx
                    cmd["cursor_y"] = cy
                    cmd["gesture"] = "SKIN"
                    cmd["mode"] = "NORMAL"

            self.gesture_command.emit(cmd)
        except Exception:
            pass

        # --- Convert to QPixmap for display ---
        rgb = cv2.cvtColor(overlay, cv2.COLOR_BGR2RGB)
        hh, ww, ch = rgb.shape
        bytes_per_line = ch * ww
        qimg = QImage(rgb.data, ww, hh, bytes_per_line, QImage.Format.Format_RGB888)
        self._pixmap = QPixmap.fromImage(qimg)
        self.update()

    def _draw_hand(self, img, lms, w, h, color):
        import cv2
        connections = [
            (0, 1), (1, 2), (2, 3), (3, 4),
            (0, 5), (5, 6), (6, 7), (7, 8),
            (0, 9), (9, 10), (10, 11), (11, 12),
            (0, 13), (13, 14), (14, 15), (15, 16),
            (0, 17), (17, 18), (18, 19), (19, 20),
            (5, 9), (9, 13), (13, 17),
        ]
        for a, b in connections:
            ax, ay = int(lms[a].x * w), int(lms[a].y * h)
            bx, by = int(lms[b].x * w), int(lms[b].y * h)
            cv2.line(img, (ax, ay), (bx, by), (0, 255, 200), 2)
        for i, pt in enumerate(lms):
            px, py = int(pt.x * w), int(pt.y * h)
            r = 4 if i in [4, 8] else 2
            c = (255, 255, 255) if i in [4, 8] else color
            cv2.circle(img, (px, py), r, c, -1)

    def paintEvent(self, event):
        if self._pixmap is None:
            return
        painter = QPainter(self)
        # Scale pixmap to fill the widget
        painter.drawPixmap(0, 0, self._pixmap.scaled(
            self.width(), self.height(),
            Qt.AspectRatioMode.IgnoreAspectRatio,
            Qt.TransformationMode.SmoothTransformation,
        ))
        painter.end()
